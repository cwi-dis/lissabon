#include "iotsa.h"
#include "iotsaTouch.h"
#include "iotsaConfigFile.h"


void IotsaTouchMod::setup() {
}

void IotsaTouchMod::serverSetup() {
}

void IotsaTouchMod::loop() {
  for (int i=0; i<nPad; i++) {
    int val = touchRead(pads[i].pin);
    IotsaSerial.printf("touch[%d]: pin=%d: %d\n", i, pads[i].pin, val);
  }
}
