
// Pin to which MOSFET is attached. Channel is only relevant for esp32.
#define PIN_PWM_DIMMER 2
#define CHANNEL_PWM_DIMMER 1

#define WITH_1BUTTON
#define WITH_UI

#include "iotsaInput.h"

// One buttons: short press: toggle on/off, long press: cycle value between min and max.
Button button(15, true, true, true);
CyclingButton encoder(button);

Input* inputs[] = {
  &encoder
};
