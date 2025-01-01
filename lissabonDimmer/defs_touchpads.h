
// Pin to which MOSFET is attached. Channel is only relevant for esp32.

#define PIN_PWM_DIMMER 2
#define CHANNEL_PWM_DIMMER 1

// Uses touchpads for intensity up/down and on/off.
#define WITH_TOUCHPADS
// Uses a user interface in the first place
#define WITH_UI

#include "iotsaInput.h"
// Two touchpad pins: off/decrement (long press), on/increment (long press)
Touchpad touchdown(2, true, true, true);
Touchpad touchup(13, true, true, true);
UpDownButtons encoder(touchdown, touchup, true);

Input* inputs[] = {
  &encoder
};