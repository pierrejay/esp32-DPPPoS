#include "PPPoS.h"
#include "lwip/dns.h"

#ifdef PPPOS_DEBUG
#define LOG_INTERFACE Serial
#define log(x) LOG_INTERFACE.print(x)
#define logln(x) LOG_INTERFACE.println(x)
#define logf(x, ...) LOG_INTERFACE.printf(x, __VA_ARGS__)
#else
#define log(x) ((void)0)
#define logln(x) ((void)0)
#define logf(x, ...) ((void)0)
#endif

PPPoSClass::PPPoSClass() : _serial(nullptr), ppp(nullptr) {
  // Rien de particulier dans le constructeur
}

PPPoSClass::~PPPoSClass() {
  // Gestion de la désallocation si nécessaire (non implémenté ici)
}

void PPPoSClass::loop() {
  if (!_serial) return;

  // Gestion des données reçues
  uint8_t rx_buf[256];
  size_t rx_len = 0;
  
  // On lit tant qu'il y a des données et qu'on a de la place dans le buffer
  while (_serial->available() && rx_len < sizeof(rx_buf)) {
    int c = _serial->read();
    if (c < 0) break;  // Erreur de lecture
    rx_buf[rx_len++] = (uint8_t)c;
  }

  if (rx_len > 0) {
    // Injection des trames reçues dans la pile PPP
    pppos_input(ppp, rx_buf, rx_len);
    // Hexdump
    logf("[PPPoS] Données reçues : %d bytes\n", rx_len);
    size_t hexdump_len = rx_len * 3 + 1;
    char hexdump[hexdump_len];
    for (size_t i = 0; i < rx_len; i++) {
      sprintf(&hexdump[i * 3], "%02X ", rx_buf[i]);
    }
    logf("[PPPoS] RX Hexdump : %s\n", hexdump);
  }
}


// Ajoutez cette nouvelle fonction statique qui servira de wrapper
void PPPoSClass::loopTask(void* pvParameters) {
  PPPoSClass* instance = (PPPoSClass*)pvParameters;
  while (true) {
    instance->loop();
    vTaskDelay(1);
  }
}

void PPPoSClass::begin(HardwareSerial &serial) {
  _serial = &serial;

  logln("[PPPoS] Initialisation de la pile TCP/IP");
  esp_netif_init();

  // Création de l'interface PPPoS.
  // On passe "this" en contexte pour pouvoir accéder aux méthodes et au port série depuis les callbacks.
  logln("[PPPoS] Création de l'interface PPPoS");
  ppp = pppapi_pppos_create(&ppp_netif, pppos_output_cb, ppp_link_status_cb, this);
  if (ppp == NULL) {
    logln("[PPPoS] Erreur lors de la création de l'interface PPPoS");
    return;
  }
  // Définit l'interface PPP comme interface réseau par défaut
  logln("[PPPoS] Configuration de l'interface par défaut");
  auto retSetDefault = pppapi_set_default(ppp);
  if (retSetDefault != ERR_OK) {
    logf("[PPPoS] Erreur lors de la configuration de l'interface par défaut : %s\n", lwip_strerr(retSetDefault));
    return;
  }

  // Démarre la connexion PPP (le second paramètre est un flag, ici 0)
  logln("[PPPoS] Démarrage de la connexion PPP");
  auto retConnect = pppapi_connect(ppp, 0);
  if (retConnect != ERR_OK) {
    logf("[PPPoS] Erreur lors de la connexion PPP : %s\n", lwip_strerr(retConnect));
    return;
  }

  // Démarre la tâche de traitement des données UART
  logln("[PPPoS] Démarrage de la tâche de traitement des données UART");
  xTaskCreate(loopTask, "PPPoSTask", 16384, this, 5, NULL);
}

bool PPPoSClass::connected() {
  // Si une adresse IP a été assignée à l'interface, la connexion est établie
  return (ppp_netif.ip_addr.u_addr.ip4.addr != 0);
}

u32_t PPPoSClass::pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
  PPPoSClass* self = (PPPoSClass*) ctx;
  if (self && self->_serial) {
    size_t written = self->_serial->write(data, len);
    // Hexdump
    logf("[PPPoS] Données envoyées : %d bytes\n", written);
    size_t hexdump_len = len * 3 + 1;
    char hexdump[hexdump_len];
    for (size_t i = 0; i < len; i++) {
      sprintf(&hexdump[i * 3], "%02X ", data[i]);
    }
    logf("[PPPoS] TX Hexdump : %s\n", hexdump);
    // Hexdump
    return written;
  }
  return 0;
}

void PPPoSClass::ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
  PPPoSClass* self = (PPPoSClass*) ctx;
  if (err_code == PPPERR_NONE) {
    logln("[PPPoS] Connexion PPP établie");
    logf("[PPPoS] Adresse IP : %s\n", ip4addr_ntoa(netif_ip4_addr(&self->ppp_netif)));

    // Définit la passerelle par défaut
    ip4_addr_t gw;
    IP4_ADDR(&gw, 10,0,0,1);
    netif_set_gw(&self->ppp_netif, &gw);
    netif_set_default(&self->ppp_netif);

    // Configure un serveur DNS, ici par exemple 8.8.8.8
    ip_addr_t dns;
    IP_ADDR4(&dns, 8, 8, 8, 8);
    dns_setserver(0, &dns);
    
    // Affichage des serveurs DNS configurés
    const ip_addr_t* dns1 = dns_getserver(0);
    const ip_addr_t* dns2 = dns_getserver(1);
    const ip_addr_t* dns3 = dns_getserver(2);

    logln("[PPPoS] Serveurs DNS configurés :");
    if (dns1 && !ip_addr_isany(dns1)) {
      logf("[PPPoS] DNS1: %s\n", ip4addr_ntoa((const ip4_addr_t*)dns1));
    }
    if (dns2 && !ip_addr_isany(dns2)) {
      logf("[PPPoS] DNS2: %s\n", ip4addr_ntoa((const ip4_addr_t*)dns2));
    }
    if (dns3 && !ip_addr_isany(dns3)) {
      logf("[PPPoS] DNS3: %s\n", ip4addr_ntoa((const ip4_addr_t*)dns3));
    }
  } else {
    logf("[PPPoS] Déconnecté, erreur : %d\n", err_code);
  }
}

// Instanciation globale de la librairie (pour un accès simple depuis le sketch)
PPPoSClass PPPoS;
