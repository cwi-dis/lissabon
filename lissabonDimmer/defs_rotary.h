
// Pin to which MOSFET is attached. Channel is only relevant for esp32.
#define PIN_PWM_DIMMER 2
#define CHANNEL_PWM_DIMMER 1


#include "iotsaInput.h"

// Uses a rotary encoder
#define WITH_ROTARY
#define WITH_UI

// A rotary encoder for increment/decrement and a button for on/off.
Button button(4, true, false, true);
RotaryEncoder encoder(16, 17);

Input* inputs[] = {
  &button,
  &encoder
};

