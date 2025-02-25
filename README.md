# ESP32 DPPPoS Library

## Overview

Implements a direct Point-to-Point Protocol (PPP) connection between an ESP32 and a host via UART. Unlike IDF PPPoS examples that require a GSM modem, it can establish a direct serial connection to a Linux machine natively supporting PPP, leveraging its routing and NAT capabilities to provide network connectivity to the ESP32.

This library seamlessly integrates with ESP32's native TCP/IP stack, making the PPP connection appear as a standard network interface (similar to WiFi or Ethernet). It is possible to usefamiliar Arduino networking APIs (`NetworkClient`, etc.) and popular libraries like `ESPAsyncWebServer` without any modifications.

A cool feature is the ability to expose the ESP32 securely to the Internet through Tailscale running on the Linux host. This provides a simple yet secure way to create remotely accessible IoT devices without complex VPN setups or cloud service dependencies.

Builds upon the concepts from [ESP32-PPPos-TLS](https://github.com/hussainhadi673/ESP32-PPPos-TLS) by hussainhadi673, reimplemented as a standalone Arduino library without TLS. For most remote control use cases, the security provided by Tailscale's encrypted mesh network is sufficient, eliminating the need for additional TLS overhead.

## Prerequisites

- ESP32 MCU
- Arduino Core ESP32 v3.0.0 or higher
- Linux host machine
- UART connection between ESP32 and Linux host

## Installation

1. Clone or download the library to your Arduino project's `lib` directory
2. Include the header in your project:
```cpp
#include "PPPoS.h"
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
// Configure Serial for PPPoS
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

### 2. Monitor Connection Status

```cpp
void loop() {
    if (PPPoS.connected()) {
        // Your network code here
    } else {
        Serial.println("Waiting for PPP connection...");
        delay(1000);
    }
}
```

## Linux Host Configuration

### 1. PPP Setup Script

Save the following script as `setup.sh`:

```bash
# Start pppd daemon
sudo pppd /dev/ttyUSB0 115200 debug noauth local updetach unit 1 \
    nobsdcomp novj nocrtscts 10.0.0.1:10.0.0.2

# Enable IP forwarding
sudo sysctl -w net.ipv4.ip_forward=1

# Setup NAT for Internet access
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE

# Save iptables rules
sudo iptables-save | sudo tee /etc/iptables.rules
```

### 2. Remote Access with Tailscale (Optional)

To enable secure remote access:

```bash
# Advertise the PPP network on Tailscale
sudo tailscale up --advertise-routes=10.0.0.0/24
```

Note: Route approval in Tailscale admin console is required.

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

## Error Handling

The library includes automatic error recovery:

- Watchdog task monitoring connection status
- Automatic reconnection attempts
- Connection state management
- ESP32 PPP driver error handling

## Performance Considerations

### Buffer Sizes

Default UART buffer sizes (configurable):
```cpp
static constexpr uint16_t UART_RX_BUFFER_SIZE = 2048;
static constexpr uint16_t UART_TX_BUFFER_SIZE = 2048;
```

### Task Priorities

The library creates two FreeRTOS tasks:
```cpp
static constexpr uint8_t NET_WATCHDOG_TASK_PRIORITY = 1;
static constexpr uint8_t LOOP_TASK_PRIORITY = 5;
```

## Security Considerations

- The PPP connection itself is unencrypted
- When used with Tailscale, the connection is secured by Tailscale's encryption
- The library does not implement TLS, use a secure client if required

## License

MIT License