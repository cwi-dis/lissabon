#ifndef _PWMDIMMER_H_
#define _PWMDIMMER_H_
#include "iotsa.h"
#include "AbstractDimmer.h"

namespace Lissabon {

class PWMDimmer : public AbstractDimmer {
public:
  PWMDimmer(int _num, int pin, int channel, DimmerCallbacks *_callbacks);
  void setup();
  bool available();
  void identify();
  void loop();
protected:
  int pin;
  int channel;

#ifdef DIMMER_WITHOUT_LEVEL
  void switchLevel(bool on);
#endif
};
};
#endif // _PWMDIMMER_H_