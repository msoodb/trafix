# trafix
Trafix - Network Monitoring Tool

Trafix is a lightweight, easy-to-use network monitoring tool for Linux. It helps you track active network connections, monitor bandwidth usage, and identify top traffic sources in real time, all from a simple command-line interface.
Key Features:

- Monitor Active Connections: View all active TCP/UDP connections with details such as local and remote addresses, ports, and connection states.
- Track Bandwidth Usage: Monitor incoming and outgoing network traffic in real-time.
- Top Talkers: Identify the processes or IP addresses consuming the most bandwidth.
- Alerts & Thresholds: Set custom thresholds for network usage and receive alerts when limits are exceeded.
- User-Friendly CLI: Simple and intuitive command-line interface with filtering, sorting, and customizable options.

Trafix is designed to be fast, efficient, and lightweight, with minimal system resource usage.


```sh
	make clean
	make
	bin/trafix
```


## Enhancement
Great question — your network dashboard can become super informative with just a few more additions. Here’s a list of useful and practical metrics you can display beyond just sent/received data and connection info:

🔌 Connection Info
    ✅ Active Interface Name (already have)
    ✅ IP Address (IPv4/IPv6) (already have)
    ✅ SSID (for Wi-Fi) (already have)
    ✅ VPN Interface & IP (already added)
    🆕 Gateway IP – via ip route | grep default
    🆕 DNS Servers – from /etc/resolv.conf or nmcli dev show

📶 Wi-Fi Specific (if connected via Wi-Fi)
    🆕 Signal Strength / Quality – via iwconfig or iw dev wlan0 link
    🆕 Link Speed / Bitrate – from iwconfig or iw
    🆕 Frequency / Channel – Wi-Fi band (2.4GHz vs 5GHz)
    🆕 MAC Address (your own interface) – from ip link show wlan0

🌐 General Network Stats
    🆕 Total Data Transferred (MB/GB) – cumulative from /proc/net/dev
    🆕 Packets Sent/Received
    🆕 Error Counts – dropped packets, errors per interface
    🆕 Interface Status – UP/DOWN status
    🆕 Interface Speed – via ethtool <iface> (requires root or capabilities)

🌍 External Info
    🆕 Public IP Address – via external services (e.g., curl ifconfig.me) (if internet available)
    🆕 Ping Latency to Gateway or Google – using ping and parsing result
    🆕 Download/Upload Speed – integrate speedtest-cli (or lib)

📡 Routing Info
    🆕 Default Gateway Interface – already noted, but can also show metric
    🆕 Routing Table Summary – list key routes from ip route

🧪 Advanced (Optional)
    🆕 Connected Devices in LAN – via ARP table (ip neigh) or nmap
    🆕 Open Ports / Listening Services – from ss -tuln
    🆕 Firewall Status – via ufw status, iptables, or nft

🧱 Example Section Titles You Can Add
    🧾 Interface Summary
    📶 Wireless Status
    🔌 Wired Connection
    🧭 Routing & Gateway
    🔒 VPN & Security
    📈 Traffic Statistics
    🌍 Public IP & Latency
