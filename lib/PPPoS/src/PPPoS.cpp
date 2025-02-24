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


PPPoSClass::PPPoSClass() : _serial(nullptr), ppp(nullptr), connectionStatus(DISCONNECTED) {}


// ---------------- PUBLIC METHODS ----------------

bool PPPoSClass::begin(HardwareSerial &serial, const IPConfig* config) {
  _serial = &serial;
  _serial->setTxBufferSize(UART_TX_BUFFER_SIZE);
  _serial->setRxBufferSize(UART_RX_BUFFER_SIZE);

  if (config) _config = *config;

  logln("[PPPoS] begin(): Setting up PPPoS connection...");
  bool connecting = connect();
  if (!connecting) {
    logln("[PPPoS] begin(): PPPoS connection setup failed, aborting");
    return false;
  }
  logln("[PPPoS] begin(): PPPoS connection setup complete, starting tasks...");

  xTaskCreate(LoopTask, "PPPoS_LoopTask", LOOPTASK_STACK_SIZE, this, LOOP_TASK_PRIORITY, NULL);
  xTaskCreate(NetWatchdogTask, "PPPoS_NetWatchdogTask", NETWATCHDOGTASK_STACK_SIZE, this, NET_WATCHDOG_TASK_PRIORITY, NULL);

  return true;
}

bool PPPoSClass::connected() {
  return connectionStatus == CONNECTED;
}



// ---------------- TASK WRAPPERS ----------------

