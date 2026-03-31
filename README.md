# MQTT POS Thermal Printer

A ESP-based project that listens for MQTT messages containing print jobs (title, text) and prints them to a thermal POS printer.

## Features
- Listen on an MQTT topic for print jobs
- Print title and text with configurable formatting

## Hardware
- ESP32 (or similar with HardwareSerial/UART)
- Thermal POS printer with ESC/POS compatibility. 
- TTL connected. TX/RX wired to the printer's serial input (see `config.h` for pins)

## Quick Start
1. Open `POS_printer/POS_printer.ino` in the Arduino IDE or PlatformIO.
2. Copy `POS_printer/config.h.example` to `POS_printer/config.h` and edit Wi‑Fi and MQTT credentials.
3. Upload to the device.

## MQTT payload (examples)
- Minimal:

```json
{"title":"Hello","text":"This is a test"}
```

## Configuration
- `POS_printer/config.h` contains:
	- Wi‑Fi SSID/password
	- MQTT server, topic and credentials
	- `MQTT_PACKET_SIZE` — buffer size for incoming MQTT messages
	- RX / TX pins for the printer

## Building / Uploading
- Arduino IDE: select the board (ESP32), set the correct COM port, then upload.
- PlatformIO: use the provided project settings or create a simple `platformio.ini` targeting your board.

## Troubleshooting
- If JSON parsing fails, check the payload size vs `MQTT_PACKET_SIZE`.
- If printed images look bad, try different icon encodings or sizes, or disable experimental `ICON`.
- If QR codes don't print, verify your printer supports ESC/POS QR commands (many do).

## License
See `LICENSE` in this repository.