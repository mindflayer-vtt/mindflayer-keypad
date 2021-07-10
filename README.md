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
