#include <Arduino.h>
#include "DPPPoS.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <time.h>

#define USB Serial
#define UART Serial1

#define UART_TX_PIN D7
#define UART_RX_PIN D6

#define LOG_INTERFACE Serial
#define log(x) LOG_INTERFACE.print(x)
#define logln(x) LOG_INTERFACE.println(x)
#define logf(x, ...) LOG_INTERFACE.printf(x, __VA_ARGS__)

static constexpr uint32_t PPPOS_BAUDRATE = 230400;
static const IPAddress PPPOS_GATEWAY(10, 0, 0, 1);
static const IPAddress PPPOS_DNS(8, 8, 8, 8);
static constexpr uint32_t PPPOS_GATEWAY_PORT = 8080;

static constexpr uint32_t GOOGLE_PING_INTERVAL = 5000; // 0 = disabled
static constexpr uint32_t GATEWAY_PING_INTERVAL = 0; // 0 = disabled
static constexpr uint32_t PPP_CHECK_INTERVAL = 1000;

// Structure pour stocker la dernière mesure de ping Google
struct GooglePingInfo {
  uint32_t latency;
  uint32_t timestamp;
} lastGooglePing = {0, 0};

// Example of function using the Arduino TCP/IP API to perform a TCP connection
void pingGoogle() {
  logln("Pinging google.com...");
  NetworkClient client;
  if (client.connect("google.com", 80)) {
    uint32_t startTime = millis();
    client.print("GET / HTTP/1.0\r\nHost: google.com\r\n\r\n");
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
  
  // Initialisation de SPIFFS
  if(!SPIFFS.begin(true)){
    logln("Une erreur est survenue lors de l'initialisation de SPIFFS");
    return;
  }

  // Debug : Liste les fichiers présents dans SPIFFS
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file){
    logf("Fichier trouvé: %s\n", file.name());
    file = root.openNextFile();
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

  // Configuration et attente de la synchronisation NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Attendre que l'heure soit synchronisée
  logln("Attente de la synchronisation NTP...");
  time_t now = 0;
  while (now < 24 * 3600) {  // On attend d'avoir une date > 1er Jan 1970
    vTaskDelay(pdMS_TO_TICKS(100));
    now = time(nullptr);
  }
  logln("Heure synchronisée !");

  // Modification de la route racine pour servir index.html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // Ajouter un endpoint léger pour le ping
  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "pong");
  });

  // Ajouter un nouvel endpoint pour récupérer les infos de ping Google
  server.on("/googlePing", HTTP_GET, [](AsyncWebServerRequest *request){
    String response = String(lastGooglePing.latency) + "," + String(lastGooglePing.timestamp);
    request->send(200, "text/plain", response);
  });

  server.begin();
}

void loop() {
  // Once the PPP connection is established, we can use the Arduino TCP/IP API
  static uint32_t last_ping = 0;
  static uint32_t last_ppp_check = 0;

  if (PPPoS.connected()) {

    if constexpr (GOOGLE_PING_INTERVAL > 0) {
      if (millis() - last_ping > GOOGLE_PING_INTERVAL) {
        pingGoogle();
        last_ping = millis();
      } 
    }

    if constexpr (GATEWAY_PING_INTERVAL > 0) {
      if (millis() - last_ping > GATEWAY_PING_INTERVAL) {
        pingGateway();
        last_ping = millis();
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