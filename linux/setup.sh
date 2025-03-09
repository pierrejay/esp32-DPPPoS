
########################################################
#                 BASIC SETUP                          #
########################################################

# Enable UART pins on your hardware
# Here, we're using UART1 on a MilkV Duo S (typically not needed on Raspberry Pi)
sudo cvi_pinmux -w JTAG_CPU_TMS/UART1_TX
sudo cvi_pinmux -w JTAG_CPU_TCK/UART1_RX

# Start pppd
sudo pppd /dev/ttyS1 \      # UART serial port
       1500000 \            # Default baudrate: 1500000 bauds
       debug \              # Enable debug mode
       noauth \             # Disable authentication
       local \              # Indicate that this machine is local
       updetach \           # Detach the process from the terminal upon connection
       unit 1 \             # Use ppp1 interface
       nobsdcomp \          # Disable BSD compression
       novj \               # Disable Van Jacobson compression
       nocrtscts \          # Disable RTS/CTS flow control
       10.0.0.1:10.0.0.2    # Gateway IP:ESP32 IP


########################################################
#         ENABLE INTERNET ACCESS FROM ESP32            #
########################################################

# Enable IP forwarding
# To make it permanent, add in /etc/sysctl.conf: net.ipv4.ip_forward = 1
sudo sysctl -w net.ipv4.ip_forward=1

# Enable NAT masquerading to have internet access from the ESP32
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE


########################################################
#          ROUTE TAILSCALE TRAFFIC TO ESP32            #
########################################################

# OPTION 1: redirect all port 80 traffic from Tailscale to the ESP32
# - Route port 80 of ESP32 (10.0.0.2) to Tailscale interface
# - Allow in/out traffic forwarding for this connection
# - Save iptables rules
sudo iptables -t nat -A PREROUTING -i tailscale0 -p tcp -d YOUR_TAILSCALE_IP --dport 80 -j DNAT --to-destination 10.0.0.2:80
sudo iptables -A FORWARD -i tailscale0 -p tcp -d 10.0.0.2 --dport 80 -j ACCEPT
sudo iptables -A FORWARD -o tailscale0 -p tcp -s 10.0.0.2 --sport 80 -j ACCEPT
sudo iptables-save | sudo tee /etc/iptables.rules

# OPTION 2: Use Tailscale route advertisement feature
# - Advertise the 10.0.0.0/24 network on the Tailnet
# (Local route must be enabled in the Tailscale admin console after sending the command)
sudo tailscale up --advertise-routes=10.0.0.0/24


########################################################
#          RUN EXAMPLE PYTHON "PING" SERVER            #
########################################################

# Launch a Python server in background mode to respond to HTTP GET requests
nohup sudo python3 -m http.server 8080 &