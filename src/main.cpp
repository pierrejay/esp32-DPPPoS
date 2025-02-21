#include <Arduino.h>
#include <PPP.h>

// Paramètres PPP (valeurs fictives car on se connecte directement au Raspberry Pi)
#define PPP_APN   "dummy"
#define PPP_PIN   NULL

// Définition des broches UART utilisées
// Note : la broche TX de l’ESP32 doit être reliée au RX du Pi et vice-versa
#define PPP_TX_PIN 43
#define PPP_RX_PIN 44
// Pas de contrôle de flux utilisé
#define PPP_RTS_PIN -1
#define PPP_CTS_PIN -1

// Callback pour capter les événements PPP (connexion, obtention d'IP, etc.)
void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_PPP_START:
      Serial.println("PPP démarré");
      break;
    case ARDUINO_EVENT_PPP_CONNECTED:
      Serial.println("PPP connecté");
      break;
    case ARDUINO_EVENT_PPP_GOT_IP:
      Serial.println("PPP IP obtenue");
      break;
    case ARDUINO_EVENT_PPP_DISCONNECTED:
      Serial.println("PPP déconnecté");
      break;
    default:
      break;
  }
}

// Fonction de « ping » : on se connecte à google.com sur le port 80, envoie une requête HTTP simple,
// et on affiche la réponse dans le moniteur série (cela permet de vérifier la connectivité).
void pingGoogle() {
  Serial.println("Ping de google.com en cours...");
  NetworkClient client;
  if (client.connect("google.com", 80)) {
    // Envoyer une requête HTTP GET minimale
    client.print("GET / HTTP/1.0\r\nHost: google.com\r\n\r\n");
    unsigned long timeout = millis() + 5000;  // timeout de 5 secondes
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        Serial.write(c);
      }
    }
    Serial.println("\nPing terminé");
    client.stop();
  } else {
    Serial.println("Échec de la connexion à google.com");
  }
}

void setup() {
  Serial.begin(115200);
  // Enregistrer la fonction de callback pour suivre les événements PPP
  Network.onEvent(onEvent);
  
  // Configurer les paramètres PPP (APN et PIN factices)
  PPP.setApn(PPP_APN);
  PPP.setPin(PPP_PIN);
  
  // Configurer les broches UART utilisées pour la communication PPP
  PPP.setPins(PPP_TX_PIN, PPP_RX_PIN, PPP_RTS_PIN, PPP_CTS_PIN, ESP_MODEM_FLOW_CONTROL_NONE);
  
  // Démarrer la liaison PPP sur UART1 à 115200 bauds en utilisant le modèle générique
  Serial.println("Initialisation de la connexion PPP...");
  PPP.begin(PPP_MODEM_GENERIC, 1, 115200);
}

void loop() {
  // Dès que la connexion PPP est établie, on ping google.com toutes les 5 secondes
  if (PPP.connected()) {
    pingGoogle();
    delay(5000);
  } else {
    Serial.println("PPP non connecté...");
    delay(1000);
  }
}
