
# Trafix - Technical Features

> That sounds like an awesome project — a kind of all-in-one system monitoring dashboard for Linux. 
> Here's a well-organized list of important system data you might want to include, grouped by category:

1. System Overview
    Hostname
    OS & Kernel version
    Uptime
    System load averages (1, 5, 15 min)
    Current logged-in user(s)

2. CPU
    Per-core usage (percentage)
    Average usage (all cores)
    CPU temperature (if available)
    Frequency (per core or average)
    Top CPU-consuming processes

3. Memory
    Total / Used / Free RAM
    Swap usage
    Memory usage percentage
    Top memory-consuming processes

4. Disk / Storage
    Disk usage per mounted volume
    I/O stats (read/write speed)
    Available inodes
    Disk temperature (if available)

5. Network
    IP address (IPv4 & IPv6)
    MAC address
    Gateway
    DNS servers
    Interface status (up/down)
    Real-time upload/download speed
    Total data sent/received
    Wi-Fi signal strength, SSID, bitrate, frequency (if on Wi-Fi)

5. 1.  Active connections (count or list)

6. VPN
    VPN connection status
    VPN IP address (if connected)
    Interface name (e.g., tun0)
    Connected server location/IP

7. Processes
    Total number of processes
    Running vs sleeping/stopped
    Top processes by CPU and memory
    Zombie processes count

8. Power (for laptops)
    Battery percentage and status (charging/discharging)
    Power consumption (Watt, if available)

9. Other (Optional / Advanced)
    GPU usage (if applicable)
    Temperature sensors (using lm-sensors)
    Fan speed (if accessible)
    System logs (tail of syslog or dmesg)
    Scheduled tasks (cron jobs)
    Security alerts (e.g., failed SSH attempts)


## 
- To stress all CPU cores or Memory, run multiple instances in background:
```sh
	stress-ng --cpu 4 --timeout 30s
	stress-ng --vm 2 --vm-bytes 1G --vm-method all --verify --timeout 30s
```
