#include "PWMDImmer.h"

namespace Lissabon {

PWMDimmer::PWMDimmer(int _num, int _pin, int _channel, DimmerCallbacks *_callbacks)
: AbstractDimmer(num, _callbacks),
  pin(_pin),
  channel(_channel)
{

}


void PWMDimmer::setup() {
#ifdef ESP32
  ledcSetup(channel, pwmFrequency, 8);
  ledcAttachPin(pin, channel);
#else
  pinMode(pin, OUTPUT);
#endif
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


void PWMDimmer::identify() {
#ifdef ESP32
  ledcWrite(channel, 128);
  delay(100);
  ledcWrite(channel, 0);
  delay(100);
  ledcWrite(channel, 128);
  delay(100);
  ledcWrite(channel, 0);
  delay(100);
#else
  analogWrite(pin, 128);
  delay(100);
  analogWrite(pin, 0);
  delay(100);
  analogWrite(pin, 128);
  delay(100);
  analogWrite(pin, 0);
  delay(100);
#endif
  updateDimmer();
}

void PWMDimmer::loop() {

}

}