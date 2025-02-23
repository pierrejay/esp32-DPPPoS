#include <Arduino.h>
#include "PPPoS.h"
#include <ESPAsyncWebServer.h>

#define TXD_PIN D7
#define RXD_PIN D6

#define LOG_INTERFACE Serial
#define log(x) LOG_INTERFACE.print(x)
#define logln(x) LOG_INTERFACE.println(x)
#define logf(x, ...) LOG_INTERFACE.printf(x, __VA_ARGS__)

// Exemple de fonction utilisant l'API Arduino pour réaliser une connexion TCP
void pingGoogle() {
  logln("Ping de google.com en cours...");
  NetworkClient client;
  if (client.connect("google.com", 80)) {
    // Envoi d'une requête HTTP GET minimale
    uint32_t startTime = millis();
    client.print("GET / HTTP/1.0\r\nHost: google.com\r\n\r\n");
    unsigned long timeout = millis() + 5000;  // timeout de 5 secondes
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        log(c);
      }
    }
    uint32_t endTime = millis();
    uint32_t duration = endTime - startTime;
    logf("\nPing terminé en %d ms\n", duration);
    client.stop();
  } else {
    logln("Échec de la connexion à google.com");
  }
}

// Ping du RPi
void pingRPi() {
  logln("Ping du RPi en cours...");
  NetworkClient client;
  // Connectez-vous au serveur local sur le port 8080
  if (client.connect("10.0.0.1", 8080)) {  
    uint32_t startTime = millis();
    // Envoi d'une requête HTTP GET minimale
    client.print("GET / HTTP/1.0\r\nHost: 10.0.0.1\r\n\r\n");
    
    // Timeout de 5 secondes
    unsigned long timeout = millis() + 5000;  
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        log(c);
      }
    }
    uint32_t endTime = millis();
    uint32_t duration = endTime - startTime;
    logf("\nPing terminé en %d ms\n", duration);
    client.stop();
  } else {
    logln("Échec de la connexion au RPi");
  }
}

AsyncWebServer server(80);


void setup() {
  // Initialisation du port série pour le debug
  Serial.begin(115200);
  delay(3000);
  logln("Démarrage de la connexion PPPoS");
  Serial1.setTxBufferSize(2048);
  Serial1.setRxBufferSize(2048);
  Serial1.begin(115200, SERIAL_8N1, RXD_PIN, TXD_PIN);

  // Configuration personnalisée
  PPPoSClass::IPConfig config = {
    .gateway = IPAddress(10, 0, 0, 1),
    .dns = IPAddress(8, 8, 8, 8)
  };

  // Initialisation avec la configuration
  PPPoS.begin(Serial1, &config);

  // Setup serveur web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Hello World");
  });
  server.begin();
}

void loop() {
  // Une fois la connexion PPP établie, on peut utiliser l’API Arduino TCP/IP
  static uint32_t last_ping = 0;
  static uint32_t last_reconnect = 0;
  if (PPPoS.connected()) {
    if (millis() - last_ping > 10000) {
      pingGoogle();
      last_ping = millis();
    }
  } else {
    if (millis() - last_reconnect > 1000) {
      logln("PPP non connecté...");
      last_reconnect = millis();
    }
  }
}