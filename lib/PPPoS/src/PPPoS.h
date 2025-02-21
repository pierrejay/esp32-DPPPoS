#ifndef PPPOS_LIB_H
#define PPPOS_LIB_H

#include <Arduino.h>
#include "sdkconfig.h"
#if CONFIG_LWIP_PPP_SUPPORT
#include "esp_netif.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "netif/ppp/pppapi.h"
#include "netif/ppp/pppos.h"
#include "Network.h"

class PPPoSClass {
  public:
    PPPoSClass();
    ~PPPoSClass();

    // Démarre la liaison PPP sur le port série donné et avec le débit spécifié.
    // Exemple : PPPoS.begin(Serial1, 115200);
    void begin(HardwareSerial &serial);

    // À appeler régulièrement dans loop() pour traiter les données UART entrantes
    void loop();

    // Renvoie true si une adresse IP a été obtenue
    bool connected();

  private:
    HardwareSerial* _serial;  // Port série utilisé pour PPPoS
    ppp_pcb *ppp;             // Contrôle de la liaison PPP
    struct netif ppp_netif;     // Interface réseau associée

    // Callback appelée par lwIP pour envoyer des trames sur le lien UART
    static u32_t pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);

    // Callback appelée pour notifier les changements d'état du lien PPP
    static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx);
};

extern PPPoSClass PPPoS;

#endif // CONFIG_LWIP_PPP_SUPPORT
#endif // PPPOS_H
