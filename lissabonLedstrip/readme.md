# lissabon ledstrip controller

This iotsa application controls led strips.
It is intended mainly to be used in lighting applications, not signalling applications.

You can control the number of LEDs in the strip
and the interval between lit LEDs (to have for 
example only 1 in every 3 LED light up).

Light color can be set with either:

- RGB(W) values (numbers between 0.0 and 1.0),
- HLS values (hue as an angle between 0 and 360, lightness and saturation numbers between 0.0 and 1.0) or
-  Color temperature in Kelvin and illuminance.

Gamma correction can be applied (a value of 2.2 or 2.8 seems
to be commonly suggested for NeoPixel LEDs).

Changing the color will do a fairly fast gradual
crossfade from the old color to the new one.

When configured for RGBW LEDs and using the temperature/illuminance control an illuminance of 1.0 means "as much light as possible", i.e. it will use all 4 LEDs as much as possible. In this use case you can also configure the temperature of the White LED so the resulting color temperature should be fairly correct.

## configuration

To change the type of NeoPixel (default GRB 800KHz,
the most common type) or the output pin to which the strip is connected (default GPIO 13) you have to recompile it.

## construction

The `extras` folder has Fritzing files for a 4-touchpad (dimmer/off, brighter/on, warmer, cooler) circuit, for either 12V or 5V RGBW strips. Also available as PDF, [5V circuit](extras/Hardware-5v.pdf) and [12V circuit](extras/Hardware-12v.pdf).

There is also a Fusion 3D project for a 3D-printable case.

## usage

The program is setup to be built for esp32. It will be built with the _IotsaBLEServer_ module so it can be controlled over Bluetooth LE as well as WiFi. It is also built with _IotsaBattery_ so it can operate in low-power mode.

When configured for reasonable power saving options for control over bluetooth (100ms wake time, 900ms light sleep with WiFi off except for a short while after reboot or by request) the board should use around 15mA on average (not counting the LED power consumption, obviously:-). Formal power consumption measurements will be made later.

The intention  is that this allows you to use these LED strips for lighting in an off-grid 12V environment with solar cells charging a car battery without having to worry about the power consumption of the control hardware and software.

Accompanying _lissabonController_ and _lissabonRemote_ applications are also available, which will allow you to control a number of _lissabonLedstrip_ and _lissabonDimmer_ modules over Bluetooth LE.
