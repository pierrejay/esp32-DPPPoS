# Démarrer pppd
sudo pppd /dev/ttyUSB0 115200 debug noauth local updetach unit 1 nobsdcomp novj nocrtscts 10.0.0.1:10.0.0.2

# Activer l'IP forwarding
sudo sysctl -w net.ipv4.ip_forward=1
# Pour le rendre permanent, ajouter dans /etc/sysctl.conf :
# net.ipv4.ip_forward = 1

# Activer NAT
# (a priori pas nécessaire car déjà activé par pppd)
# sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o wlan0 -j MASQUERADE

# Advertiser le réseau 10.0.0.0/24 sur le réseau Tailnet
sudo tailscale up --advertise-routes=10.0.0.0/24
# Il faut activer la route locale dans la console admin Tailscale

