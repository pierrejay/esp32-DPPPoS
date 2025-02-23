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
#include "lwip/ip_addr.h"


class PPPoSClass {
  public:

    struct IPConfig {
      IPAddress gateway;
      IPAddress dns;
      
      void setDefault() {
        if (gateway == IPAddress(0,0,0,0)) gateway = IPAddress(10,0,0,1);
        if (dns == IPAddress(0,0,0,0)) dns = IPAddress(8,8,8,8);
      }
    };

    enum ConnectionStatus {
      CONNECTED,          // Connected to the network (set by ppp_link_status_cb() only)
      CONNECTING,         // Connecting to the network (set by connect() only)
      CONNECTION_LOST,    // Connection lost (set by ppp_link_status_cb() only)
      DISCONNECTED        // Disconnected from the network (set by disconnect() only)
    };

    PPPoSClass();
    ~PPPoSClass();

    bool begin(HardwareSerial &serial, const IPConfig* config = nullptr);
    bool connected();

  private:

    // Private attributes
    HardwareSerial* _serial;  // Serial port used for PPPoS
    ppp_pcb *ppp;             // PPP control block
    struct netif ppp_netif;   // PPP network interface
    volatile ConnectionStatus connectionStatus;
    IPConfig _config;         // IP configuration (gateway, dns)

    // Private methods
    void loop();
    bool connect();
    bool disconnect();
    void setNetworkCfg(ip4_addr_t& gw, ip_addr_t& dns);

    // FreeRTOS tasks wrappers
    static void LoopTask(void* pvParameters);
    static void NetWatchdogTask(void *pvParameters);

    // PPP callbacks
    static u32_t pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);
    static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx);

    // IP conversion
    static void IPAddressToLwIP(const IPAddress &arduino_ip, ip_addr_t &lwip_ip);
    static void IPAddressToLwIP(const IPAddress &arduino_ip, ip4_addr_t &lwip_ip);

};

extern PPPoSClass PPPoS;

#endif // CONFIG_LWIP_PPP_SUPPORT
#endif // PPPOS_H
