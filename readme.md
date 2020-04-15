# Dimmer

This iotsa application implements a low-voltage dimmer using a MOSFET. Brighness 
and on/off are controlled either through a rotary encoder or through touchpads 
(selectable at build time).

Brightness and on/off state are inspectable and controllable over Bluetooth LE
and through a web or rest interface.

It is intended mainly to be used in lighting applications.

## Hardware construction

To be supplied.

## Configuration

At compile time you select whether to use a rotary encoder, buttons or touchpads
to control the dimmer (depending on the hardware you built).

## Usage

The program is setup to be built for esp32. It will be built with the _IotsaBLEServer_ module so it can be controlled over Bluetooth LE as well as WiFi. It is also built with _IotsaBattery_ so it can operate in low-power mode.

When configured for reasonable power saving options for control over bluetooth (100ms wake time, 900ms light sleep with WiFi off except for a short while after reboot or by request) the board should use around 15mA on average (not counting the output power consumption, obviously:-). 

The intention (but that hasn't been tested yet as of this writing) is that this allows you to use these dimmers for lighting in an off-grid 12V environment with solar cells charging a car battery without having to worry about the power consumption of the control hardware and software.

An accompanying _iotsaLedstripController_ application should also be available at some point in the future, which will allow you to control a number of _iotsaDimmer_ and _iotsaLedstrip_ modules over Bluetooth LE.

