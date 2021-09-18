# Case -- Mindflayer-Keypad

The case of this iteration was 3D-printed in 3 parts and is screwed together.

## Tools and Materials

1. 3D-Printer, we used a Prusa i3 MK3S
2. Translucent and normal filament in any color you prefer
3. Five screws: M2x5mm (2 mm diameter and 5 mm length)
4. Matching screw driver with a slim shaft
5. X-Acto knife for easier support removal (if you print with PETG)
6. Rubber feet to keep the keypad from moving on the table
7. Optional: Sandpaper grid 120 up to 8000 to polish the faceplate (wet sanding)

## Fusion 360

The latest version of the case is available as public link for Fusion 360 [here](https://a360.co/3CnbHYb).  
The case files are also under the MIT license, so you are permitted to do anything with these files as long as you mention the original authors.

## Printing

When printing the case you need to split the STL into 3 parts:

 - the top cover where the PCB is mounted, 
   - it should be printed with translucent filament so the LEDs are visible
   - it should be printed with the top side on the print bed
   - 100% infill is recommended to get a better lighting effect
   - supports are required for the angled lip
 - the case
   - can be printed in whatever color you want
   - it should be printed with the bottom on the print bed
   - supports need to be enabled, but you can block supports for the mounting screwhole on the left side as supports are not needed for that 2mm hole
   - infill doesn't really matter, 15-20% should be fine
 - the little mounting post, which is used for fixing the top left corner of the cover plate to the case
   - the smaller hole should be placed on the printbed
   - supports aren't needed
   - infill doesn't matter

## Things to improve

After the third revision of the case we were pretty much satisfied, though after the assembly we noticed some minor things that could be improved:

 - [ ] adding 4 recesses for the rubber feet on the bottom of the case (just for aesthetics)
 - [ ] get rid of the mounting post and the screw on the left side by moving the Wemos D1 to the back of the case
   - this would require changing the PCB though
 - [ ] moving the Wemos D1 inside the case and add a female micro USB port or a USB-C port for a better finish
   - it might make sense to integrate the ESP8266 into the PCB itself, but that will drive up manufacturing cost which we tried to keep at the minimum
 - [ ] externalize the reset button, so you don't have to pull the plug for resets which could wear down the micro USB port
 - [ ] the space inside the keypad would technically allow for a 2000mAh battery + charging circuit so you could use the keypad fully wireless
   - with the estimated draw of 77mA for the Wemos D1 and a maximum of 33mA per WS2812b LED the battery would still last approx. ~13 hours which is more than enough for a D&D session if the keypads are fully charged
