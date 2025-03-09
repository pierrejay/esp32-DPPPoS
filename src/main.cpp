#include <Arduino.h>
#include "DPPPoS.h"

#define USB Serial
#define UART Serial1

#define UART_TX_PIN D7
#define UART_RX_PIN D6

// DPPPoS instance & custom settings
DPPPoS PPPoS;
static constexpr uint32_t PPPOS_BAUDRATE = 1500000;
static const IPAddress PPPOS_GATEWAY(10, 0, 0, 1);
static const IPAddress PPPOS_DNS(8, 8, 8, 8);
static constexpr uint32_t PPPOS_GATEWAY_PORT = 8080;

void setup() {

  // Initialize UART for PPP connection
  UART.setTxBufferSize(DPPPoS::UART_TX_BUFFER_SIZE);
  UART.setRxBufferSize(DPPPoS::UART_RX_BUFFER_SIZE);
  UART.begin(PPPOS_BAUDRATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  // Custom IP configuration
  DPPPoS::IPConfig config = {
    .gateway = PPPOS_GATEWAY,
    .dns = PPPOS_DNS
  };

  // Initialize with the custom configuration
  if (!PPPoS.begin(UART, &config)) {
    while (true) {
      delay(1000);
    }
  }
}

void loop() {

  if (PPPoS.connected()) {
    // Do something
  } 
  else {
    // Handle disconnection
  }

}