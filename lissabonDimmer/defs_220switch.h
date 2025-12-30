
// Pin to which MOSFET is attached. Channel is only relevant for esp32.

#define PIN_PWM_DIMMER 0
#define CHANNEL_PWM_DIMMER (-1)

// Uses no user interface at all (only controlled via wifi or bluetooth)
#undef WITH_UI

// Switch on/off on every reboot
#undef TOGGLE_ONOFF_ON_REBOOT