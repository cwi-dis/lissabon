# BLE Dimmer

This module can used to build a battery-operated remote controller (using BLE)
for iotsaDimmer or iotsaLedstrip devices. IotsaBLEDimmer will have two touchpads
functioning as off/on on a short touch and intensity down/up on a long press.
Which iotsaDimmer or iotsaLedstrip is controlled can be set through the
web interface or REST API. Optionally more dimmers can be controlled
with extra touchpads.

The intention is that this device is normally powered from a LiPo
battery, without WiFi, and in hibernate mode until a button is touched.
That way, the battery life should be months.

To recharge the iotsaBLEDimmer is attached to USB to recharge the LiPo.
When powered from USB WiFi will be enabled and the device can be configured,
for example to assign the iotsaDimmer or iotsaLedstrip the device controls.

