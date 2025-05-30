#ifndef PPPOS_LIB_H
#define PPPOS_LIB_H

#include <Arduino.h>
#include "sdkconfig.h"

// If PPP support is disabled, the library will not compile. Make sure to use the correct ESP32 Arduino core.
#if CONFIG_LWIP_PPP_SUPPORT 
#include "esp_netif.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "netif/ppp/pppapi.h"
#include "netif/ppp/pppos.h"
#include "Network.h"
#include "lwip/ip_addr.h"

class DPPPoS {
  public:
      // Default values
      static constexpr uint16_t UART_RX_BUFFER_SIZE = 2048;
      static constexpr uint16_t UART_TX_BUFFER_SIZE = 2048;
      static constexpr uint16_t DISCONNECT_CLEANUP_DELAY = 1000;
      static constexpr uint16_t NET_WATCHDOG_INTERVAL = 3000;
      static constexpr uint8_t NET_WATCHDOG_TASK_PRIORITY = 1;
      static constexpr uint8_t LOOP_TASK_PRIORITY = 5;
      static constexpr uint32_t LOOPTASK_STACK_SIZE = 16384;
      static constexpr uint32_t NETWATCHDOGTASK_STACK_SIZE = 16384;

    struct IPConfig {
      IPAddress gateway = IPAddress(0,0,0,0);
      IPAddress dns = IPAddress(0,0,0,0);
    };

    enum ConnectionStatus {
      CONNECTED,          // Connected to the network (set by ppp_link_status_cb() only)
      CONNECTING,         // Connecting to the network (set by connect() only)
      CONNECTION_LOST,    // Connection lost (set by ppp_link_status_cb() only)
      DISCONNECTED        // Disconnected from the network (set by disconnect() only)
    };

    DPPPoS();

    bool begin(Stream& serial, const IPConfig* config = nullptr);
    bool connected() const;
    ConnectionStatus getStatus() const;

  private:

    // Private attributes
    Stream* _serial;          // Serial port used for PPPoS
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

}; // PPPoSClass

extern DPPPoS PPPoS;

#endif // CONFIG_LWIP_PPP_SUPPORT
#endif // PPPOS_H
