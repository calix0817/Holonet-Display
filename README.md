# Imperial Holonet Display 🛸

A dual-screen tactical homelab dashboard & Star Wars planetary chronometer driven by an **ESP8266** microcontroller and two **0.96" I2C SSD1306 OLED displays** (128x64).

Data is aggregated by **Home Assistant** and published over **MQTT** as a single lightweight JSON payload.

---

## 🖥️ Screen Overview

### Screen 1: Environmental & Chrono (I2C: 0x3C)
* **Local Time & Date:** Standard 12-hour clock with synchronized NTP time.
* **Local Weather:** Current temperature, condition text, and geometric icon.
* **Galactic Standard Time:** 24-hour military precision timestamp (`HH:MM:SS`) & Czerka standard date (`YY.MM.DD`) rendered in authentic **Aurebesh** font.
* **System: Hoth:** Planetary weather report with sub-zero forecasts.
* **System: Tatooine:** Planetary weather report with desert forecasts.

### Screen 2: Tactical Homelab Telemetry (I2C: 0x3D)
* **Holocron (Windows 2025 Server):** Side-by-side CPU & RAM mini gauges, GPU load bar, and C: / D: storage capacity indicators.
* **Scarif (Synology NAS):** CPU & RAM gauges with full-width Volume 2 & Volume 3 utilization bars.
* **Unifi UDM:** CPU/RAM gauges and directional WAN traffic glyphs (`↓ RX` and `↑ TX`).
* **Kuma Alerts (Docker Monitoring):**
  * *Nominal:* Displays a rounded `ALL NOMINAL` fleet badge.
  * *Incident:* Inverts header into a high-visibility `ALERT: X DOWN` badge with a framed scrolling marquee ticker. Dynamic rotation hold prevents switching screens until the ticker has fully traversed.
* **Saber Forge (Bambu P1S 3D Printer):** Print status, remaining time in minutes, and a 14px full-width progress gauge.
* **Proxmox Imperial Fleet:** 3-node cluster grid for **Avenger (AV)**, **Devastator (DV)**, and **Executor (EX)** with live status pips (`●` Online / `○` Offline) and dual CPU/RAM meters.
* **Plex Transmissions:** Big-number active stream counter.

---

## ⚡ Key Features

* **400 kHz Fast I2C Bus:** Rapid dual-frame rendering (~23ms per display) for buttery smooth marquee scrolling.
* **Hardware Sleep & Wake:** Automatically shuts off OLED panel pixels (`SSD1306_DISPLAYOFF`) when your office is unoccupied via Home Assistant / Ecobee occupancy.
* **Emergency Outage Jump:** Immediately wakes displays and jumps straight to the downed container/node upon failure.
* **Persistent Alert Pip:** Blinking `[!]` indicator in the top-right header across both displays whenever an incident is active.
* **Night Mode:** Automatic hardware contrast dimming (`display.dim(true)`) between 11:00 PM and 6:00 AM.
* **Wireless OTA Flashing:** Dual-screen animated progress bar for over-the-air firmware updates via `ArduinoOTA`.
* **In-Universe Boot Splash:** Imperial boot sequence displaying your assigned IP address upon connection.

---

## 🔌 Hardware & Wiring

* **MCU:** ESP8266 (NodeMCU v2, ESP-12E, or WeMos D1 Mini)
* **Displays:** Two 0.96" Monochrome I2C SSD1306 OLED Displays (128x64)
  * Display 1 Address: `0x3C` (Factory Default)
  * Display 2 Address: `0x3D` (Resistor jumper moved on back of OLED module from 0x3C to 0x3D)

| Display Pin | ESP8266 Pin | Notes |
| :--- | :--- | :--- |
| **VCC** | `3V3` | 3.3V power |
| **GND** | `GND` | Ground |
| **SDA** | `D2` (GPIO 4) | Shared I2C Data bus |
| **SCL** | `D1` (GPIO 5) | Shared I2C Clock bus |

---

## 🖨️ 3D Printable Enclosure

Printable STL files designed specifically for housing the dual OLEDs and ESP8266 are available in the [`3D Model/`](3D%20Model/) directory:

* [`Holonet_V3_Body.stl`](3D%20Model/Holonet_V3_Body.stl) - Main console chassis holding the dual 0.96" SSD1306 displays and microcontroller.
* [`Holonet_V3_Backplate.stl`](3D%20Model/Holonet_V3_Backplate.stl) - Rear enclosure backplate with cable routing.

### Print Recommendations:
* **Material:** PLA, PETG, or ABS (Matte Black, Imperial Gray, or White for the Star Wars terminal look).
* **Layer Height:** 0.16mm – 0.20mm
* **Infill:** 15% – 20% (Gyroid or Grid)
* **Supports:** None or minimal depending on print orientation.

---

## 🚀 Getting Started

### 1. Clone the repository
```bash
git clone https://github.com/your-username/holonet-display.git
cd holonet-display
```

### 2. Configure Credentials
Copy the example configuration file and enter your WiFi and MQTT credentials:
```bash
cp config.example.h config.h
```
Edit `config.h`:
```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_SERVER = "192.168.1.100";
const char* MQTT_USER = "YOUR_MQTT_USERNAME";
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";
```
*(Note: `config.h` is excluded by `.gitignore` so your secrets will never be committed).*

### 3. Build & Flash (PlatformIO)
Open the project in **VS Code** with the **PlatformIO IDE** extension installed:
1. Connect your ESP8266 via USB.
2. Click **Upload (`→`)** on the bottom status bar.
3. Open the **Serial Monitor (`🔌`)** at `115200` baud.

---

## 📡 Home Assistant MQTT Payload

Publish a JSON payload to `displays/Holonet` every 10–30 seconds:

```json
{
  "holocron_cpu": 18,
  "holocron_mem": 42,
  "holocron_gpu": 5,
  "holocron_c": 75,
  "holocron_d": 40,
  "scarif_cpu": 12,
  "scarif_mem": 38,
  "scarif_vol2": 62,
  "scarif_vol3": 45,
  "unifi_cpu": 24,
  "unifi_mem": 55,
  "unifi_rx": "142.5",
  "unifi_tx": "18.2",
  "kuma_down": 0,
  "kuma_list": "",
  "p1s_status": "IDLE",
  "p1s_prog": 0,
  "p1s_rem": 0,
  "px_av_cpu": 22,
  "px_av_mem": 58,
  "px_av_stat": "online",
  "px_dv_cpu": 14,
  "px_dv_mem": 44,
  "px_dv_stat": "online",
  "px_ex_cpu": 8,
  "px_ex_mem": 32,
  "px_ex_stat": "online",
  "plex_streams": 1,
  "wx_home_temp": 72.0,
  "wx_home_code": 0,
  "wx_hoth_temp": -14.0,
  "wx_hoth_code": 71,
  "wx_tat_temp": 104.0,
  "wx_tat_code": 0
}
```

### Remote Power Toggle
* Publish `"ON"` or `"OFF"` to `displays/Holonet/power`
* Or include `"display_power": true` in the JSON payload above.

