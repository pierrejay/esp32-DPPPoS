#include "PPPoS.h"

PPPoSClass::PPPoSClass() : _serial(nullptr), ppp(nullptr) {
  // Rien de particulier dans le constructeur
}

PPPoSClass::~PPPoSClass() {
  // Gestion de la désallocation si nécessaire (non implémenté ici)
}

void PPPoSClass::begin(HardwareSerial &serial) {
  _serial = &serial;
  
  // On ne fait plus d'attente ici car c'est le port de données, pas celui de debug
  // while (!_serial) {}

  logln("[PPPoS] Initialisation de la pile TCP/IP");
  esp_netif_init();

  // Création de l’interface PPPoS.
  // On passe "this" en contexte pour pouvoir accéder aux méthodes et au port série depuis les callbacks.
  logln("[PPPoS] Création de l'interface PPPoS");
  ppp = pppapi_pppos_create(&ppp_netif, pppos_output_cb, ppp_link_status_cb, this);
  if (ppp == NULL) {
    logln("[PPPoS] Erreur lors de la création de l'interface PPPoS");
    return;
  }
  // Définit l’interface PPP comme interface réseau par défaut
  logln("[PPPoS] Configuration de l'interface par défaut");
  pppapi_set_default(ppp);

  // Démarre la connexion PPP (le second paramètre est un flag, ici 0)
  logln("[PPPoS] Démarrage de la connexion PPP");
  pppapi_connect(ppp, 0);
}

void PPPoSClass::loop() {
  if (!_serial) return;
  uint8_t buf[256];
  // Si des données sont disponibles sur le port série, on les lit
  if (_serial->available()) {
    int len = _serial->readBytes(buf, sizeof(buf));
    if (len > 0) {
      // Injection des trames reçues dans la pile PPP
      pppos_input(ppp, buf, len);
      logf("[PPPoS] Données reçues : %d bytes\n", len);
    }
  }
}

bool PPPoSClass::connected() {
  // Si une adresse IP a été assignée à l'interface, la connexion est établie
  return (ppp_netif.ip_addr.u_addr.ip4.addr != 0);
}

u32_t PPPoSClass::pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
  PPPoSClass* self = (PPPoSClass*) ctx;
  if (self && self->_serial) {
    size_t written = self->_serial->write(data, len);
    self->_serial->flush();
    logf("[PPPoS] Données envoyées : %d bytes\n", written);
    return written;
  }
  return 0;
}

void PPPoSClass::ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
  PPPoSClass* self = (PPPoSClass*) ctx;
  if (err_code == PPPERR_NONE) {
    logln("[PPPoS] Connexion PPP établie");
    logf("[PPPoS] Adresse IP : %s\n", ip4addr_ntoa(netif_ip4_addr(&self->ppp_netif)));
  } else {
    logf("[PPPoS] Déconnecté, erreur : %d\n", err_code);
  }
}

// Instanciation globale de la librairie (pour un accès simple depuis le sketch)
PPPoSClass PPPoS;
