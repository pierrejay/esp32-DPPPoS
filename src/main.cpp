#include <Arduino.h>
#include "DPPPoS.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <time.h>

#define USB Serial
#define UART Serial1

#define UART_TX_PIN D7
#define UART_RX_PIN D6

#define LOG_INTERFACE USB
#define log(x) LOG_INTERFACE.print(x)
#define logln(x) LOG_INTERFACE.println(x)
#define logf(x, ...) LOG_INTERFACE.printf(x, __VA_ARGS__)

static constexpr uint32_t PPPOS_BAUDRATE = 1500000;
static const IPAddress PPPOS_GATEWAY(10, 0, 0, 1);
static const IPAddress PPPOS_DNS(8, 8, 8, 8);
static constexpr uint32_t PPPOS_GATEWAY_PORT = 8080;

static constexpr uint32_t GOOGLE_PING_INTERVAL = 4000; // 0 = disabled
static constexpr uint32_t GATEWAY_PING_INTERVAL = 4000; // 0 = disabled
static constexpr uint32_t PING_TIMEOUT = 5000;
static constexpr uint32_t PPP_CHECK_INTERVAL = 1000;

// Stores last Google & Gateway ping info
struct PingInfo {
  uint32_t latency;
  uint32_t timestamp;
};
PingInfo lastGooglePing = {0, 0};
PingInfo lastGatewayPing = {0, 0};

// Example of function using the Arduino TCP/IP API to perform a TCP connection
void pingGoogle() {
  logln("Pinging google.com...");
  NetworkClient client;
  if (client.connect("google.com", 80)) {
    uint32_t startTime = millis();
    client.print("GET / HTTP/1.0\r\nHost: google.com\r\n\r\n");
    unsigned long timeout = millis() + PING_TIMEOUT;
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        // log(c);
      }
    }
    uint32_t endTime = millis();
    uint32_t duration = endTime - startTime;
    logf("\nGoogle Ping completed in %d ms\n", duration);
    
    // Sauvegarder les informations du ping avec le timestamp Unix actuel
    lastGooglePing.latency = duration;
    lastGooglePing.timestamp = time(nullptr) * 1000; // Convertir en millisecondes pour JS
    
    client.stop();
  } else {
    logln("Failed to connect to google.com");
    lastGooglePing.latency = 0;
    lastGooglePing.timestamp = time(nullptr) * 1000;
  }
}

// Ping the Gateway (use python3 -m http.server PORT to start on the Linux host)
void pingGateway() {
  logln("Pinging the Gateway...");
  NetworkClient client;
  // Connect to the local server on port 8080
  if (client.connect(PPPOS_GATEWAY, PPPOS_GATEWAY_PORT)) {  
    uint32_t startTime = millis();
    // Send a minimal HTTP GET request
    client.print("GET / HTTP/1.0\r\nHost: " + String(PPPOS_GATEWAY) + "\r\n\r\n");
    
    // Timeout of 5 seconds
    unsigned long timeout = millis() + PING_TIMEOUT;  
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        // log(c);
      }
    }
    uint32_t endTime = millis();
    uint32_t duration = endTime - startTime;
    logf("\nGateway Ping completed in %d ms\n", duration);
    
    // Sauvegarder les informations du ping
    lastGatewayPing.latency = duration;
    lastGatewayPing.timestamp = time(nullptr) * 1000;
    
    client.stop();
  } else {
    logln("Failed to connect to the Gateway");
    lastGatewayPing.latency = 0;
    lastGatewayPing.timestamp = time(nullptr) * 1000;
  }
}

// AsyncWebServer instance to test inbound requests
AsyncWebServer server(80);

void setup() {
  // Initialize Serial for debugging
  USB.begin(115200);
  delay(3000);

  logln("== PPPoS Client Setup ==");
  
  // Mount LittleFS & list files
  if(LittleFS.begin(false)){
    logln("LittleFS successfully mounted");
    
    File root = LittleFS.open("/");
      if (!root){
        logln("Failed to open root directory");
      } else if(!root.isDirectory()){
        logln("Root is not a directory");
      } else {
        logln("Listing files:");
        File file = root.openNextFile();
        if(!file) {
          logln("No files found !");
        }
        while(file){
          logf("  - File: %s, size: %d bytes\n", file.name(), file.size());
          file = root.openNextFile();
        }
      }
  } else {
    logln("ERROR: Failed to mount LittleFS");
  }

  UART.setTxBufferSize(DPPPoS::UART_TX_BUFFER_SIZE);
  UART.setRxBufferSize(DPPPoS::UART_RX_BUFFER_SIZE);
  UART.begin(PPPOS_BAUDRATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  // Custom IP configuration
  DPPPoS::IPConfig config = {
    .gateway = PPPOS_GATEWAY,
    .dns = PPPOS_DNS
  };

  // Initialize with the custom configuration
  if (!PPPoS.begin(UART, &config)) {
    logln("PPPoS initialization failed, idling...");
    while (true) {
      delay(1000);
    }
  }

  // Configure & wait for NTP synchronization
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Wait for NTP synchronization
  logln("Waiting for NTP synchronization...");
  time_t now = 0;
  while (now < 24 * 3600) {  // On attend d'avoir une date > 1er Jan 1970
    vTaskDelay(pdMS_TO_TICKS(100));
    now = time(nullptr);
  }
  logln("NTP synchronization complete");

  // HTTP Routes - Homepage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  // HTTP Routes - Lightweight GET ping endpoint
  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "pong");
  });

  // HTTP Routes - Endpoint to retrieve Google GET ping info
  server.on("/googlePing", HTTP_GET, [](AsyncWebServerRequest *request){
    String response = String(lastGooglePing.latency) + "," + String(lastGooglePing.timestamp);
    request->send(200, "text/plain", response);
  });

  // HTTP Routes - Endpoint to retrieve Gateway GET ping info
  server.on("/gatewayPing", HTTP_GET, [](AsyncWebServerRequest *request){
    String response = String(lastGatewayPing.latency) + "," + String(lastGatewayPing.timestamp);
    request->send(200, "text/plain", response);
  });

  // Start the HTTP server
  server.begin();
}

void loop() {
  // Once the PPP connection is established, we can use the Arduino TCP/IP API
  static uint32_t last_google_ping = 0;
  static uint32_t last_gateway_ping = 0;
  static uint32_t last_ppp_check = 0;

  if (PPPoS.connected()) {

    if constexpr (GOOGLE_PING_INTERVAL > 0) {
      if (millis() - last_google_ping > GOOGLE_PING_INTERVAL) {
        pingGoogle();
        last_google_ping = millis();
      } 
    }

    if constexpr (GATEWAY_PING_INTERVAL > 0) {
      if (millis() - last_gateway_ping > GATEWAY_PING_INTERVAL) {
        pingGateway();
        last_gateway_ping = millis();
      }
    }

  } 
  else {
      if (millis() - last_ppp_check > PPP_CHECK_INTERVAL) {
        logln("PPP not connected...");
        last_ppp_check = millis();
      }
  }
}