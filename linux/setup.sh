# Démarrer pppd
sudo pppd /dev/ttyUSB0 921600 debug noauth local updetach unit 1 nobsdcomp novj nocrtscts 10.0.0.1:10.0.0.2

# Activer l'IP forwarding
sudo sysctl -w net.ipv4.ip_forward=1

# Activer NAT
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o wlan0 -j MASQUERADE