#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "esp_heap_caps.h" // Optional: to check memory if needed

// ----------------------------------------------------------------
// WiFi Credentials
// ----------------------------------------------------------------
const char* ssid     = "HaWa.2g";
const char* password = "Gaza.2215";

// ----------------------------------------------------------------
// Server Endpoint
// ----------------------------------------------------------------
const char* host       = "192.168.1.228";
const int   httpPort   = 8080;            // HTTP port for local server
const char* uploadPath = "/upload";
const char* device_id  = "edge_01";

// ----------------------------------------------------------------
// GPS Coordinates (update with your actual location)
// ----------------------------------------------------------------
const char* latitude   = "55.616158";
const char* longitude  = "12.978885";

// ----------------------------------------------------------------
// Audio Settings - MAX9814 Microphone
// ----------------------------------------------------------------
#define SAMPLE_RATE     8000
#define BITS_PER_SAMPLE 16
#define RECORD_TIME     3
#define ADC_PIN         34        // MAX9814 OUT pin connected here
#define ADC_RESOLUTION  12        // ESP32 ADC is 12-bit (0-4095)
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
// recordAudio(): Records audio from MAX9814 to SPIFFS as WAV
// ----------------------------------------------------------------
void recordAudio() {
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[Error] Cannot open file for writing.");
    return;
  }

  // Configure ADC for better audio quality
  analogReadResolution(ADC_RESOLUTION);
  analogSetAttenuation(ADC_11db);  // Full range 0-3.3V for MAX9814 output

  Serial.println("[Info] Starting audio recording from MAX9814...");

  // 1) Write minimal 44-byte WAV header (placeholder)
  uint8_t header[44] = {
    'R','I','F','F', 0,0,0,0,
    'W','A','V','E',
    'f','m','t',' ', 16,0,0,0,
    1,0, 1,0,
    0x40,0x1F,0x00,0x00, // 8000 sample rate (0x1F40 = 8000)
    0x80,0x3E,0x00,0x00, // Byte rate = 8000 * 2 (0x3E80 = 16000)
    2,0,
    16,0,
    'd','a','t','a', 0,0,0,0
  };
  file.write(header, 44);

  // 2) Record audio samples from MAX9814
  uint32_t samplesRecorded = 0;
  uint32_t totalSamples = SAMPLE_RATE * RECORD_TIME;
  uint32_t sampleIntervalUs = 1000000 / SAMPLE_RATE; // 125µs for 8kHz
  
  Serial.printf("[Info] Sample interval: %u microseconds\n", sampleIntervalUs);
  
  unsigned long startTime = micros();
  unsigned long lastSampleTime = startTime;
  
  for (uint32_t i = 0; i < totalSamples; i++) {
    // Calculate exact time when this sample should be taken
    unsigned long targetTime = startTime + (i * sampleIntervalUs);
    
    // Wait until it's time for the next sample
    while (micros() < targetTime) {
      // Busy wait for precise timing
    }
    
    // Read 12-bit ADC value (0-4095) and scale to 16-bit signed (-32768 to 32767)
    uint16_t adcValue = analogRead(ADC_PIN);
    int16_t sample = (adcValue - 2048) * 16;  // Center around 0 and scale up
    
    file.write((uint8_t*)&sample, 2);
    samplesRecorded++;
    lastSampleTime = micros();
    
    // Progress indicator every second
    if (samplesRecorded % SAMPLE_RATE == 0) {
      Serial.print(".");
    }
  }
  
  unsigned long totalTime = micros() - startTime;
  float actualSampleRate = (float)samplesRecorded / (totalTime / 1000000.0);
  Serial.printf("\n[Info] Actual sample rate: %.2f Hz (target: %d Hz)\n", actualSampleRate, SAMPLE_RATE);
  Serial.println();

  // 3) Update the WAV header sizes
  uint32_t fileSize = file.size();
  uint32_t dataSize = fileSize - 44;
  file.seek(4);  file.write((uint8_t*)&fileSize, 4);
  file.seek(40); file.write((uint8_t*)&dataSize, 4);

  file.close();
  Serial.print("[Info] Recorded WAV size: ");
  Serial.print(fileSize);
  Serial.print(" bytes (");
  Serial.print(samplesRecorded);
  Serial.println(" samples)");
}

// ----------------------------------------------------------------
// sendChunk() - Utility to do chunked transfer
// ----------------------------------------------------------------
void sendChunk(WiFiClient &client, const uint8_t* data, size_t len) {
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
void sendChunk(WiFiClient &client, const String &str) {
  sendChunk(client, (const uint8_t*)str.c_str(), str.length());
}

// ----------------------------------------------------------------
// uploadFileStream(): streams the file in chunks (no big String!)
// ----------------------------------------------------------------
/**
 * @brief Uploads a WAV file to a remote server using HTTP chunked transfer encoding.
 * 
 * This function reads a WAV audio file from SPIFFS and uploads it to a configured
 * HTTP server using multipart/form-data encoding with chunked transfer. The upload
 * includes the audio file along with metadata fields (device_id, latitude, longitude).
 * 
 * The function performs the following steps:
 * 1. Opens the WAV file from SPIFFS for reading
 * 2. Establishes a TCP connection to the configured HTTP server
 * 3. Sends HTTP POST headers with chunked transfer encoding
 * 4. Constructs and sends multipart/form-data boundary headers
 * 5. Streams the file content in 1KB chunks to minimize memory usage
 * 6. Appends form fields for device_id, latitude, and longitude
 * 7. Closes the multipart boundary
 * 8. Reads and prints the server's HTTP response
 * 9. Closes the connection
 * 
 * @note Uses global variables: filename, host, httpPort, uploadPath, device_id, latitude, longitude
 * @note Requires an active WiFi connection before calling
 * @note Prints debug information and memory statistics to Serial
 * @note Uses chunked encoding to avoid buffering entire file in memory
 * 
 * @see sendChunk() Helper function for sending individual chunks
 * @see printMemoryStats() Helper function for memory diagnostics
 * 
 * @return void (prints error messages to Serial on failure)
 */
void uploadFileStream() {
  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("[Error] Cannot open WAV file for uploading.");
    return;
  }
  
  // 1) Connect to the server
  WiFiClient client;
  printMemoryStats("Before connect()");

  Serial.print("[Info] Connecting to ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(httpPort);
  if (!client.connect(host, httpPort)) {
    Serial.println("[Error] Connection failed!");
    file.close();
    return;
  }
  Serial.println("[Info] Connected via HTTP!");
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

  // 6) Send latitude field
  String latHeader = "--" + boundary + "\r\n";
  latHeader += "Content-Disposition: form-data; name=\"latitude\"\r\n\r\n";
  latHeader += String(latitude) + "\r\n";
  sendChunk(client, latHeader);

  // 7) Send longitude field
  String lonHeader = "--" + boundary + "\r\n";
  lonHeader += "Content-Disposition: form-data; name=\"longitude\"\r\n\r\n";
  lonHeader += String(longitude) + "\r\n";
  sendChunk(client, lonHeader);

  // 8) Final boundary
  String ending = "--" + boundary + "--\r\n";
  sendChunk(client, ending);

  // 9) Indicate the end of chunks
  client.print("0\r\n\r\n");

  // 10) Read the server's response
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
