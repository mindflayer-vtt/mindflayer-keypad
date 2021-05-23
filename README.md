# Keyboard Standalone Software

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

1. Edit the platformio.ini file and change the WIFI_SSID, WIFI_PASS and WSS_URL variables
   ```ini
   '-DWIFI_SSID="<your ssid>"'
   '-DWIFI_PASS="<your wifi password>"'
   '-DWSS_URL="<your websocketserver url>"'
   ```

## Flashing

1. Connect the wemos d1 mini to the PC with USB
2. Run the following command to flash the controller
   ```bash
   platformio -e controller_1 -t upload
   ```
