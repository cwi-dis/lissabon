#ifndef _IOTSATOUCH_H_
#define _IOTSATOUCH_H_
#include "iotsa.h"
#include "iotsaApi.h"

class Touchpad {
  friend class IotsaTouchMod;
public:
  Touchpad(int _pin) : pin(_pin) {}
protected:
  int pin;
};

class IotsaTouchMod : public IotsaMod {
public:
  IotsaTouchMod(IotsaApplication& app, Touchpad *_pads, int _nPad) : IotsaMod(app), pads(_pads), nPad(_nPad) {}
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info() { return ""; }
protected:
  Touchpad *pads;
  int nPad;

};

#endif
