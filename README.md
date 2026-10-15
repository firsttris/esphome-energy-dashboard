<div align="center">

# ⚡ ESPHome Energy Dashboard

**Visualize Your Home's Energy Flow in Real-Time!**

<img src="docs/demo.gif" width="360" alt="Energy Dashboard: start animation and live energy flow">

[![ESPHome](https://img.shields.io/badge/ESPHome-Compatible-blue?logo=esphome)](https://esphome.io/)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Integration-41BDF5?logo=homeassistant)](https://www.home-assistant.io/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Build](https://github.com/firsttris/esphome-energy-dashboard/actions/workflows/build.yml/badge.svg)](https://github.com/firsttris/esphome-energy-dashboard/actions/workflows/build.yml)

A modular ESPHome project that shows your home's live energy flow on the [Guition-ESP32-S3-4848S040](https://devices.esphome.io/devices/guition-esp32-s3-4848s040/) display: solar, grid, battery, hot water, gas and water, with data from Home Assistant.

---

</div>

| Midday · feed-in | Battery full | Evening · battery | Night · grid | Today |
|:---:|:---:|:---:|:---:|:---:|
| <img src="docs/mittag.png" width="170"> | <img src="docs/voll.png" width="170"> | <img src="docs/abend.png" width="170"> | <img src="docs/nacht.png" width="170"> | <img src="docs/heute.png" width="170"> |

<sub>Screenshots and GIF are rendered with example values by CI from the desktop preview (`preview/`).</sub>

## 📱 Features

- **🔄 Live energy flow**: Glowing comets travel along the lines – more and faster comets the more power flows. Idle lines fade into the background
- **🏠 Home in the center**: Live consumption in large digits, a ring showing where today's energy came from (PV / battery / grid) and today's self-sufficiency
- **🔋 Liquid battery**: The state of charge is a moving wave, bubbles rise while charging, plus a forecast like *full at ~14:30* or *lasts until ~05:40*
- **☀️ Sun arc**: The sun (or moon) moves across the sky between sunrise and sunset; the ring around PV shows today's yield against the PV forecast
- **🎨 Mood**: The sky turns golden while feeding in, magenta on battery and blue on grid power
- **💶 Money**: How much your PV saved today, based on your electricity prices
- **💡 Hints**: e.g. *2.2 kW surplus · good moment for the washing machine*
- **♨️ Hot water**: Tank colored by temperature, sparks rise while the heating rod runs
- **🎉 Moments**: A short particle burst when the battery reaches 100 %
- **📈 Page "Today"**: PV, consumption and battery over the day (15-min values, kept across reboots)
- **⚡ Smooth on the ESP32-S3**: Glow and masks are pre-rendered images, only small objects move. An FPS counter (fps · ms per frame · share of screen redrawn) can be shown bottom left
- **📡 Offline Indicator**: A small cloud icon shows when Home Assistant is disconnected

## 📋 Prerequisites

| Requirement | Description |
|-------------|-------------|
| **🔧 Hardware** | [Guition-ESP32-S3-4848S040](https://devices.esphome.io/devices/guition-esp32-s3-4848s040/) (ESP32-S3 with 4.8" IPS touch display) |
| **🏠 Home Assistant** | With ESPHome Integration for sensor data |
| **💻 Software** | ESPHome (local or Docker/Podman) |
| **🔌 Cable** | USB cable for initial flashing |

---

## 🚀 Installation and Setup

### 📝 Step 1: Clone the Repository

```bash
git clone https://github.com/firsttris/esphome-energy-dashboard.git
cd esphome-energy-dashboard
```

### 🔐 Step 2: Configure Secrets

Copy the example and fill in your own values:

```bash
cp secrets.example.yaml secrets.yaml
```

```yaml
wifi_ssid: "Your_WiFi_Name"
wifi_password: "Your_WiFi_Password"
api_key: "Your_ESPHOME_API_Key"          # Generate a secure key
fallback_password: "Your_Fallback_Password"  # Password of the fallback hotspot
```

> **💡 Important**: Use a strong API key (e.g., 32 characters long). You can generate one with:
> ```bash
> openssl rand -base64 32
> ```

### ⚙️ Step 3: Customize Entity IDs

Customize the project to your requirements.
Ensure that the entity IDs in [sensors/homeassistant.yml](sensors/homeassistant.yml) match your HA sensors:

**Daily values** (shown on the dashboard):

| ID | Meaning | Example Entity ID |
|----|---------|-------------------|
| `solar_power` | ☀️ Solar production today (kWh) | `sensor.deye_wechselrichter_deye_tagliche_produktion` |
| `grid_power` / `grid_feed_in` | 🔌 Grid import / feed-in today (kWh) | `sensor.deye_wechselrichter_deye_taglich_energie_bezogen` |
| `home_power` | 🏠 Home consumption today (kWh) | `sensor.strom_tagesverbrauch` |
| `battery_soc` | 🔋 Battery state of charge (%) | `sensor.deye_wechselrichter_deye_batterie_soc` |
| `battery_charge` / `battery_discharge` | 🔋 Battery charged / discharged today (kWh) | `sensor.deye_wechselrichter_deye_tagliche_batterie_ladung` |
| `gas_power` / `water_power` | 🔥💧 Gas / water today (m³) | `sensor.gas_tagesverbrauch` |
| `heating_rod_daily_energy` | ♨️ Heating rod today (kWh) | `sensor.ac_elwa_2_energy_consumption_daily` |
| `solar_power_2` | ☀️ Second inverter today (kWh), optional | `sensor.hoymiles_tagesertrag` |
| `pv_forecast_today` / `pv_forecast_tomorrow` | ☀️ PV forecast (kWh), e.g. Forecast.Solar | `sensor.pv_prognose_heute` |
| `boiler_temperature` | ♨️ Hot water temperature (°C) | `sensor.ac_elwa_2_temperatur_1` |
| `sun_next_rising` / `sun_next_setting` | 🌅 Sun arc (attributes of `sun.sun`) | `sun.sun` |

**Current power** (drives the flow animations and the power table):

| ID | Meaning | Sign convention |
|----|---------|-----------------|
| `current_solar_power` | ☀️ PV power (W) | |
| `current_grid_power` | 🔌 Grid power (W) | **positive = import, negative = feed-in** |
| `current_battery_power` | 🔋 Battery power (W) | **positive = discharging, negative = charging** |
| `current_home_consumption` | 🏠 Home consumption (W) | |
| `current_heating_rod_power` | ♨️ Heating rod (W) | |

> **⚠️ Sign conventions:** If your inverter reports grid or battery power the other way round, add a `multiply: -1` filter to that sensor.

> **🔀 How the flows are split:** The animations use PV, grid, battery and home power (`current_home_consumption`). If the home sensor has no value, home consumption is derived from the energy balance. Power is split the way an inverter prioritizes it: PV supplies the home first, then the battery, and the rest is fed into the grid. The battery supplies the home first and feeds any surplus into the grid. The grid covers the rest of the home consumption and then charges the battery. The grid ↔ battery line shows both directions: white when the grid charges the battery, pink and reversed when the battery feeds into the grid.

> **✅ Note**: The dashboard works even if some sensors don't exist – the loading screen waits at most 10 s for missing sensors.

#### 📊 No daily sensors yet? Create utility meters

The dashboard shows **today's** values, but most meters and inverters only report a running total. Home Assistant's Energy Dashboard does not expose its daily values to other integrations, so add a [utility meter](https://www.home-assistant.io/integrations/utility_meter/) for each total that resets every day. Add it to `configuration.yaml`, or under *Settings → Devices & Services → Helpers → Utility Meter* (meter reset cycle: daily):

```yaml
utility_meter:
  strom_tagesverbrauch:
    source: sensor.stromzaehler_total      # your total meter (kWh)
    cycle: daily
  gas_tagesverbrauch:
    source: sensor.gaszaehler_total        # your gas meter (m³)
    cycle: daily
```

This creates `sensor.strom_tagesverbrauch` and `sensor.gas_tagesverbrauch`, which you then enter in [sensors/homeassistant.yml](sensors/homeassistant.yml). Many inverters (e.g. Deye) already provide daily values for PV, grid and battery.

### 🎛️ Optional: Tweak Behaviour

The `substitutions` at the top of [main.yml](main.yml) control the behaviour:

| Substitution | Default | Description |
|--------------|---------|-------------|
| `flow_threshold_w` | `20` | Minimum power (W) before a flow is animated |
| `flow_full_speed_w` | `5000` | Power (W) at which the animation reaches full speed |
| `battery_capacity_kwh` | `9.0` | Usable battery capacity for *full at* / *lasts until* |
| `battery_reserve_soc` | `20` | Minimum state of charge (%) the battery discharges to |
| `price_import` / `price_export` | `0.2957` / `0.0786` | Electricity price and feed-in tariff (€/kWh) for *saved today* |
| `show_fps` | `true` | FPS counter bottom left: fps · ms per frame (copy to display) · share of the screen redrawn |

### ⚡ Step 4: Compile and Flash Firmware

Connect your ESP32-S3 via USB and flash the firmware using local ESPHome:

```bash
esphome run main.yml --device=/dev/ttyUSB0
```

> **💡 Note:** Replace `/dev/ttyUSB0` with your device path (see troubleshooting below)

<details>
<summary><b>📚 Important Notes & Troubleshooting</b></summary>

#### 📍 Device Paths by OS

| OS | Typical Paths |
|----|--------------|
| 🐧 **Linux** | `/dev/ttyUSB0`, `/dev/ttyACM0`, `/dev/ttyUSB1` |
| 🍎 **macOS** | `/dev/cu.usbserial-*`, `/dev/cu.wchusbserial*` |
| 🪟 **Windows** | `COM3`, `COM4`, etc. |

**Check available ports:**
- Linux/macOS: `ls /dev/tty*`
- Windows: Device Manager

---

#### 🔐 USB Permission Issues

**Standard Linux - Add user to dialout group:**
```bash
sudo usermod -a -G dialout $USER
# Then log out and back in
```

**Fedora Atomic/Bazzite with rootless Docker/Podman:**

The dialout group doesn't work reliably on immutable systems. You need to fix permissions before each flash:

```bash
# Check permissions
ls -la /dev/ttyUSB0
# Output: crw-rw----. 1 root dialout 166, 0 ...

# Fix temporarily (resets on USB reconnect)
sudo chmod 666 /dev/ttyUSB0

# If using Docker/Podman, restart the container
docker-compose restart
```

> ⚠️ **Note:** You need to run `sudo chmod 666` each time you reconnect the USB device. If using Docker/Podman, restart the container after fixing permissions.

</details>

---

<details>
<summary><b>🐳 Alternative: Using Docker/Podman</b></summary>

> ⚠️ **Important:** When using Docker/Podman (rootless), you need to fix USB permissions before flashing.

```bash
# 1. Start container
docker-compose up -d

# 2. Flash the firmware
docker-compose exec esphome esphome run main.yml --device=/dev/ttyUSB0
```

> **📌 Note:** The docker-compose.yml mounts the config directory and runs in host network mode for easy access.

</details>

---

<details>
<summary><b>🌐 Alternative: Web Dashboard (GUI)</b></summary>

Choose between local or hosted dashboard:

**Docker Dashboard:**
```bash
# Start container if not already running
docker-compose up -d

# Start the dashboard
docker-compose exec esphome esphome dashboard /config
```
Then open **http://localhost:6052** in your browser.

**Local Dashboard** (requires local ESPHome):
```bash
esphome dashboard .
```
Then open **http://localhost:6052** in your browser and use the web interface.

**Browser flashing** (no USB drivers or ESPHome needed on the flashing computer):

[ESPHome Web](https://web.esphome.io/) flashes a finished firmware file, it cannot compile `main.yml` itself.

1. Compile once: `esphome compile main.yml` (or in the dashboard: *Install → Manual download → Factory format*)
2. Visit **https://web.esphome.io/** in Chrome or Edge
3. Click "Connect" and select your ESP32-S3
4. Click "Install" and choose `.esphome/build/energy-dashboard/build/firmware.factory.bin`


</details>

### 🏠 Step 5: Add to Home Assistant

After the first boot the device connects to your WiFi. Home Assistant usually discovers it automatically under *Settings → Devices & Services*. When asked for the encryption key, enter the `api_key` from your `secrets.yaml`. The dashboard loads its data as soon as Home Assistant is connected.

### 🔄 Step 6: Updates over WiFi

The USB cable is only needed for the first flash. After that, updates are installed over WiFi (OTA):

```bash
git pull
esphome run main.yml --device=energy-dashboard.local
```

> **💡 Tip:** If `energy-dashboard.local` cannot be resolved, use the device's IP address instead (shown in your router or in Home Assistant). The backlight turns off while the update is installed.

---

## 📱 Usage

After successful startup, the display shows a loading screen followed by the main dashboard. 

### 🎯 Navigation

👆 **Touch the screen** to switch between views:

| View | Description |
|------|-------------|
| **⚡ Energy flow** | Live power of PV, grid, battery, home and heating rod with animated flows |
| **📈 Today** | Today's totals and the day's course of PV, consumption and battery |

### 🧭 Reading the Dashboard

| Element | Meaning |
|---------|---------|
| **Large numbers** | Current power. Grid: purple = feed-in, blue = import |
| **Comets** | Current power flow; more and faster comets = more power. None = less than `flow_threshold_w` |
| **Ring around the home** | Where today's consumption came from: PV (yellow), battery (pink), grid (blue) |
| **Ring around PV** | Today's yield compared to the forecast |
| **Battery wave** | State of charge, `+` charging / `−` discharging |
| **Sky color** | Golden = feed-in, magenta = battery supplies the home, blue = grid import |
| **Red cloud (top)** | Home Assistant is not connected, values may be outdated |

### 💡 Additional Features

- **🌟 Backlight Control**: Automatically turns off during OTA updates
- **📶 Fallback Hotspot**: If WiFi is unreachable, the device opens the hotspot `Energy-Dashboard Fallback`
- **🏠 Home Assistant Integration**: Control backlight via Home Assistant
- **🔄 Real-time Updates**: Data refreshes automatically from your sensors

## ⚙️ Configuration Structure

The configuration is modular for easy customization:

| File | Description |
|------|-------------|
| 📄 [main.yml](main.yml) | Main configuration file |
| 🔧 [base/hardware.yml](base/hardware.yml) | Hardware-specific settings (display, I2C, SPI) |
| 🌐 [base/network.yml](base/network.yml) | WiFi, API, OTA configuration |
| 💡 [base/backlight.yml](base/backlight.yml) | Backlight |
| 📊 [sensors/homeassistant.yml](sensors/homeassistant.yml) | Home Assistant sensor imports |
| 🎨 [ui/scene.h](ui/scene.h) | The whole UI in C++ (LVGL 9): nodes, flows, battery, sky, page "Today", FPS |
| 🧮 [ui/dashboard.h](ui/dashboard.h) | Logic without LVGL: flow split, formatting, battery forecast |
| 🔗 [ui/bindings.yml](ui/bindings.yml) | Passes sensor values to the UI |
| 🖼️ [images/](images/) | Pre-rendered glow, sun, moon and battery images ([tools/make_images.py](tools/make_images.py)) |

### 📂 File Structure

```
.
├── main.yml                    # Main ESPHome configuration
├── secrets.yaml                # Your credentials (not in git)
├── secrets.example.yaml        # Template for secrets.yaml
├── docker-compose.yml          # Docker setup
├── base/
│   ├── hardware.yml            # Display & hardware config
│   ├── network.yml             # Network settings
│   └── backlight.yml           # Backlight
├── sensors/
│   └── homeassistant.yml       # HA sensor definitions (entity IDs)
├── ui/
│   ├── scene.h                 # UI (LVGL): energy flow page, page "Today", FPS
│   ├── dashboard.h             # Logic: flow split, formatting, battery forecast
│   ├── bindings.yml            # Sensor values -> UI
│   ├── fonts.yml               # Fonts (Barlow Semi Condensed, MDI icons)
│   ├── images.yml              # Pre-rendered images
│   ├── layout.yml              # LVGL setup, pages, timers
│   └── loading.yml             # Loading screen & connection status
├── images/                     # Glow, sun, moon, battery wave and mask (PNG)
├── tools/make_images.py        # Generates images/
├── preview/                    # Desktop preview (SDL) with example values
├── fonts/                      # Font files
└── docs/                       # Screenshots (rendered by CI)
```

## 🖥️ Desktop Preview

`preview/` builds the dashboard for the PC (SDL) with example values for four scenes (midday, battery full, evening, night). It needs ESPHome, SDL2, Xvfb, xdotool, ImageMagick and ffmpeg:

```bash
sh preview/render.sh          # screenshots in preview/shots/
GIF=1 sh preview/render.sh    # plus an animated GIF
```

The workflow [screenshots.yml](.github/workflows/screenshots.yml) does the same on every push to `main` and commits the images in `docs/`.

## 📚 Further Resources

| Resource | Description |
|----------|-------------|
| 📖 [ESPHome Documentation](https://esphome.io/) | Official ESPHome documentation |
| 🏠 [Home Assistant](https://www.home-assistant.io/) | Home automation platform |
| 🖥️ [Guition ESP32-S3-4848S040](https://devices.esphome.io/devices/guition-esp32-s3-4848s040/) | Device information |
| 🔋 [Energy Dashboard](https://www.home-assistant.io/docs/energy/) | Home Assistant Energy Dashboard |

---

<div align="center">

⭐ Like the ESPHome Energy Dashboard? A [star on GitHub](https://github.com/firsttris/esphome-energy-dashboard) helps others find it.<br>
🐛 [Report a bug](https://github.com/firsttris/esphome-energy-dashboard/issues/new) · 💡 [Request a feature](https://github.com/firsttris/esphome-energy-dashboard/issues/new)

<sub>License: <a href="LICENSE">MIT</a> · © Tristan Teufel and contributors</sub>

</div>
