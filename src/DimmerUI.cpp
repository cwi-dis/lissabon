#include "DimmerUI.h"

namespace Lissabon {

bool
DimmerUI::touchedOnOff() {
  IFDEBUG IotsaSerial.printf("touchedOnOff %d: onOff=%d level=%f\n", dimmer.num, dimmer.isOn, dimmer.level);
  dimmer.callbacks->uiButtonChanged();
  dimmer.updateDimmer();
  return true;
}

bool
DimmerUI::levelChanged() {
  IFDEBUG IotsaSerial.printf("levelChanged %d: onOff=%d level=%f", dimmer.num, dimmer.isOn, dimmer.level);
#ifdef DIMMER_WITH_TEMPERATURE
  IFDEBUG IotsaSerial.printf(" temperature=%f", dimmer.temperature);
#endif
  IFDEBUG IotsaSerial.println();
  dimmer.updateDimmer();
  return true;
}

void
DimmerUI::setUpDownButtons(UpDownButtons& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder.bindVar(dimmer.level, dimmer.minLevel, 1.0, 0.02);
  encoder.bindStateVar(dimmer.isOn);
  encoder.setStateCallback(std::bind(&DimmerUI::touchedOnOff, this));
}

void
DimmerUI::setRotaryEncoder(Button&button, RotaryEncoder& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  button.setCallback(std::bind(&DimmerUI::touchedOnOff, this));
  button.bindVar(dimmer.isOn, true);
  encoder.bindVar(dimmer.level, dimmer.minLevel, 1.0, 0.02);
  encoder.setAcceleration(500);
}

#ifdef DIMMER_WITH_TEMPERATURE
void
DimmerUI::setTemperatureUpDownButtons(UpDownButtons& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder.bindVar(dimmer.temperature, 2200.0, 6500.0, 100);
  encoder.setStateCallback(std::bind(&DimmerUI::touchedOnOff, this));
}

void
DimmerUI::setTemperatureRotaryEncoder(RotaryEncoder& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  encoder.bindVar(dimmer.temperature, 2200.0, 6500.0, 100);
}
#endif // DIMMER_WITH_TEMPERATURE
}