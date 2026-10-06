//
// lissabonController: a battery-operated BLE controller (OLED + rotary
// encoder) for a dynamic collection of remote Lissabon dimmers (ledstrips,
// PWM dimmers, ...). App-specific module logic lives in
// LissabonControllerMod.h/.cpp; this file is just the glue (defines, module
// instantiation, setup()/loop()).
//

#include "iotsa.h"
#include "iotsaConfigFile.h"

IotsaApplication application("Iotsa LEDstrip Controller");

#include "iotsaBattery.h"
// #define PIN_DISABLESLEEP 0
#define PIN_VBAT 37
#define VBAT_100_PERCENT (12.0/11.0) // 100K and 1M resistors divide by 11, not 10...
IotsaBatteryMod batteryMod(application);

#include "buttons.h"
IotsaInputMod touchMod(application, getInputs(), getInputCount());

#include "LissabonControllerMod.h"

// Instantiate the module, and install it in the framework
LissabonControllerMod ledstripControllerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
