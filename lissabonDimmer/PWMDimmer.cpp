#include "PWMDimmer.h"

namespace Lissabon {

PWMDimmer::PWMDimmer(int _num, int _pin, int _channel, DimmerCallbacks *_callbacks)
: AbstractDimmer(_num, _callbacks),
  pin(_pin),
  channel(_channel)
{

}

#ifdef DIMMER_WITHOUT_LEVEL
void PWMDimmer::switchLevel(bool on) {
  if (on) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  } else {
    pinMode(pin, INPUT);
  }
}
#endif

void PWMDimmer::setup() {
#ifdef DIMMER_WITHOUT_LEVEL
  switchLevel(false);
#else
#ifdef ESP32
#ifndef newer
  pinMode(pin, OUTPUT);
  ledcSetup(channel, pwmFrequency, 8);
  ledcAttachPin(pin, channel);
#else
  ledcAttachChannel(pin, pwmFrequency, 8);
#endif
#else
  pinMode(pin, OUTPUT);
#endif
#endif
}

bool PWMDimmer::available() {
  return true;
}


void PWMDimmer::identify() {
  IotsaSerial.printf("xxxjack identify dimmer%d channel %d pin %d\n", num, channel, pin);
#ifdef DIMMER_WITHOUT_LEVEL
  switchLevel(true);
  delay(100);
  switchLevel(false);
  delay(100);
  switchLevel(true);
  delay(100);
  switchLevel(false);
  delay(100);
#else
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
#endif
  updateDimmer();
}

void PWMDimmer::loop() {
  // Quick return if we have nothing to do
#ifdef DIMMER_WITHOUT_LEVEL
  if (isOn) {
    iotsaConfig.postponeSleep(100);
  }
#else
  if (animationStartMillis == 0 || animationEndMillis == 0) {
    // The dimmer shouldn't sleep if it is controlling the PWM output
    if (level > 0 && isOn) iotsaConfig.postponeSleep(100);
    return;
  }
#endif
  calcCurLevel();
#ifdef DIMMER_WITHOUT_LEVEL
  switchLevel(isOn);
#else
#ifdef ESP32
  ledcWrite(channel, int(255*curLevel));
#else
  analogWrite(pin, int(255*curLevel));
#endif
#endif
}

}