# ESPHome Energie Dashboard

Ein modulares ESPHome-Projekt, das die tägliche Energieverteilung ähnlich wie in Home Assistant visualisiert, mit Echtzeit-Daten zu Solarproduktion, Netzbezug/-einspeisung, Batteriestatus und Gasverbrauch, auf dem [Guition-ESP32-S3-4848S040](https://devices.esphome.io/devices/guition-esp32-s3-4848s040/) Display.

## Voraussetzungen

- **Hardware**: Guition-ESP32-S3-4848S040 (ESP32-S3 mit 4.8" IPS-Touch-Display)
- **Software**:
  - Docker und Docker-Compose
  - ESPHome (läuft im Container)
  - Home Assistant (für Sensordaten)
- **USB-Kabel**: Für das initiale Flashen des ESP32

## Installation und Setup

### 1. Repository klonen

```bash
git clone https://github.com/dein-username/esphome-energy-dashboard.git
cd esphome-energy-dashboard
```

### 2. Secrets konfigurieren

Erstelle oder bearbeite die Datei `secrets.yaml` und füge deine eigenen Werte ein:

```yaml
wifi_ssid: "Dein_WLAN_Name"
wifi_password: "Dein_WLAN_Passwort"
api_key: "Dein_ESPHOME_API_Schluessel"  # Generiere einen sicheren Schlüssel
```

**Wichtig**: Verwende einen starken API-Schlüssel (z.B. 32 Zeichen lang). Du kannst einen mit `openssl rand -base64 32` generieren.

### 3. Docker-Compose starten

Starte den ESPHome-Container im Hintergrund:

```bash
docker-compose up -d
```

Der Container mountet das Config-Verzeichnis und läuft im Host-Netzwerkmodus für einfachen Zugriff.

### 4. Firmware kompilieren und flashen

Verbinde deinen ESP32 über USB und flash die Firmware:

```bash
docker-compose exec esphome 

esphome run main.yml --device=/dev/ttyUSB0
```

**Hinweise**:
- Passe den Device-Pfad (`/dev/ttyUSB0`) an deinen System an (z.B. `/dev/ttyACM0` auf manchen Linux-Systemen).
- Stelle sicher, dass dein Benutzer Zugriff auf den USB-Port hat (ggf. `sudo` verwenden oder udev-Regeln anpassen).
- Beim ersten Flash wird die Firmware kompiliert – das kann einige Minuten dauern.

### 5. Home Assistant Integration

Nach dem Flashen verbindet sich das Gerät automatisch mit deinem WLAN und Home Assistant. Stelle sicher, dass die Entity-IDs in `sensors/homeassistant.yml` mit deinen HA-Sensoren übereinstimmen. Beispiel-Entities:

- `sensor.deye_wechselrichter_deye_tagliche_produktion` (Solarproduktion)
- `sensor.strom_tagesverbrauch` (Hausverbrauch)
- `sensor.deye_wechselrichter_deye_batterie_soc` (Batterie SOC)

Passe die Entity-IDs bei Bedarf an.

## Verwendung

Nach dem erfolgreichen Start zeigt das Display ein Loading-Screen, gefolgt vom Haupt-Dashboard. Berühre den Bildschirm, um zwischen Dashboard und detaillierter Power-Tabelle zu wechseln.

- **Dashboard**: Übersicht mit Energiefluss-Diagrammen und aktuellen Werten.
- **Power-Tabelle**: Detaillierte Tabelle mit historischen Daten.

Das Backlight schaltet sich bei OTA-Updates aus und kann über Home Assistant gesteuert werden.

## Konfiguration anpassen

Die Konfiguration ist modular aufgebaut:

- `base/hardware.yml`: Hardware-spezifische Einstellungen (Display, I2C, SPI)
- `base/network.yml`: WiFi, API, OTA
- `sensors/homeassistant.yml`: HA-Sensoren
- `ui/*.yml`: UI-Komponenten (Fonts, Layout, Animationen)

Bearbeite diese Dateien nach Bedarf und kompiliere neu mit `esphome compile main.yml`.

## Beiträge

Beiträge sind willkommen! Erstelle Issues für Bugs oder Feature-Requests, oder sende Pull-Requests.

## Lizenz

Dieses Projekt ist unter der MIT-Lizenz lizenziert. Siehe [LICENSE](LICENSE) für Details.

## Screenshots

*(Füge hier Screenshots des Dashboards hinzu, wenn verfügbar)*
