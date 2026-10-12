# MyPi Companion — Example Macros for Raspberry Pi

58 macros in 5 menu groups. Import them via the GUI editor (`/macros`).

> **[TEMPLATE]** = the command is sent **without Enter** (`cr: false`) — edit the placeholders
> in the terminal before pressing Enter. Replace `<PLACEHOLDER>` with real values.

---

## 1. Network

| ID | Name | Command | CR |
|----|------|---------|----|
| 1 | Show network interfaces | `ip a` | ✓ |
| 2 | Show routing table | `ip route show` | ✓ |
| 3 | Show ARP table | `arp -n` | ✓ |
| 7 | Hostname + IP | `echo "Host: $(hostname)  IP: $(hostname -I)"` | ✓ |
| 6 | Show open ports | `ss -tulnp` | ✓ |
| 4 | Ping gateway | `ping -c 4 $(ip route | awk '/default/{print $3; exit}')` | ✓ |
| 5 | Show external IP | `curl -s ifconfig.me && echo` | ✓ |
| 8 | Ping 8.8.8.8 | `ping -c 4 8.8.8.8` | ✓ |

### WiFi (submenu)

| ID | Name | Command | CR |
|----|------|---------|----|
| 9  | Scan WiFi | `sudo iwlist wlan0 scan | grep -E 'ESSID|Signal level|Frequency'` | ✓ |
| 10 | List WiFi networks | `nmcli dev wifi list` | ✓ |
| 11 | WiFi interface info | `iwconfig wlan0 2>/dev/null || echo 'wlan0 not found'` | ✓ |
| 12 | WiFi signal strength | `cat /proc/net/wireless` | ✓ |
| 13 | WiFi active connection profile | `nmcli con show --active` | ✓ |

### Change IP — templates (submenu)

| ID | Name | Command | CR |
|----|------|---------|----|
| 41 | Set static IP [TEMPLATE] | `sudo ip addr add <IP/PREFIX> dev <INTERFACE>` | ✗ |
| 42 | Remove IP [TEMPLATE] | `sudo ip addr del <IP/PREFIX> dev <INTERFACE>` | ✗ |
| 43 | Set default gateway [TEMPLATE] | `sudo ip route add default via <GATEWAY_IP>` | ✗ |
| 46 | nmcli set IP [TEMPLATE] | `sudo nmcli con mod '<PROFILE>' ipv4.addresses <IP/PREFIX> ipv4.method manual` | ✗ |
| 47 | nmcli activate profile [TEMPLATE] | `sudo nmcli con up '<PROFILE>'` | ✗ |
| 44 | Restart NetworkManager | `sudo systemctl restart NetworkManager` | ✓ |
| 45 | Restart networking | `sudo systemctl restart networking` | ✓ |

---

## 2. Processes

| ID | Name | Command | CR |
|----|------|---------|----|
| 14 | Top CPU processes | `ps aux --sort=-%cpu | head -16` | ✓ |
| 15 | Top RAM processes | `ps aux --sort=-%mem | head -16` | ✓ |
| 16 | Process tree | `pstree -p | head -40` | ✓ |
| 17 | Show memory | `free -h` | ✓ |
| 18 | Show swap | `swapon --show; free -h | tail -1` | ✓ |
| 19 | Systemd failures | `systemctl --failed` | ✓ |
| 20 | Last 30 log lines | `journalctl -n 30 --no-pager` | ✓ |
| 21 | Kernel log USB/serial | `dmesg | grep -iE 'usb|tty' | tail -20` | ✓ |

---

## 3. System

| ID | Name | Command | CR |
|----|------|---------|----|
| 22 | Disk usage | `df -h` | ✓ |
| 23 | Home directory size | `du -sh /home/* 2>/dev/null` | ✓ |
| 24 | Uptime | `uptime` | ✓ |
| 25 | Kernel info | `uname -a` | ✓ |
| 26 | RPi model | `cat /proc/cpuinfo | grep -m1 Model` | ✓ |
| 27 | CPU temperature | `vcgencmd measure_temp` (fallback: `/sys/class/thermal`) | ✓ |
| 28 | CPU throttling | `vcgencmd get_throttled` | ✓ |
| 29 | Logged in users | `who` | ✓ |
| 30 | Login history | `last | head -10` | ✓ |
| 48 | Reboot Pi | `sudo reboot` | ✗ |
| 49 | Shutdown Pi | `sudo poweroff` | ✗ |

### Services — templates (submenu)

| ID | Name | Command | CR |
|----|------|---------|----|
| 50 | Systemd service status [TEMPLATE] | `systemctl status <SERVICE>` | ✗ |
| 51 | Start service [TEMPLATE] | `sudo systemctl start <SERVICE>` | ✗ |
| 52 | Restart service [TEMPLATE] | `sudo systemctl restart <SERVICE>` | ✗ |

---

## 4. USB & Hardware

| ID | Name | Command | CR |
|----|------|---------|----|
| 31 | List USB devices | `lsusb` | ✓ |
| 32 | USB tree view | `lsusb -t` | ✓ |
| 33 | USB dmesg log | `dmesg | grep -i usb | tail -20` | ✓ |
| 34 | Show serial ports | `ls /dev/ttyUSB* /dev/ttyACM* /dev/ttyS* 2>/dev/null` | ✓ |
| 35 | Block devices (disk+USB) | `lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT | grep -v loop` | ✓ |
| 36 | Mounted filesystems | `mount | grep -v 'tmpfs\|devtmpfs\|sysfs\|proc\|...'` | ✓ |
| 57 | I2C scan | `i2cdetect -y 1` | ✓ |
| 58 | GPIO status | `raspi-gpio get` | ✓ |

---

## 5. Packages & Dev

| ID | Name | Command | CR |
|----|------|---------|----|
| 37 | Update package list | `sudo apt update` | ✓ |
| 38 | Upgradable packages | `apt list --upgradable 2>/dev/null | head -20` | ✓ |
| 39 | Install package [TEMPLATE] | `sudo apt install -y <PACKAGENAME>` | ✗ |
| 40 | Remove package [TEMPLATE] | `sudo apt remove <PACKAGENAME>` | ✗ |
| 55 | Python3 version | `python3 --version` | ✓ |
| 56 | pip list installed | `pip3 list 2>/dev/null | head -20` | ✓ |
| 53 | SSH to host [TEMPLATE] | `ssh <USER>@<HOST>` | ✗ |
| 54 | Copy file SCP [TEMPLATE] | `scp <FILE> <USER>@<HOST>:<PATH>` | ✗ |

---

## Import Instructions

1. Open `http://192.168.4.1/macros` in your browser
2. Click **Import** (top right) and pick `example_macros.json` → imports the macro pool
3. Click **Import** again and pick `example_menus.json` → imports the menu structure
4. Click **Save all** (or Ctrl+S) — nothing is written to the ESP until you save
5. Alternatively: upload the files directly via `/upload` and name them `macros.json` / `menus.json`

> Note: If you already have your own `macros.json`/`menus.json` on the device, make a backup using
> the Export button in the GUI editor before overwriting them.
