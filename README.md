<div align="center">

# ⚡ ESPHome Energy Dashboard

**Visualize Your Home's Energy Flow in Real-Time!**

<img src="docs/screenshot.jpg" width="500" alt="Energy Dashboard Screenshot">

[![ESPHome](https://img.shields.io/badge/ESPHome-Compatible-blue?logo=esphome)](https://esphome.io/)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Integration-41BDF5?logo=homeassistant)](https://www.home-assistant.io/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A modular ESPHome project that visualizes daily energy distribution similar to Home Assistant, with real-time data on solar production, grid consumption/feed-in, battery status, and gas consumption, on the [Guition-ESP32-S3-4848S040](https://devices.esphome.io/devices/guition-esp32-s3-4848s040/) display.

---

</div>

## 📱 Features

- **🔄 Energy Flow Visualization**: See your energy distribution at a glance
- **☀️ Solar Production Monitoring**: Track your daily solar generation
- **🔋 Battery Status**: Real-time battery state of charge
- **📊 Dual Page Display**: 
  - Page 1: Daily accumulated energy consumption (Home Assistant style)
  - Page 2: Current load table with detailed breakdown
- **👆 Touch Interface**: Simply tap the screen to switch between views
- **🎨 Modular Design**: Easy to customize and extend

## 📋 Prerequisites

| Requirement | Description |
|-------------|-------------|
| **🔧 Hardware** | [Guition-ESP32-S3-4848S040](https://devices.esphome.io/devices/guition-esp32-s3-4848s040/) (ESP32-S3 with 4.8" IPS touch display) |
| **🏠 Home Assistant** | With ESPHome Integration for sensor data |
| **💻 Software** | ESPHome (local or Docker/Podman) |
| **🔌 Cable** | USB cable for initial flashing |

---

---

## 🚀 Installation and Setup

### 📝 Step 1: Clone the Repository

```bash
git clone https://github.com/firsttris/esphome-energy-dashboard.git
cd esphome-energy-dashboard
```

### 🔐 Step 2: Configure Secrets

Create or edit the `secrets.yaml` file and add your own values:

```yaml
wifi_ssid: "Your_WiFi_Name"
wifi_password: "Your_WiFi_Password"
api_key: "Your_ESPHOME_API_Key"  # Generate a secure key
```

> **💡 Important**: Use a strong API key (e.g., 32 characters long). You can generate one with:
> ```bash
> openssl rand -base64 32
> ```

### ⚙️ Step 3: Customize Entity IDs

Customize the project to your requirements.
Ensure that the entity IDs in [sensors/homeassistant.yml](sensors/homeassistant.yml) match your HA sensors:

| Entity Type | Example Entity ID |
|-------------|-------------------|
| ☀️ **Solar Production** | `sensor.deye_inverter_deye_daily_production` |
| 🏠 **Home Consumption** | `sensor.electricity_daily_consumption` |
| 🔋 **Battery SOC** | `sensor.deye_inverter_deye_battery_soc` |

> **✅ Note**: The dashboard should work regardless of whether the sensors exist or not.

> **📊 Important**: The dashboard requires sensors that accumulate **daily consumption values**, similar to Home Assistant's energy distribution feature. While Home Assistant typically uses total meters, here daily meters are needed. Since the daily accumulated values from the energy dashboard are not accessible, I created utility meters in Home Assistant for daily consumption.

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

**Hosted Dashboard** (no installation needed):
> **🎉 No installation needed!** Flash directly from your browser.

1. Visit **https://web.esphome.io/**
2. Click "Connect" and select your ESP32-S3 device
3. Upload your `main.yml` configuration file
4. Click "Install" to compile and flash

**Perfect for:** Users who prefer GUI over command line, or quick flashing without local ESPHome installation.

</details>

## 📱 Usage

After successful startup, the display shows a loading screen followed by the main dashboard. 

### 🎯 Navigation

👆 **Touch the screen** to switch between views:

| View | Description |
|------|-------------|
| **📊 Dashboard** | Overview with energy flow diagrams and current values |
| **📋 Power Table** | Detailed table showing the current load |

### 💡 Additional Features

- **🌟 Backlight Control**: Automatically turns off during OTA updates
- **🏠 Home Assistant Integration**: Control backlight via Home Assistant
- **🔄 Real-time Updates**: Data refreshes automatically from your sensors

## ⚙️ Configuration Structure

The configuration is modular for easy customization:

| File | Description |
|------|-------------|
| 📄 [main.yml](main.yml) | Main configuration file |
| 🔧 [base/hardware.yml](base/hardware.yml) | Hardware-specific settings (display, I2C, SPI) |
| 🌐 [base/network.yml](base/network.yml) | WiFi, API, OTA configuration |
| 📊 [sensors/homeassistant.yml](sensors/homeassistant.yml) | Home Assistant sensor imports |
| 🎨 [ui/*.yml](ui/) | UI components (fonts, layout, animations, pages) |

### 📂 File Structure

```
.
├── main.yml                    # Main ESPHome configuration
├── secrets.yaml                # Your credentials (not in git)
├── docker-compose.yml          # Docker setup
├── base/
│   ├── hardware.yml            # Display & hardware config
│   └── network.yml             # Network settings
├── sensors/
│   └── homeassistant.yml       # HA sensor definitions
├── ui/
│   ├── animations.yml          # UI animations
│   ├── fonts.yml               # Font definitions
│   ├── layout.yml              # Layout configuration
│   ├── loading.yml             # Loading screen
│   ├── page_dashboard.yml      # Main dashboard page
│   ├── page_power_table.yml    # Power table page
│   └── power_table_updates.yml # Power table updates
├── fonts/                      # Font files
└── docs/                       # Documentation & screenshots
```

## 📚 Further Resources

| Resource | Description |
|----------|-------------|
| 📖 [ESPHome Documentation](https://esphome.io/) | Official ESPHome documentation |
| 🏠 [Home Assistant](https://www.home-assistant.io/) | Home automation platform |
| 🖥️ [Guition ESP32-S3-4848S040](https://devices.esphome.io/devices/guition-esp32-s3-4848s040/) | Device information |
| 🔋 [Energy Dashboard](https://www.home-assistant.io/docs/energy/) | Home Assistant Energy Dashboard |

---

<div align="center">

**Made by the open source community**

⭐ Star us on [GitHub](https://github.com/firsttris/esphome-energy-dashboard) • 🐛 [Report a Bug](https://github.com/firsttris/esphome-energy-dashboard/issues) • 💡 [Request a Feature](https://github.com/firsttris/esphome-energy-dashboard/issues)

</div>