#include "PWMDimmer.h"

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
  // Quick return if we have nothing to do
  if (millisAnimationStart == 0 || millisAnimationEnd == 0) {
    // The dimmer shouldn't sleep if it is controlling the PWM output
    if (level > 0 && isOn) iotsaConfig.postponeSleep(100);
    return;
  }
  // Determine how far along the animation we are, and terminate the animation when done (or if it looks preposterous)
  uint32_t thisDur = millisAnimationEnd - millisAnimationStart;
  if (thisDur == 0) thisDur = 1;
  float progress = float(millis() - millisAnimationStart) / float(thisDur);
  float wantedLevel = level;
  if (!isOn) wantedLevel = 0;
  if (progress < 0) progress = 0;
  if (progress >= 1) {
    progress = 1;
    millisAnimationStart = 0;
    prevLevel = wantedLevel;

    IFDEBUG IotsaSerial.printf("IotsaDimmer: wantedLevel=%f level=%f\n", wantedLevel, level);
  }
  float curLevel = wantedLevel*progress + prevLevel*(1-progress);
  if (curLevel < 0) curLevel = 0;
  if (curLevel > 1) curLevel = 1;
  if (gamma && gamma != 1.0) curLevel = powf(curLevel, gamma);
#ifdef ESP32
  ledcWrite(channel, int(255*curLevel));
#else
  analogWrite(pin, int(255*curLevel));
#endif
}

}