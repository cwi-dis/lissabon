
#ifndef _DIMMERUI_H_
#define _DIMMERUI_H_
#include "iotsa.h"
#include "AbstractDimmer.h"
#include "iotsaInput.h"

namespace Lissabon {
class DimmerUI  {
public:
  //DimmerUI(int _num, DimmerCallbacks *_callbacks) : BLEDimmer(_num, _callbacks) {}
  DimmerUI(AbstractDimmer& _dimmer) : dimmer(_dimmer) {}
  void setUpDownButtons(UpDownButtons& encoder);
  void setRotaryEncoder(Button& button, RotaryEncoder& encoder);
#ifdef DIMMER_WITH_TEMPERATURE
  void setTemperatureUpDownButtons(UpDownButtons& encoder);
  void setTemperatureRotaryEncoder(Button& button, RotaryEncoder& encoder);
#endif
protected:
  bool touchedOnOff();
  bool levelChanged();
  AbstractDimmer& dimmer;
};
};
#endif // _DIMMERUI_H_