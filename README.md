# ESP32 Direct PPPoS Arduino Library

## Overview

This Arduino library implements a direct Point-to-Point Protocol (PPP) connection between an ESP32 and a host via UART. Unlike IDF PPPoS examples that require a GSM modem, it can establish a direct serial connection to a Linux machine natively supporting PPP, leveraging its routing and NAT capabilities to provide network connectivity to the ESP32.

This library seamlessly integrates with ESP32's native TCP/IP stack, making the PPP connection appear as a standard network interface (similar to WiFi or Ethernet). It is possible to use the familiar Arduino networking APIs (`NetworkClient`, etc.) and popular libraries like `ESPAsyncWebServer` without any modifications.

A cool feature is the ability to expose the ESP32 securely to the Internet through Tailscale running on the Linux host. This provides a simple yet secure way to create remotely accessible IoT devices without complex VPN setups or cloud service dependencies.

This code builds upon the concepts from [ESP32-PPPos-TLS](https://github.com/hussainhadi673/ESP32-PPPos-TLS) by hussainhadi673, reimplemented as a standalone Arduino library without TLS. For most remote control use cases, the security can be implemented on the host side (e.g. Tailscale's encrypted mesh network), eliminating the need for additional TLS overhead.

## Prerequisites

- ESP32 MCU
- Arduino Core ESP32 v3.0.0 or higher
- Linux host machine
- UART connection between ESP32 and Linux host

## Installation

1. Clone or download the library (`lib/DPPPoS`) to your Arduino project's `lib` directory
2. Include the header in your project & declare an instance of the `DPPPoS` class:
```cpp
#include "DPPPoS.h"

DPPPoS PPPoS;
```

## Hardware Setup

### Required Connections

- Connect ESP32's UART `TX` pin to Linux host's `RX`
- Connect ESP32's UART `RX` pin to Linux host's `TX`
- Connect ESP32's `GND` to Linux host's `GND`

### Example Pin Configuration

```cpp
#define UART_TX_PIN D7
#define UART_RX_PIN D6
#define UART_BAUDRATE 115200
```

## Basic Usage

### 1. Initialize the Library

```cpp
// Configure Serial1 for PPPoS with improved buffer sizes 
// (2048 by default, useful as the default PPP MTU is 1500)
Serial1.setTxBufferSize(DPPPoS::UART_TX_BUFFER_SIZE);
Serial1.setRxBufferSize(DPPPoS::UART_RX_BUFFER_SIZE);
Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

// Optional: Custom IP configuration
PPPoSClass::IPConfig config = {
    .gateway = IPAddress(10, 0, 0, 1),  // PPP Gateway IP
    .dns = IPAddress(8, 8, 8, 8)        // DNS Server
};

// Initialize PPPoS
if (!PPPoS.begin(Serial1, &config)) {
    Serial.println("PPPoS initialization failed");
    while (true) delay(1000);
}
```

No call to `PPPoS.loop()` is required in your code, PPP data is automatically sent and received in the background by the `LoopTask` task (for better performance it can be pinned to core 0 if required).

### 2. Monitor Connection Status

```cpp
void loop() {
    if (PPPoS.connected()) {
        // Your network code here
    } else {
        // You can handle disconnection here
        Serial.println("Waiting for PPP connection...");
        delay(1000);
    }
}
```

## Linux Host Configuration

Refer to the `linux/setup.sh` script for guidelines on :
- Setting up the PPP connection
- Enabling internet access from the ESP32
- Routing external traffic to the ESP32

## Advanced Features

### Network Client Usage

The library allows transparent use of ESP32's TCP/IP stack. Example using a network client:

```cpp
NetworkClient client;
if (client.connect("google.com", 80)) {
    client.println("GET / HTTP/1.0");
    client.println("Host: google.com");
    client.println();
    
    while (client.connected()) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            Serial.println(line);
        }
    }
    client.stop();
}
```

### Web Server Implementation

Compatible with `ESPAsyncWebServer` for creating a web interface serving files or an HTTP API:

```cpp
AsyncWebServer server(80);

void setup() {
    // PPPoS initialization...

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Hello World");
    });
    server.begin();
}
```

### Example sketches

- [src/main.cpp](src/main.cpp) provides a minimal example of using the library with custom IP configuration
- [examples/main.cpp](examples/main.cpp) provides an example of using the library to send HTTP GET requests local and remote machines (Linux host + google.com) and exposes a basic web page to show performance metrics (GET request RTT). Follow instructions in the `setup.sh` script to setup the Python server on the host that will reply to GET requests + upload filesystem to ESP32 flash to get access to the web page.

## Debugging

Enable debug output by defining `PPPOS_DEBUG` (e.g. in your `platformio.ini` file):

```ini
build_flags = -D PPPOS_DEBUG
```

This will output detailed connection information and data hexdumps.

## Connection States

The library manages the following connection states:

- `CONNECTED`: Successfully connected to the network
- `CONNECTING`: Connection attempt in progress
- `CONNECTION_LOST`: Connection lost, will attempt reconnection
- `DISCONNECTED`: Disconnected from network

A dedicated task (`NetWatchdogTask`) monitors the connection status and attempts reconnection if the connection is lost.

## License

MIT License