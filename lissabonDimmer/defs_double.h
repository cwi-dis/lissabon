
// Pin to which MOSFET is attached. Channel is only relevant for esp32.
// Note that due to stupidity on my side this variant does not use IO2 for the PWM
// dimmer.
#define PIN_PWM_DIMMER 16
#define CHANNEL_PWM_DIMMER 1
#define PIN_PWM_DIMMER_2 17
#define CHANNEL_PWM_DIMMER_2 2

#define WITH_DOUBLE_DIMMER
#define WITH_1BUTTON
#define WITH_UI

#include "iotsaInput.h"

// One buttons: short press: toggle on/off, long press: cycle value between min and max.
Button button(15, true, true, true);
CyclingButton encoder(button);

Button button2(4, true, true, false);
CyclingButton encoder2(button2);

Input* inputs[] = {
  &encoder,
  &encoder2
};
