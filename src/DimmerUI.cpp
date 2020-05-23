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
  IFDEBUG IotsaSerial.printf("levelChanged %d: onOff=%d level=%f\n", dimmer.num, dimmer.isOn, dimmer.level);
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

}