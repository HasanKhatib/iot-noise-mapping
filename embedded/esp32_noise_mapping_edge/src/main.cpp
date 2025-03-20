#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>

// WiFi
const char* ssid     = "pixel_hasan";
const char* password = "pixel123*";

// Endpoint
const char* serverUrl = "https://yxmeppy5ty.eu-west-1.awsapprunner.com/upload";
const char* device_id = "RP2040_01";

// Audio settings
#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE 16
#define RECORD_TIME     2
#define ADC_PIN         34
const char* filename  = "/audio.wav";

void recordAudio() {
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[Error] Can't open file for writing");
    return;
  }

  // Basic WAV header placeholder
  uint8_t header[44] = {
    'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ',
    16,0,0,0, 1,0, 1,0, 0x80,0x3E,0x00,0x00, 0x00,0x7D,0x00,0x00,
    2,0, 16,0, 'd','a','t','a', 0,0,0,0
  };
  file.write(header, 44);

  // Record data
  for (int i = 0; i < SAMPLE_RATE * RECORD_TIME; i++) {
    int16_t sample = analogRead(ADC_PIN);
    file.write((uint8_t*)&sample, 2);
    delayMicroseconds(62);
  }

  // Update header
  uint32_t fileSize = file.size();
  uint32_t dataSize = fileSize - 44;
  file.seek(4);  file.write((uint8_t*)&fileSize, 4);
  file.seek(40); file.write((uint8_t*)&dataSize, 4);

  file.close();

  Serial.print("[Info] Recorded WAV size: ");
  Serial.println(fileSize);
}

void uploadFileInOneShot() {
  // Open the WAV file
  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("[Error] Failed to open file for reading");
    return;
  }

  // Read into memory
  String fileData;
  size_t bytesRead = 0;
  while (file.available()) {
    int c = file.read();
    if (c < 0) break;
    fileData += char(c);
    bytesRead++;
  }
  file.close();

  Serial.print("[Debug] read bytes: ");
  Serial.println(bytesRead);

  // Prepare multipart
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  
  String part1 = "--" + boundary + "\r\n"
               + "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
               + "Content-Type: audio/wav\r\n\r\n";

  String part2 = "\r\n--" + boundary + "\r\n"
               + "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n"
               + device_id + "\r\n"
               + "--" + boundary + "--\r\n";

  String fullBody = part1 + fileData + part2;
  size_t contentLength = fullBody.length();

  Serial.print("[Debug] fullBody size: ");
  Serial.println(contentLength);

  // Make request
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, serverUrl)) {
    Serial.println("[Error] http.begin() failed.");
    return;
  }

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Content-Length", String(contentLength));

  Serial.println("[Info] Sending POST...");
  int httpCode = http.POST((uint8_t*)fullBody.c_str(), fullBody.length());
  
  Serial.print("[Info] HTTP code: ");
  Serial.println(httpCode);

  String response = http.getString();
  Serial.println("[Server Response]");
  Serial.println(response);

  http.end();
}

void setup() {
  Serial.begin(9600);
  delay(1000);

  // Initialize SPIFFS once (remove or comment out SPIFFS.format)
  if (!SPIFFS.begin(true)) {
    Serial.println("[Error] SPIFFS.begin() failed!");
    return;
  }

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.println("[Info] Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Info] WiFi connected!");

  recordAudio();
  uploadFileInOneShot();

  // Deep sleep
  esp_sleep_enable_timer_wakeup(10ULL * 1000000ULL);
  Serial.println("[Info] Going to deep sleep...");
  esp_deep_sleep_start();
}

void loop() {}
