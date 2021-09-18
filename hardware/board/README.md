# Schematic and PCB -- Mindflayer-Keypad

Both the Schematic and the PCB of the original Batch were made using [EasyEDA](https://easyeda.com/) and produced at [JLCPCB](https://jlcpcb.com/) with the top side populated during manufacturing.

## Materials required

 - any Wemos D1 Mini revision
 - a 470µF electrolytic capacitor for the bottom
 - 11x mechanical switches (we used Gateron Browns but any Cherry MX or clones (like Kailh) should work as long as they have the same formfactor & pins)

## Things to improve

 - [ ] like mentioned in the [README for the case](https://github.com/shawly/mindflayer-keypad/tree/main/hardware/case) the Wemos could be moved to another position to get a better finish, though this is just for aesthetics
 - [ ] the PCB would have enough space for a charging circuit and the case has enough room inside to fit a 2000mAh battery or even bigger
   - with the estimated draw of 77mA for the Wemos D1 and a maximum of 33mA per WS2812b LED a 2000mAh battery would last approx. ~13 hours which is more than enough for a D&D session if the keypads are fully charged
 - [ ] the ESP8266 could be integrated into the PCB, but the difficulty for soldering the chip would greatly increase as well as the cost if you wanted the ESP preassembled by the manufacturer so we decided against this
