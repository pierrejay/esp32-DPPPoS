# Start pppd (on pppd1 here)
# - Interface IP address (gateway) : 10.0.0.1
# - ESP32 IP address (client) : 10.0.0.2
sudo pppd /dev/ttyUSB0 115200 debug noauth local updetach unit 1 nobsdcomp novj nocrtscts 10.0.0.1:10.0.0.2

# Enable IP forwarding
# To make it permanent, add in /etc/sysctl.conf: net.ipv4.ip_forward = 1
sudo sysctl -w net.ipv4.ip_forward=1

# Enable NAT masquerading to have internet access from the ESP32
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE

## OPTIONAL: Route port 80 of ESP32 (10.0.0.2) to Tailscale only
# Only allow traffic from Tailscale to the ESP32 on port 80
# sudo iptables -t nat -A PREROUTING -i tailscale0 -p tcp -d YOUR_TAILSCALE_IP --dport 80 -j DNAT --to-destination 10.0.0.2:80
## Allow traffic forward for this connection
# sudo iptables -A FORWARD -i tailscale0 -p tcp -d 10.0.0.2 --dport 80 -j ACCEPT
# sudo iptables -A FORWARD -o tailscale0 -p tcp -s 10.0.0.2 --sport 80 -j ACCEPT

# Save iptables rules
sudo iptables-save | sudo tee /etc/iptables.rules

# Advertise the 10.0.0.0/24 network on the Tailnet
sudo tailscale up --advertise-routes=10.0.0.0/24
# You must activate the local route in the Tailscale admin console