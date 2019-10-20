# Ledstrip controller

This iotsa application controls led strips.
It is intended mainly to be used in lighting applications, not signalling applications.

You can control the number of LEDs in the strip
and the interval between lit LEDs (to have for 
example only 1 in every 3 LED light up).

Light color can be set with either RGB values
(numbers between 0.0 and 1.0) or HLS values
(hue as an angle between 0 and 360, lightness and
saturation numbers between 0.0 and 1.0).

Gamma correction can be applied (a value of 2.8 seems
to be commonly suggested for NeoPixel LEDs).

## configuration

To change the type of NeoPixel (default GRB 800KHz,
the most common type) or the output pin to which the strip is connected (default GPIO 13) you have to recompile it.
