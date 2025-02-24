# Start pppd (on pppd1 here)
# - Interface IP address (gateway) : 10.0.0.1
# - ESP32 IP address (client) : 10.0.0.2
sudo pppd /dev/ttyUSB0 115200 debug noauth local updetach unit 1 nobsdcomp novj nocrtscts 10.0.0.1:10.0.0.2

# Enable IP forwarding
# To make it permanent, add in /etc/sysctl.conf: net.ipv4.ip_forward = 1
sudo sysctl -w net.ipv4.ip_forward=1

# Enable NAT masquerading to have internet access from the ESP32
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE

# Save iptables rules
sudo iptables-save | sudo tee /etc/iptables.rules

# Advertise the 10.0.0.0/24 network on the Tailnet
sudo tailscale up --advertise-routes=10.0.0.0/24
# You must activate the local route in the Tailscale admin console
