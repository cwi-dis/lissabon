#include "PWMDImmer.h"

namespace Lissabon {

PWMDimmer::PWMDimmer(int _num, int pin, DimmerCallbacks *_callbacks)
: AbstractDimmer(num, _callbacks)
{

}

void PWMDimmer::updateDimmer() {
  float newLevel = isOn ? level : 0;
  int thisDuration = int(millisAnimationDuration * fabs(newLevel-prevLevel));
  millisAnimationStart = millis();
  millisAnimationEnd = millis() + thisDuration;
  iotsaConfig.postponeSleep(thisDuration+100);
}

bool PWMDimmer::available() {
  return true;
}

void PWMDimmer::loop() {

}

}