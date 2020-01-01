#include "iotsa.h"
#include "iotsaTouch.h"
#include "iotsaConfigFile.h"

#define DEBOUNCE_DELAY 50 // 50 ms debouncing

static void dummyTouchCallback() {}

void IotsaTouchMod::setup() {
  bool anyWake;
  for(int i=0; i<nButton; i++) {
    if (buttons[i].wakeOnPress) {
      anyWake = true;
      touchAttachInterrupt(buttons[i].pin, dummyTouchCallback, buttons[i].threshold);
    }
  }
  if (anyWake) {
    IFDEBUG IotsaSerial.println("IotsaTouchMod: enable wake on touch");
    esp_sleep_enable_touchpad_wakeup();
  }
}

void IotsaTouchMod::serverSetup() {
}

void IotsaTouchMod::loop() {

  for (int i=0; i<nButton; i++) {
    uint16_t value = touchRead(buttons[i].pin);
    if (value == 0) continue;
    bool state = value < buttons[i].threshold;
    if (state != buttons[i].debounceState) {
      // The touchpad seems to have changed state. But we want
      // it to remain in the new state for some time (to cater for 50Hz/60Hz interference)
      buttons[i].debounceTime = millis();
      iotsaConfig.postponeSleep(DEBOUNCE_DELAY*2);
    }
    buttons[i].debounceState = state;
    if (millis() > buttons[i].debounceTime + DEBOUNCE_DELAY && state != buttons[i].buttonState) {
      // The touchpad has been in the new state for long enough for us to trust it.
      buttons[i].buttonState = state;
      bool doSend = (buttons[i].buttonState && buttons[i].sendOnPress) || (!buttons[i].buttonState && buttons[i].sendOnRelease);
      IFDEBUG IotsaSerial.printf("Touch callback for button %d pin %d state %d value %d\n", i, buttons[i].pin, state, value);
      if (doSend && buttons[i].activationCallback) {
        buttons[i].activationCallback();
      }
    }
  }
}