void PPPoSClass::LoopTask(void* pvParameters) {
  PPPoSClass* instance = (PPPoSClass*)pvParameters;
  while (true) {
    instance->loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void PPPoSClass::NetWatchdogTask(void *pvParameters) {
  PPPoSClass* instance = (PPPoSClass*)pvParameters;
  while (true) {
    if (instance->connectionStatus == CONNECTION_LOST) {
      instance->disconnect();
    }
    if (instance->connectionStatus == DISCONNECTED) {
      instance->connect();
    }
    vTaskDelay(pdMS_TO_TICKS(NET_WATCHDOG_INTERVAL));
  }
}



// ---------------- LOOP FUNCTION ----------------

void PPPoSClass::loop() {
  if (!_serial) return;

  // Only process data if connecting or connected
  if (connectionStatus != CONNECTING && connectionStatus != CONNECTED) return;

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



// ---------------- CONNEXION METHODS ----------------

bool PPPoSClass::connect() {
  if (connectionStatus == CONNECTED || connectionStatus == CONNECTING) {
    logln("[PPPoS] connect(): Connection already active, aborting");
    return false;
  }

  logln("[PPPoS] begin(): Initializing TCP/IP stack");
  esp_netif_init();

  logln("[PPPoS] begin(): Creating PPPoS interface");
  ppp = pppapi_pppos_create(&ppp_netif, pppos_output_cb, ppp_link_status_cb, this);
  if (ppp == NULL) {
    // If creation failed, set connection status to DISCONNECTED and return.
    // This will require to call begin() again to try to reconnect, or reboot the device.
    logln("[PPPoS] Error creating PPPoS interface");
    return false;
  }

  logln("[PPPoS] begin(): Setting default interface");
  auto retSetDefault = pppapi_set_default(ppp);
  if (retSetDefault != ERR_OK) {
    // If setting default interface failed, set connection status to DISCONNECTED and return.
    // This will require to call begin() again to try to reconnect, or reboot the device.
    logf("[PPPoS] begin(): Error setting default interface : %s\n", lwip_strerr(retSetDefault));
    return false;
  }

  logln("[PPPoS] begin(): Starting PPP connection");
  auto retConnect = pppapi_connect(ppp, 0);
  if (retConnect != ERR_OK) {
    // If connecting failed, set connection status to DISCONNECTED and return.
    // This will require to call begin() again to try to reconnect, or reboot the device.
    logf("[PPPoS] begin(): Error connecting PPP : %s\n", lwip_strerr(retConnect));
    return false;
  }

  if (_serial->available()) {
    logln("[PPPoS] begin(): Clearing serial buffer");
    while (_serial->available()) {
      _serial->read();
    }
  }

  connectionStatus = CONNECTING;
  return true;
}

bool PPPoSClass::disconnect() {
  if (connectionStatus == DISCONNECTED) {
    logln("[PPPoS] disconnect(): Interface already disconnected, aborting");
    return false;
  }

  if (!ppp) {
    logln("[PPPoS] disconnect(): No PPP interface found, aborting");
    return false;
  }

  logln("[PPPoS] disconnect(): Starting disconnection procedure");

  logln("[PPPoS] disconnect(): Resetting PPP PCB");
  if (ppp) {
    pppapi_free(ppp);
    ppp = nullptr;
  }
  
  logln("[PPPoS] disconnect(): Resetting network interface");
  netif_remove(&ppp_netif);
  memset(&ppp_netif, 0, sizeof(ppp_netif));

  logln("[PPPoS] disconnect(): Waiting for stack to clean up");
  vTaskDelay(pdMS_TO_TICKS(DISCONNECT_CLEANUP_DELAY));

  connectionStatus = DISCONNECTED;
  return true;
}

void PPPoSClass::setNetworkCfg(ip4_addr_t& gw, ip_addr_t& dns) {
  // Gateway configuration
  if (_config.gateway != IPAddress(0,0,0,0)) {
    IPAddressToLwIP(_config.gateway, gw);
    netif_set_gw(&ppp_netif, &gw);
    netif_set_default(&ppp_netif);
  }

  // DNS configuration
  if (_config.dns != IPAddress(0,0,0,0)) {
    IPAddressToLwIP(_config.dns, dns);
    dns_setserver(0, &dns);
  }
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

      ip4_addr_t gw;
      ip_addr_t dns;
      self->setNetworkCfg(gw, dns);
      char gw_str[16];
      ip4addr_ntoa_r(&gw, gw_str, sizeof(gw_str));
      const ip_addr_t* dns1 = dns_getserver(0);

      logf("[PPPoS] ppp_link_status_cb(): - IP address : %s\n", ip4addr_ntoa(netif_ip4_addr(&self->ppp_netif)));
      logf("[PPPoS] ppp_link_status_cb(): - Gateway : %s\n", gw_str); 
      logf("[PPPoS] ppp_link_status_cb(): - DNS : %s\n", ip4addr_ntoa((const ip4_addr_t*)dns1));

      self->connectionStatus = CONNECTED;
      break;
    }

    case PPPERR_CONNECT:
      logln("[PPPoS] ppp_link_status_cb(): Connection lost");
      self->connectionStatus = CONNECTION_LOST;
      break;

    case PPPERR_USER:  // Disconnection requested by user
    case PPPERR_AUTHFAIL:
    case PPPERR_PROTOCOL:
    case PPPERR_PEERDEAD:
    case PPPERR_IDLETIMEOUT:
    case PPPERR_CONNECTTIME:
      logf("[PPPoS] ppp_link_status_cb(): Disconnected, error : %d\n", err_code);
      self->connectionStatus = CONNECTION_LOST;
      break;

    default:
      logf("[PPPoS] ppp_link_status_cb(): Unknown error : %d\n", err_code);
      self->connectionStatus = CONNECTION_LOST;
      break;
  }
}



// ---------------- UTILITY FUNCTIONS ----------------

void PPPoSClass::IPAddressToLwIP(const IPAddress &arduino_ip, ip_addr_t &lwip_ip) {
  IP_ADDR4(&lwip_ip, 
    arduino_ip[0], 
    arduino_ip[1], 
    arduino_ip[2], 
    arduino_ip[3]
  );
}

void PPPoSClass::IPAddressToLwIP(const IPAddress &arduino_ip, ip4_addr_t &lwip_ip) {
  IP4_ADDR(&lwip_ip,
    arduino_ip[0],
    arduino_ip[1],
    arduino_ip[2],
    arduino_ip[3]
  );
}

// Global instance of the library (for simple access from the sketch)
PPPoSClass PPPoS;
