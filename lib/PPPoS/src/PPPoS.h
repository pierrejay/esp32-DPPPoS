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

    enum ConnectionStatus {
      DISCONNECTED,
      CONNECTING,
      CONNECTED
    };

    PPPoSClass();
    ~PPPoSClass();

    void begin(HardwareSerial &serial);
    bool connected();


  private:

    // Private attributes
    HardwareSerial* _serial;  // Serial port used for PPPoS
    ppp_pcb *ppp;             // PPP control block
    struct netif ppp_netif;   // PPP network interface
    ConnectionStatus connectionStatus;

    // Private methods
    void loop();
    void reconnect(); 

    // FreeRTOS tasks wrappers
    static void LoopTask(void* pvParameters);
    static void ReconnectTask(void *pvParameters);

    // PPP callbacks
    static u32_t pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);
    static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx);

};

extern PPPoSClass PPPoS;

#endif // CONFIG_LWIP_PPP_SUPPORT
#endif // PPPOS_H
