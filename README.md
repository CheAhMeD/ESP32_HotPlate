# ESP32 HotPlate (Reflow Station)
DIY Hot Plate for SMD Soldering Using ESP32

## System Overview
#### Architecture

<img src="Drawings/hot_plate_arch.svg" alt="Hot Plate" width="300" height="300">

To access the Cirkit Designer project folder follow this link ([Cirkit](https://app.cirkitdesigner.com/project/63c725ec-d5b7-41f8-a704-f3c43ff69539))

#### Used Hardware

#### Case
The wooden case is made from 4mm plywood sheets.
*The file for laser cutting can be found in Drawings folder [here](Drawings/hot_plate_case.svg).*

#### BOM
##### Required
- ESP32-WROOM-32UE Dev Kit ([AliExpress](https://www.aliexpress.com/i/1005006101661404.html))
- RP1A23D5 ([mouser](https://www.mouser.be/ProductDetail/Carlo-Gavazzi/RP1A23D5?qs=xZ%2FP%252Ba9zWqYrTlescD3zNw%3D%3D&srsltid=AfmBOop4c0-2oQbLZNWjqGRbjlYNQFfOyFUVsmCj10K13PxnHtzqtBrx&_gl=1*1asaar2*_ga*MTIzNzcwODgzNi4xNzM1MjQ2ODIx))
- 400W PTC Heating plate **(I Used 2)** ([AliExpress](https://www.aliexpress.com/item/1005007420346819.html))
- Adafruit 2.8" TFT LCD Shield ([Adafruit](https://learn.adafruit.com/adafruit-2-8-tft-touch-shield-v2?view=all))
- 5V DC Cooling Fan **(I Used 2)** ([AliExpress](https://www.aliexpress.com/item/1005005517832482.html))
- AC to DC (5V) PSU ([AliExpress](https://www.aliexpress.com/item/1005003273464395.html))
- MAX6675 Thermocouple Module ([AliExpress](https://www.aliexpress.com/item/1005006222174410.html))
- FAN Driving Circuit (100uF Polorized Capacitor + 1K Omh Resistor + BD139 BJT Transistor + 1N4007 Diode) **Or Similar**
- Passive Buzzer ([AliExpress](https://www.aliexpress.com/item/1005006237096111.html))
- Push Button (Any)
##### Optional
- Custom PCB to house everything.
*I designed one using KiCad if any one is interested drop me a message, but you can just solder the different components manually*

## Usage
In the images folder there is a small **video demo** on how to use the hot plate.

### Screenshots
<img src="images/hot_plate.png" alt="Hot Plate" width="280" height="300">
<img src="images/main.png" alt="Hot Plate" width="300" height="200">
<img src="images/settings_soak.png" alt="Hot Plate" width="300" height="200">
<img src="images/settings_pid.png" alt="Hot Plate" width="300" height="200">
<img src="images/plot.png" alt="Hot Plate" width="300" height="200">

## About hot plates and reflow soldering

A standard reflow system consists of five phases: preheating, soaking, reflowing, dwelling, and cooling.

![stages](images/reflow_stages.png)

IMHO, PID control is required for a successful build of a reflow "system".

*More details on the importance of PID can be found [here](https://hobbybotics.com/projects/hobbybotics-reflow-controller-v8-03/)* 

## Authors

- [@CheAhmed](https://github.com/CheAhMeD)





