#ifndef _PWMDIMMER_H_
#define _PWMDIMMER_H_
#include "iotsa.h"
#include "AbstractDimmer.h"

namespace Lissabon {

class PWMDimmer : public AbstractDimmer {
public:
  PWMDimmer(int _num, int pin, DimmerCallbacks *_callbacks);
  void updateDimmer();
  bool available();
  void identify();
  void loop();
protected:
  int pin;

  float prevLevel;
  float gamma;
#ifdef ESP32
  float pwmFrequency;
#endif
  uint32_t millisAnimationStart;
  uint32_t millisAnimationEnd;
  int millisAnimationDuration;
};
};
#endif // _PWMDIMMER_H_