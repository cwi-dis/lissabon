
#ifndef _DIMMERUI_H_
#define _DIMMERUI_H_
#include "iotsa.h"
#include "BLEDImmer.h"
#include "iotsaInput.h"

class DimmerUI  {
public:
  //DimmerUI(int _num, DimmerCallbacks *_callbacks) : BLEDimmer(_num, _callbacks) {}
  DimmerUI(BLEDimmer& _dimmer) : dimmer(_dimmer) {}
  void setEncoder(UpDownButtons& encoder);
  bool touchedOnOff();
  bool levelChanged();
protected:
  BLEDimmer& dimmer;
};
#endif // _DIMMERUI_H_