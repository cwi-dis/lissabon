
// Pin to which MOSFET is attached. Channel is only relevant for esp32.
#define PIN_PWM_DIMMER 2
#define CHANNEL_PWM_DIMMER 1

#include "iotsaInput.h"

// Uses buttons for intensity up/down and on/off.
#define WITH_BUTTONS
// Uses a user interface in the first place
#define WITH_UI

// Two buttons: off/decrement (long press), on/increment (long press)
Button buttondown(16, true, true, true);
Button buttonup(17, true, true, true);
UpDownButtons encoder(buttondown, buttonup, true);

Input* inputs[] = {
  &encoder
};
