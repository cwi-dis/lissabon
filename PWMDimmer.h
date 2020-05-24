#ifndef _PWMDIMMER_H_
#define _PWMDIMMER_H_
#include "iotsa.h"
#include "AbstractDimmer.h"

namespace Lissabon {

class PWMDimmer : public AbstractDimmer {
public:
  PWMDimmer(int _num, int pin, int channel, DimmerCallbacks *_callbacks);
  void updateDimmer();
  bool available();
  void identify();
  void loop();
protected:
  int pin;
  int channel;

  float prevLevel;
  uint32_t millisAnimationStart;
  uint32_t millisAnimationEnd;
  int millisAnimationDuration;
};
};
#endif // _PWMDIMMER_H_