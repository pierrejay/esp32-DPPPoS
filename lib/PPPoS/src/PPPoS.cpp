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

PPPoSClass::PPPoSClass() : _serial(nullptr), ppp(nullptr), connectionStatus(DISCONNECTED) {
  // Rien de particulier dans le constructeur
}

PPPoSClass::~PPPoSClass() {
  // Gestion de la désallocation si nécessaire (non implémenté ici)
}

// ---------------- TASK WRAPPERS ----------------

void PPPoSClass::LoopTask(void* pvParameters) {
  PPPoSClass* instance = (PPPoSClass*)pvParameters;
  while (true) {
    instance->loop();
    vTaskDelay(1);
  }
}

void PPPoSClass::ReconnectTask(void *pvParameters) {
  PPPoSClass* instance = (PPPoSClass*)pvParameters;
  instance->reconnect();
  vTaskDelete(NULL); // Delete the reconnect task after it's done
}


// ---------------- LIFECYCLE METHODS ----------------

void PPPoSClass::loop() {
  if (!_serial) return;

  // If connection is lost, launch reconnect task and delete the current loop task to stop UART processing
  if (connectionStatus == DISCONNECTED) {
    xTaskCreate(ReconnectTask, "ReconnectTask", 16384, this, 5, NULL);
    vTaskDelete(NULL);
    return;
  }

  // Read data from UART
  uint8_t rx_buf[256];
  size_t rx_len = 0;
  while (_serial->available() && rx_len < sizeof(rx_buf)) {
    uint8_t c = _serial->read();
    if (c < 0) break; // Read error
    rx_buf[rx_len++] = (uint8_t)c;
  }

  // Inject data into PPP stack
  if (rx_len > 0) {
    pppos_input(ppp, rx_buf, rx_len);
    logf("[PPPoS] loop(): Received %d bytes\n", rx_len);
    #ifdef PPPOS_DEBUG
      size_t hexdump_len = rx_len * 3 + 1;
      char hexdump[hexdump_len];
      for (size_t i = 0; i < rx_len; i++) {
        sprintf(&hexdump[i * 3], "%02X ", rx_buf[i]);
      }
      logf("[PPPoS] loop(): RX Hexdump : %s\n", hexdump);
    #endif
  }
}

void PPPoSClass::reconnect() {
  if (ppp != nullptr && connectionStatus == DISCONNECTED) {
    connectionStatus = CONNECTING;

    logln("[PPPoS] reconnect(): Starting reconnection procedure");

    logln("[PPPoS] reconnect(): Resetting PPP PCB");
    if (ppp) {
      pppapi_free(ppp);
      ppp = nullptr;
    }
    
    logln("[PPPoS] reconnect(): Resetting network interface");
    netif_remove(&ppp_netif);
    memset(&ppp_netif, 0, sizeof(ppp_netif));

    logln("[PPPoS] reconnect(): Waiting for stack to clean up");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    logln("[PPPoS] reconnect(): Restarting PPP connection");
    if (_serial) begin(*_serial);
  }
}


// ---------------- LIFECYCLE METHODS ----------------

void PPPoSClass::begin(HardwareSerial &serial) {
  _serial = &serial;
  connectionStatus = CONNECTING;

  logln("[PPPoS] begin(): Initializing TCP/IP stack");
  esp_netif_init();

  logln("[PPPoS] begin(): Creating PPPoS interface");
  ppp = pppapi_pppos_create(&ppp_netif, pppos_output_cb, ppp_link_status_cb, this);
  if (ppp == NULL) {
    // If creation failed, set connection status to DISCONNECTED and return.
    // This will require to call begin() again to try to reconnect, or reboot the device.
    logln("[PPPoS] Error creating PPPoS interface");
    connectionStatus = DISCONNECTED;
    return;
  }

  logln("[PPPoS] begin(): Setting default interface");
  auto retSetDefault = pppapi_set_default(ppp);
  if (retSetDefault != ERR_OK) {
    // If setting default interface failed, set connection status to DISCONNECTED and return.
    // This will require to call begin() again to try to reconnect, or reboot the device.
    logf("[PPPoS] begin(): Error setting default interface : %s\n", lwip_strerr(retSetDefault));
    connectionStatus = DISCONNECTED;
    return;
  }

  logln("[PPPoS] begin(): Starting PPP connection");
  auto retConnect = pppapi_connect(ppp, 0);
  if (retConnect != ERR_OK) {
    // If connecting failed, set connection status to DISCONNECTED and return.
    // This will require to call begin() again to try to reconnect, or reboot the device.
    logf("[PPPoS] begin(): Error connecting PPP : %s\n", lwip_strerr(retConnect));
    connectionStatus = DISCONNECTED;
    return;
  }

  if (_serial->available()) {
    logln("[PPPoS] begin(): Clearing serial buffer");
    while (_serial->available()) {
      _serial->read();
    }
  }

  // Start the loop task to enable UART processing
  logln("[PPPoS] begin(): Starting UART processing task");
  xTaskCreate(LoopTask, "LoopTask", 16384, this, 5, NULL);
}

