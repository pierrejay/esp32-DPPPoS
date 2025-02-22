#include <Arduino.h>
#include "PPPoS.h"

#define TXD_PIN D7
#define RXD_PIN D6

#define LOG_INTERFACE Serial
#define log(x) LOG_INTERFACE.print(x)
#define logln(x) LOG_INTERFACE.println(x)
#define logf(x, ...) LOG_INTERFACE.printf(x, __VA_ARGS__)

void hexdump(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    logf("%02X ", data[i]);
  }
  logln("");
}

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

void setup() {
  // Initialisation du port série pour le debug
  Serial.begin(115200);
  delay(3000);
  logln("Démarrage de la connexion PPPoS");
  Serial1.begin(921600, SERIAL_8N1, RXD_PIN, TXD_PIN);

  // Par exemple, on peut utiliser Serial1 pour la liaison PPP, ou même Serial si disponible
  PPPoS.begin(Serial1);
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

  // static uint32_t last_read = 0;
  // if (millis() - last_read > 50) {
  //   if (Serial1.available()) {
  //     uint8_t buf[256];
  //     size_t len = Serial1.readBytes(buf, sizeof(buf));
  //     hexdump(buf, len);
  //   }
  //   last_read = millis();
  // }
}