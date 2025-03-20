#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SPIFFS.h>
#include "esp_heap_caps.h" // Optional: to check memory if needed

// ----------------------------------------------------------------
// WiFi Credentials
// ----------------------------------------------------------------
const char* ssid     = "pixel_hasan";
const char* password = "pixel123*";

// ----------------------------------------------------------------
// AWS Endpoint
// ----------------------------------------------------------------
const char* host       = "yxmeppy5ty.eu-west-1.awsapprunner.com";
const int   httpsPort  = 443;
const char* uploadPath = "/upload";
const char* device_id  = "edge_01";

// ----------------------------------------------------------------
// Audio Settings
// ----------------------------------------------------------------
#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE 16
#define RECORD_TIME     3
#define ADC_PIN         34
const char* filename   = "/audio.wav";

// ----------------------------------------------------------------
// Helper to see memory usage (optional)
// ----------------------------------------------------------------
void printMemoryStats(const char* label) {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  Serial.printf("[Mem] %s | FreeHeap=%u | LargestBlock=%u\n", label, freeHeap, largestBlock);
}

// ----------------------------------------------------------------
// Re-init Wi-Fi each time (useful after deep sleep)
// ----------------------------------------------------------------
void reinitWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("[Info] Re-initializing WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Info] WiFi connected!");
  printMemoryStats("After WiFi connected");
}

// ----------------------------------------------------------------
// recordAudio(): Records audio to SPIFFS as a WAV file
// ----------------------------------------------------------------
void recordAudio() {
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[Error] Cannot open file for writing.");
    return;
  }

  // 1) Write minimal 44-byte WAV header (placeholder)
  uint8_t header[44] = {
    'R','I','F','F', 0,0,0,0,
    'W','A','V','E',
    'f','m','t',' ', 16,0,0,0,
    1,0, 1,0,
    0x80,0x3E,0x00,0x00, // 16000 sample rate
    0x00,0x7D,0x00,0x00, // Byte rate = 16000 * 2
    2,0,
    16,0,
    'd','a','t','a', 0,0,0,0
  };
  file.write(header, 44);

  // 2) Record audio samples
  for (int i = 0; i < SAMPLE_RATE * RECORD_TIME; i++) {
    int16_t sample = analogRead(ADC_PIN);
    file.write((uint8_t*)&sample, 2);
    delayMicroseconds(62); // ~1/16000 second
  }

  // 3) Update the WAV header sizes
  uint32_t fileSize = file.size();
  uint32_t dataSize = fileSize - 44;
  file.seek(4);  file.write((uint8_t*)&fileSize, 4);
  file.seek(40); file.write((uint8_t*)&dataSize, 4);

  file.close();
  Serial.print("[Info] Recorded WAV size: ");
  Serial.println(fileSize);
}

// ----------------------------------------------------------------
// sendChunk() - Utility to do chunked transfer
// ----------------------------------------------------------------
void sendChunk(WiFiClientSecure &client, const uint8_t* data, size_t len) {
  // Print chunk size in hex, followed by CRLF
  char chunkSize[16];
  sprintf(chunkSize, "%X\r\n", (unsigned int)len);
  client.print(chunkSize);

  // Write data
  client.write(data, len);

  // End of chunk with CRLF
  client.print("\r\n");
}

// Overload for sending a String as a chunk
void sendChunk(WiFiClientSecure &client, const String &str) {
  sendChunk(client, (const uint8_t*)str.c_str(), str.length());
}

// ----------------------------------------------------------------
// uploadFileStream(): streams the file in chunks (no big String!)
// ----------------------------------------------------------------
void uploadFileStream() {
  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("[Error] Cannot open WAV file for uploading.");
    return;
  }
  
  // 1) Connect to the server
  WiFiClientSecure client;
  client.setInsecure(); // for test; ideally load real CA cert
  printMemoryStats("Before connect()");

  Serial.print("[Info] Connecting to ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(httpsPort);
  if (!client.connect(host, httpsPort)) {
    Serial.println("[Error] Connection failed!");
    file.close();
    return;
  }
  Serial.println("[Info] Connected via TLS!");
  printMemoryStats("After connect()");

  // 2) Send the HTTP request line and basic headers
  //    We'll use chunked encoding so we don't need Content-Length
  String boundary = "----Esp32Boundary12345";
  client.print("POST ");
  client.print(uploadPath);
  client.print(" HTTP/1.1\r\n");
  client.print("Host: ");
  client.print(host);
  client.print("\r\n");
  client.print("Content-Type: multipart/form-data; boundary=");
  client.print(boundary);
  client.print("\r\n");
  client.print("Transfer-Encoding: chunked\r\n");
  client.print("Connection: close\r\n\r\n");

  // 3) Send the file part header as one chunk
  String fileHeader = "--" + boundary + "\r\n";
  fileHeader += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  fileHeader += "Content-Type: audio/wav\r\n\r\n";
  sendChunk(client, fileHeader);

  // 4) Send the file data in small chunks
  const size_t CHUNK_SIZE = 1024;
  uint8_t buffer[CHUNK_SIZE];
  while (true) {
    size_t bytesRead = file.read(buffer, CHUNK_SIZE);
    if (bytesRead == 0) {
      // End of file
      break;
    }
    sendChunk(client, buffer, bytesRead);
  }
  file.close();

  // 5) End the file part with CRLF, then start the device_id field
  String devIDHeader = "\r\n--" + boundary + "\r\n";
  devIDHeader += "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n";
  devIDHeader += String(device_id) + "\r\n";
  sendChunk(client, devIDHeader);

  // 6) Final boundary
  String ending = "--" + boundary + "--\r\n";
  sendChunk(client, ending);

  // 7) Indicate the end of chunks
  client.print("0\r\n\r\n");

  // 8) Read the server's response
  Serial.println("[Info] Waiting for server response...");
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }
  client.stop();
  Serial.println("[Info] Upload complete, connection closed.");
}

// ----------------------------------------------------------------
// setup()
// ----------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  delay(1000);

  // 1) Initialize SPIFFS (no format on each boot, just mount)
  if (!SPIFFS.begin(true)) {
    Serial.println("[Error] SPIFFS.begin() failed!");
    return;
  }

  // 2) Re-init WiFi
  reinitWiFi();

  // 3) Record the WAV
  recordAudio();

  // 4) Upload using streaming approach
  uploadFileStream();

  // 5) Deep sleep for 10 seconds
  esp_sleep_enable_timer_wakeup(10ULL * 1000000ULL);
  Serial.println("[Info] Going to deep sleep...");
  esp_deep_sleep_start();
}

void loop() {
  // Not used, deep-sleep approach
}
