#include <Arduino.h>
#include "PPPoS.h"
#include <ESPAsyncWebServer.h>

#define USB Serial
#define UART Serial1

#define UART_TX_PIN D7
#define UART_RX_PIN D6

#define LOG_INTERFACE Serial
#define log(x) LOG_INTERFACE.print(x)
#define logln(x) LOG_INTERFACE.println(x)
#define logf(x, ...) LOG_INTERFACE.printf(x, __VA_ARGS__)

#define PPPOS_BAUDRATE 115200
#define PPPOS_GATEWAY IPAddress(10, 0, 0, 1)
#define PPPOS_DNS IPAddress(8, 8, 8, 8)
#define PPPOS_GATEWAY_PORT 8080

// Example of function using the Arduino TCP/IP API to perform a TCP connection
void pingGoogle() {
  logln("Pinging google.com...");
  NetworkClient client;
  if (client.connect("google.com", 80)) {
    // Send a minimal HTTP GET request
    uint32_t startTime = millis();
    client.print("GET / HTTP/1.0\r\nHost: google.com\r\n\r\n");
    unsigned long timeout = millis() + 5000;  // timeout of 5 seconds
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        log(c);
      }
    }
    uint32_t endTime = millis();
    uint32_t duration = endTime - startTime;
    logf("\nPing completed in %d ms\n", duration);
    client.stop();
  } else {
    logln("Failed to connect to google.com");
  }
}

// Ping the Gateway
void pingGateway() {
  logln("Pinging the Gateway...");
  NetworkClient client;
  // Connect to the local server on port 8080
  if (client.connect(PPPOS_GATEWAY, PPPOS_GATEWAY_PORT)) {  
    uint32_t startTime = millis();
    // Send a minimal HTTP GET request
    client.print("GET / HTTP/1.0\r\nHost: " + String(PPPOS_GATEWAY) + "\r\n\r\n");
    
    // Timeout of 5 seconds
    unsigned long timeout = millis() + 5000;  
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        log(c);
      }
    }
    uint32_t endTime = millis();
    uint32_t duration = endTime - startTime;
    logf("\nPing completed in %d ms\n", duration);
    client.stop();
  } else {
    logln("Failed to connect to the Gateway");
  }
}

// AsyncWebServer instance to test inbound requests
AsyncWebServer server(80);

void setup() {
  // Initialize Serial for debugging
  USB.begin(115200);
  delay(3000);

  logln("== PPPoS Client Setup ==");
  UART.begin(PPPOS_BAUDRATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  // Custom IP configuration
  PPPoSClass::IPConfig config = {
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

  // Setup serveur web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Hello World");
  });
  server.begin();
}

void loop() {
  // Once the PPP connection is established, we can use the Arduino TCP/IP API
  static uint32_t last_ping = 0;
  static uint32_t last_reconnect = 0;
  if (PPPoS.connected()) {
    if (millis() - last_ping > 10000) {
      pingGoogle();
      last_ping = millis();
    }
  } else {
    if (millis() - last_reconnect > 1000) {
      logln("PPP not connected...");
      last_reconnect = millis();
    }
  }
}