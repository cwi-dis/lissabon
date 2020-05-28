#include "LedstripDimmer.h"

namespace Lissabon {

LedstripDimmer::LedstripDimmer(int _num, IotsaPixelstripMod& _mod, DimmerCallbacks *_callbacks)
: AbstractDimmer(num, _callbacks),
  mod(_mod)
{

}


void LedstripDimmer::setup() {
  mod.setPixelsource(this);
}

void LedstripDimmer::updateDimmer() {
  float newLevel = isOn ? level : 0;
  tempColor = TempFColor(temperature, newLevel);
  color = rgbwSpace.toRgbw(tempColor);

  int thisDuration = int(millisAnimationDuration * fabs(newLevel-prevLevel));
  millisAnimationStart = millis();
  millisAnimationEnd = millis() + thisDuration;
  iotsaConfig.postponeSleep(thisDuration+100);
}

#if 0

bool LedstripDimmer::changedBrightness() {
  isOn = true;
  setTI(tempColor.Temperature, tempColor.Brightness);
  updateDimmer(); // xxxjack quick=true
  // And prepare for saving (because we don't want to wear out the Flash chip)
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 2000;
  return true;
}

bool LedstripDimmer::changedOnOff() {
  // Start the animation to get to the wanted value
  updateDimmer();
  // And prepare for saving (because we don't want to wear out the Flash chip)
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 2000;
  return true;
}

void LedstripDimmer::setTI(float _temp, float _brightness) {
  if (_temp < 1000 || _temp > 10000) _temp = rgbwSpace.WTemperature; // Cater for coming from RGB or HLS value
  tempColor = TempFColor(_temp, _brightness);
  color = rgbwSpace.toRgbw(tempColor);

  IFDEBUG IotsaSerial.printf("setTI(%f, %f): r=%d g=%d b=%d w=%d\n", _temp, _brightness, color.R, color.G, color.B, color.W);
}
#endif

bool LedstripDimmer::available() {
  return true;
}


void LedstripDimmer::identify() {
  if (!buffer) return;
  memset(buffer, 255, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  memset(buffer, 0, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  memset(buffer, 255, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  memset(buffer, 0, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  updateDimmer();
}


void LedstripDimmer::setHandler(uint8_t *_buffer, size_t _count, int _bpp, IotsaPixelsourceHandler *_handler) {
  buffer = _buffer;
  count = _count;
  bpp = _bpp;
  stripHandler = _handler;
  updateDimmer(); // xxxjack quick=true
}

void LedstripDimmer::loop() {
  // Quick return if we have nothing to do
  if (millisAnimationStart == 0 || millisAnimationEnd == 0) return;

  // The color we want to go to
  RgbwFColor wanted = RgbwFColor(0);
  if (isOn) wanted = color;

  // Enable power to the led strip, if needed
  if (wanted.CalculateBrightness() > 0 || prevColor.CalculateBrightness() > 0) stripHandler->powerOn();

  // Determine how far along the animation we are, and terminate the animation when done (or if it looks preposterous)
  uint32_t thisDur = millisAnimationEnd - millisAnimationStart;
  if (thisDur == 0) thisDur = 1;
  float progress = float(millis() - millisAnimationStart) / float(thisDur);

  if (progress < 0) progress = 0;
  if (progress > 1) {
    progress = 1;
    millisAnimationStart = 0;
    prevColor = wanted;
    IFDEBUG IotsaSerial.printf("LedstripDimmer: isOn=%d r=%d, g=%d, b=%d, w=%d count=%d\n", isOn, color.R, color.G, color.B, color.W, count);
  }
  RgbwFColor cur = RgbwFColor::LinearBlend(prevColor, wanted, progress);
  RgbwColor pixelColor(cur);

#if 0
  IFDEBUG IotsaSerial.printf("LedstripDimmer::loop: r=%d, g=%d, b=%d, w=%d, count=%d, progress=%f\n", _r, _g, _b, _w, count, progress);
#endif
  if (buffer != NULL && count != 0 && stripHandler != NULL) {
    bool change = false;
    uint8_t *p = buffer;
    for (int i=0; i<count; i++) {
      RgbwColor thisPixelColor = pixelColor;
      if (*p != thisPixelColor.R) {
        *p = thisPixelColor.R;
        change = true;
      }
      p++;
      if (*p != thisPixelColor.G) {
        *p = thisPixelColor.G;
        change = true;
      }
      p++;
      if (*p != thisPixelColor.B) {
        *p = thisPixelColor.B;
        change = true;
      }
      p++;
      if (bpp == 4) {
        if (*p != thisPixelColor.W) {
          *p = thisPixelColor.W;
          change = true;
        }
        p++;
      }
    }
    if (change) {
      stripHandler->pixelSourceCallback();
    }
  }
  // Disable power to the led strip, if we can
  if ( millisAnimationStart == 0 && prevColor.CalculateBrightness() == 0) stripHandler->powerOff();
}

}