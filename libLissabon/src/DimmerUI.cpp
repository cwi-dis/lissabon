#include "DimmerUI.h"

namespace Lissabon {

bool
DimmerUI::touchedOnOff() {
#ifdef DIMMER_WITH_LEVEL
  IOTSA_LOG_DEBUG("DimmerUI", "touchedOnOff %d: onOff=%d level=%f", dimmer.num, dimmer.isOn, dimmer.level);
#else
  IOTSA_LOG_DEBUG("DimmerUI", "touchedOnOff %d: onOff=%d", dimmer.num, dimmer.isOn);
#endif
  if (dimmer.callbacks) dimmer.callbacks->dimmerOnOffChanged();
  dimmer.updateDimmer();
  return true;
}

bool
DimmerUI::levelChanged() {
#ifdef DIMMER_WITH_LEVEL
  dimmer.isOn = true;
#ifdef DIMMER_WITH_TEMPERATURE
  IOTSA_LOG_DEBUG("DimmerUI", "levelChanged %d: onOff=%d level=%f temperature=%f", dimmer.num, dimmer.isOn, dimmer.level, dimmer.temperature);
#else
  IOTSA_LOG_DEBUG("DimmerUI", "levelChanged %d: onOff=%d level=%f", dimmer.num, dimmer.isOn, dimmer.level);
#endif
  dimmer.updateDimmer();
#endif // DIMMER_WITH_LEVEL
  return true;
}

void
DimmerUI::setUpDownButtons(UpDownButtons& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
#ifdef DIMMER_WITH_LEVEL
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder.bindVar(dimmer.level, dimmer.minLevel, 1.0, 0.02);
#endif
  encoder.bindStateVar(dimmer.isOn);
  encoder.setStateCallback(std::bind(&DimmerUI::touchedOnOff, this));
}

void
DimmerUI::setRotaryEncoder(Button& button, RotaryEncoder& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  button.setCallback(std::bind(&DimmerUI::touchedOnOff, this));
  button.bindVar(dimmer.isOn, true);
#ifdef DIMMER_WITH_LEVEL
  encoder.bindVar(dimmer.level, dimmer.minLevel, 1.0, 0.02);
  encoder.setAcceleration(500);
#endif
}

void
DimmerUI::setCyclingButton(CyclingButton& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  encoder.setStateCallback(std::bind(&DimmerUI::touchedOnOff, this));
  encoder.bindStateVar(dimmer.isOn);

#ifdef DIMMER_WITH_LEVEL
  encoder.bindVar(dimmer.level, dimmer.minLevel, 1.0, 0.02);
  // encoder.setAcceleration(500);
#endif
}

void
DimmerUI::setOnOffButton(Button& button) {
  button.setCallback(std::bind(&DimmerUI::touchedOnOff, this));
  button.bindVar(dimmer.isOn, true);
}

#ifdef DIMMER_WITH_TEMPERATURE
void
DimmerUI::setTemperatureUpDownButtons(UpDownButtons& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder.bindVar(dimmer.temperature, DIMMER_MIN_TEMPERATURE, DIMMER_MAX_TEMPERATURE, 100);
  encoder.setStateCallback(std::bind(&DimmerUI::touchedOnOff, this));
}

void
DimmerUI::setTemperatureRotaryEncoder(RotaryEncoder& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  encoder.bindVar(dimmer.temperature, DIMMER_MIN_TEMPERATURE, DIMMER_MAX_TEMPERATURE, 100);
}
#endif // DIMMER_WITH_TEMPERATURE
}