bool PPPoSClass::connected() {
  return connectionStatus == CONNECTED;
}


// ---------------- PPP CALLBACKS ----------------

u32_t PPPoSClass::pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
  PPPoSClass* self = (PPPoSClass*) ctx;
  if (self && self->_serial) {
    size_t written = self->_serial->write(data, len);
    // Hexdump
    logf("[PPPoS] pppos_output_cb(): Sent %d bytes\n", written);
    #ifdef PPPOS_DEBUG
      size_t hexdump_len = len * 3 + 1;
      char hexdump[hexdump_len];
      for (size_t i = 0; i < len; i++) {
        sprintf(&hexdump[i * 3], "%02X ", data[i]);
      }
        logf("[PPPoS] pppos_output_cb(): TX Hexdump : %s\n", hexdump);
    #endif
    return written;
  }
  return 0;
}

void PPPoSClass::ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
  PPPoSClass* self = (PPPoSClass*) ctx;
  
  switch(err_code) {
    case PPPERR_NONE: {
      if (self->connectionStatus == CONNECTED) {
        logln("[PPPoS] ppp_link_status_cb(): Connection already active");
        return;
      }
      logln("[PPPoS] ppp_link_status_cb(): Connection established");
      logf("[PPPoS] ppp_link_status_cb(): - IP address : %s\n", ip4addr_ntoa(netif_ip4_addr(&self->ppp_netif)));

      // Gateway configuration
      ip4_addr_t gw;
      IP4_ADDR(&gw, 10,0,0,1);
      netif_set_gw(&self->ppp_netif, &gw);
      netif_set_default(&self->ppp_netif);
      char gw_str[16];
      ip4addr_ntoa_r(&gw, gw_str, sizeof(gw_str));
      logf("[PPPoS] ppp_link_status_cb(): - Gateway : %s\n", gw_str);

      // DNS configuration
      ip_addr_t dns;
      IP_ADDR4(&dns, 8, 8, 8, 8);
      dns_setserver(0, &dns);
      const ip_addr_t* dns1 = dns_getserver(0);
      logf("[PPPoS] ppp_link_status_cb(): - DNS : %s\n", ip4addr_ntoa((const ip4_addr_t*)dns1));

      self->connectionStatus = CONNECTED;
      break;
    }

    case PPPERR_CONNECT:
      logln("[PPPoS] ppp_link_status_cb(): Connection lost");
      self->connectionStatus = DISCONNECTED;
      break;

    case PPPERR_USER:  // Disconnection requested by user
    case PPPERR_AUTHFAIL:
    case PPPERR_PROTOCOL:
    case PPPERR_PEERDEAD:
    case PPPERR_IDLETIMEOUT:
    case PPPERR_CONNECTTIME:
      logf("[PPPoS] ppp_link_status_cb(): Disconnected, error : %d\n", err_code);
      self->connectionStatus = DISCONNECTED;
      break;

    default:
      logf("[PPPoS] ppp_link_status_cb(): Unknown error : %d\n", err_code);
      self->connectionStatus = DISCONNECTED;
      break;
  }
}

// Global instance of the library (for simple access from the sketch)
PPPoSClass PPPoS;
