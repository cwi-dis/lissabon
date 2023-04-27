#ifndef BUTTONS_H
#define BUTTONS_H
#include <iotsa.h>
#include "iotsaInput.h"

Input** getInputs();
int getInputCount();

class ButtonsCallbacks {
public:
  virtual bool selectDimmer(bool next, bool prev) = 0;
  virtual float getTemperature() = 0;
  virtual void setTemperature(float temperature) = 0;
  virtual float getLevel() = 0;
  virtual void setLevel(float level) = 0;
  virtual void toggle() = 0;
};

class Buttons {
public:
  Buttons(ButtonsCallbacks* _controller)
  : controller(_controller)
  {}
  void setup();
  bool uiRockerPressed();
  bool uiButtonPressed();
  bool uiEncoderChanged();
private:
  ButtonsCallbacks* controller;
  void _tap();
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};
#endif // BUTTONS_H