#ifndef _IOTSATOUCH_H_
#define _IOTSATOUCH_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaRequest.h"

typedef std::function<bool(IotsaRequestContainer*)> ActivationCallbackType;

class Touchpad : public IotsaRequestContainer {
  friend class IotsaTouchMod;
public:
  Touchpad(int _pin, bool _sendOnPress, bool _sendOnRelease, bool _wake) : pin(_pin), sendOnPress(_sendOnPress), sendOnRelease(_sendOnRelease), wakeOnPress(_wake), activationCallback(NULL) {}
protected:
  int pin;
  uint16_t thresholdLow = 20;
  uint16_t thresholdHigh = 30;
  bool sendOnPress;
  bool sendOnRelease;
  bool wakeOnPress;
  ActivationCallbackType activationCallback;
  bool buttonState;
};

class IotsaTouchMod : public IotsaMod {
public:
  IotsaTouchMod(IotsaApplication& app, Touchpad *_buttons, int _nButton) : IotsaMod(app), buttons(_buttons), nButton(_nButton) {}
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info() { return ""; }
protected:
  Touchpad *buttons;
  int nButton;

};

#endif
