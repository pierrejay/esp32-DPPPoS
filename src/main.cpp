#include <Arduino.h>
#include "PPPoS.h"

#ifndef LOG_STREAM
#define LOG_STREAM Serial
#endif
#ifndef log
#define log(x) LOG_STREAM.print(x)
#endif
#ifndef logln
#define logln(x) LOG_STREAM.println(x)
#endif
#ifndef logf
#define logf(x, ...) LOG_STREAM.printf(x, __VA_ARGS__)
#endif

// Exemple de fonction utilisant l'API Arduino pour réaliser une connexion TCP
void pingGoogle() {
  logln("Ping de google.com en cours...");
  NetworkClient client;
  if (client.connect("google.com", 80)) {
    // Envoi d'une requête HTTP GET minimale
    client.print("GET / HTTP/1.0\r\nHost: google.com\r\n\r\n");
    unsigned long timeout = millis() + 5000;  // timeout de 5 secondes
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        log(c);
      }
    }
    logln("\nPing terminé");
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
  Serial1.begin(115200, SERIAL_8N1, 44, 43);

  // Par exemple, on peut utiliser Serial1 pour la liaison PPP, ou même Serial si disponible
  PPPoS.begin(Serial1);
}

void loop() {
  // Traitement des données PPP
  PPPoS.loop();

  // Une fois la connexion PPP établie, on peut utiliser l’API Arduino TCP/IP
  if (PPPoS.connected()) {
    pingGoogle();
    delay(10000);
  } else {
    logln("PPP non connecté...");
    delay(1000);
  }
}