<div align="center">
<img width="460" src="https://raw.githubusercontent.com/mindflayer-vtt/mindflayer-keypad/main/.github/foundryvtt-mindflayer-logo.png">
</div>

# Mind Flayer - Keypad
Firmware for an ESP8266 based keypad that can be used with the Mind Flayer VTT module &amp; server

<div align="center">
<img width="460" src="https://raw.githubusercontent.com/mindflayer-vtt/mindflayer-keypad/main/.github/keypad.png">
</div>

## Schematic and PCB
See [hardware/board/README.md](https://github.com/mindflayer-vtt/mindflayer-keypad/tree/main/hardware/board) for more information.

## Case
See [hardware/case/README.md](https://github.com/mindflayer-vtt/mindflayer-keypad/tree/main/hardware/case) for more information.

## Requirements
You have to have set up the following software in order to compile an flash the controller:

 - Python

## Setup

1. Use the following command to setup a python virtual environment:
   ```bash
   python -m venv .venv
   ```
2. Activate the python virtual environment
   ```bash
   source .venv/bin/activate
   ```
3. Install platformio
   ```bash
   pip install platformio
   ```

## Configuration

1. Create a `include/config.h` file and add the WIFI_SSID, WIFI_PASS and WSS_URL variables
   ```cpp
   #define WIFI_SSID "<your ssid>"
   #define WIFI_PASS "<your wifi password>"
   #define WSS_URL   "<your websocketserver url>"
   ```

## Flashing

1. Connect the wemos d1 mini to the PC with USB
2. Run the following command to flash the controller
   ```bash
   platformio run -e controller_1 -t upload
   ```
