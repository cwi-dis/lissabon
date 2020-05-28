#ifndef _LEDSTRIPDIMMER_H_
#define _LEDSTRIPDIMMER_H_
#include "iotsa.h"
#include "AbstractDimmer.h"
#include "iotsaPixelStrip.h"

#include "NPBColorLib.h"

namespace Lissabon {

class LedstripDimmer : public AbstractDimmer, public IotsaPixelsource {
public:
  LedstripDimmer(int _num, IotsaPixelstripMod& _mod, DimmerCallbacks *_callbacks);
  void setup();
  void updateDimmer();
  bool available();
  void identify();
  void loop();

  void setHandler(uint8_t *_buffer, size_t _count, int bpp, IotsaPixelsourceHandler *handler);
protected:
  IotsaPixelstripMod& mod;
  float prevLevel;
  uint32_t millisAnimationStart;
  uint32_t millisAnimationEnd;
  int millisAnimationDuration;
  uint8_t *buffer;
  int count;  // Number of LEDs
  int bpp; // Number of colors per LED (3 or 4)
  IotsaPixelsourceHandler *stripHandler;

  TempFColor tempColor;
  Colorspace rgbwSpace;

  RgbwColor color;
  RgbwColor prevColor;
#if 0
  bool isOn;  // Master boolean: if false there is no light.



  HslFColor hslColor;
  bool hslColorIsValid;

  uint32_t millisStartAnimation;
  int millisAnimationDuration;
  int millisThisAnimationDuration;

  int darkPixels; // Number of unlit pixels between lit pixels
#endif
};
};
#endif // _LEDSTRIPDIMMER_H_