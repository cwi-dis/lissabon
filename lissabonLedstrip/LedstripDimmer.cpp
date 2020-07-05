#include "LedstripDimmer.h"
#include <cmath>

//const float PI = atan(1)*4;
const float SQRT_HALF = sqrt(0.5);

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
  // Compute level and animation duration
  AbstractDimmer::updateDimmer();
  // Compute curve.
  // 
  // First determine the maximum output level at which no color distortion happens.
  // If we want more light than this we have to use all LEDs.
  // Also if the spread is very wide we ignore it and light up all LEDs.
  //
  float maxOutputLevel = maxLevelCorrectColor();
  if (level == 0 || level >= maxOutputLevel || focalSpread > 0.9) {
    for(int i=0; i<count; i++) pixelLevels[i] = 1.0;
    overrideLevel = -1; // We use the level as provided
    return;
  }
  // We will compute the per-pixel level, so we set the overrideLevel to maximum color-preserving.
  overrideLevel = maxOutputLevel;
  //
  // Now determine how much light we can produce with the preferred curve
  //
  float curSpread = focalSpread;
  float beginValue = levelFuncCumulative(0, curSpread);
  float endValue = levelFuncCumulative(1, curSpread);
  float cumulativeValue = endValue - beginValue;
  //IotsaSerial.printf("xxxjack level=%f focalSpread=%f maxLevel=%f totalValue=%f\n", level, curSpread, maxOutputLevel, cumulativeValue);
  //
  // With this curve we can produce cumulativeValue*maxOutputLevel light.
  // If that is not enough we widen the curve.
  //
  while (level > cumulativeValue*maxOutputLevel && curSpread < 1) {
    curSpread = curSpread + 0.05;
    beginValue = levelFuncCumulative(0, curSpread);
    endValue = levelFuncCumulative(1, curSpread);
    cumulativeValue = endValue - beginValue;
    //IotsaSerial.printf("xxxjack curSpread=%f maxLevel=%f totalValue=%f\n", curSpread, maxOutputLevel, cumulativeValue);
  }
  //
  // cumulativeValue*maxOutputLevel is the amount of light produced. We need to
  // correct this so level is what is actually produced.
  float correction = level / cumulativeValue*maxOutputLevel;
  if (pixelLevels != NULL) {
    for(int i=0; i<count; i++) {
      float peakValue = levelFunc((float)(i+1)/count, curSpread) * correction;
      pixelLevels[i] = peakValue;
    }
  }
}

void LedstripDimmer::updateColorspace(float whiteTemperature, float whiteBrightness) {
  rgbwSpace = Colorspace(whiteTemperature, whiteBrightness, false, false);
}

float LedstripDimmer::maxLevelCorrectColor() {
    // Check the maximum brightness we can correctly render and remember that
  Colorspace rgbwSpaceCorrect = Colorspace(rgbwSpace.WTemperature, rgbwSpace.WBrightness, true, false);
  TempFColor maxTFColor(temperature, 1.0);
  RgbwFColor maxRgbwColor = rgbwSpaceCorrect.toRgbw(maxTFColor);
  float maxLevel = maxRgbwColor.CalculateTrueBrightness(rgbwSpace.WBrightness);
  //IotsaSerial.printf("xxxjack wTemp=%f wBright=%f wtdTemp=%f R=%f G=%f B=%f W=%f maxCorrect=%f\n", rgbwSpace.WTemperature, rgbwSpace.WTemperature, temperature, maxRgbwColor.R, maxRgbwColor.G, maxRgbwColor.B, maxRgbwColor.W, maxLevel);
  return maxLevel;
}

float LedstripDimmer::levelFunc(float x, float spreadOverride) {
  // Light distribution function. Returns light level for position x (0<=x<1).
  // First determine which spread we want to use
  float spread = spreadOverride;
  if (spread < 0) spread = focalSpread;
  // If spread is close to 1.0 we use all lights
  if (spread > 0.99) return 1;
  // We turn spread into a [0, inf) number
  spread = (1/(1-powf(spread, 2)))-1;
  // Now we can compute the level function. Similar shape to a normal distribution.
  float xprime = (x-focalPoint)/spread;
  return exp(-powf(xprime, 2));
}

float LedstripDimmer::levelFuncCumulative(float x, float spreadOverride) {
  // Culumative light function, integral of levelFunc. See levelFunc for details.
  // First determine which spread we want to use
  float spread = spreadOverride;
  if (spread < 0) spread = focalSpread;
  // If spread is close to 1.0 we use all lights
  if (spread > 0.99) return 1;
  // We turn spread into a [0, inf) number
  spread = (1/(1-powf(spread, 2)))-1;
  // Now we can compute the level function. Similar shape to a normal distribution.
  float xprime = (x-focalPoint)/spread;
  return -0.5*sqrt(PI)*spread*erf(-xprime);
}


bool LedstripDimmer::available() {
  return true;
}

bool LedstripDimmer::getHandler(JsonObject& reply) {
  reply["whiteTemperature"] = rgbwSpace.WTemperature;
  reply["whiteBrightness"] = rgbwSpace.WBrightness;
  reply["focalPoint"] = focalPoint;
  reply["focalSpread"] = focalSpread;
  reply["maxLevelCorrectColor"] = maxLevelCorrectColor();
  return AbstractDimmer::getHandler(reply);
}
bool LedstripDimmer::putHandler(const JsonVariant& request) {
  return AbstractDimmer::putHandler(request);
}
bool LedstripDimmer::putConfigHandler(const JsonVariant& request) {
  float whiteTemperature = request["whiteTemperature"]|rgbwSpace.WTemperature;
  float whiteBrightness = request["whiteBrightness"]|rgbwSpace.WBrightness;
  focalPoint = request["focalPoint"] | focalPoint;
  focalSpread = request["focalSpread"] | focalSpread;
  updateColorspace(whiteTemperature, whiteBrightness);
  return AbstractDimmer::putConfigHandler(request);
}
bool LedstripDimmer::handlerArgs(IotsaWebServer *server) {
  return AbstractDimmer::handlerArgs(server);
}
bool LedstripDimmer::handlerConfigArgs(IotsaWebServer *server) {
  bool anyChanged;
  float whiteTemperature = rgbwSpace.WTemperature;
  float whiteBrightness = rgbwSpace.WBrightness;
  if( server->hasArg("whiteTemperature")) {
    whiteTemperature = server->arg("whiteTemperature").toFloat();
    anyChanged = true;
  }
  if( server->hasArg("whiteBrightness")) {
    whiteBrightness = server->arg("whiteBrightness").toFloat();
    anyChanged = true;
  }
  if( server->hasArg("focalPoint")) {
    focalPoint = server->arg("focalPoint").toFloat();
    anyChanged = true;
  }
  if( server->hasArg("focalSpread")) {
    focalSpread = server->arg("focalSpread").toFloat();
    anyChanged = true;
  }
  if( server->hasArg("calibrationMode")) {
    calibrationMode = server->arg("calibrationMode").toInt();
    anyChanged = true;
  }
  if (anyChanged) {
    updateColorspace(whiteTemperature, whiteBrightness);
    updateDimmer();
  }
  return AbstractDimmer::handlerConfigArgs(server);
}
void LedstripDimmer::configLoad(IotsaConfigFileLoad& cf) {
  float whiteTemperature, whiteBrightness;
  cf.get("whiteTemperature", whiteTemperature, 4000);
  cf.get("whiteBrightness", whiteBrightness, 1.0);
  cf.get("focalPoint", focalPoint, 0.5);
  cf.get("focalSpread", focalSpread, 1.0);
  int value;
  cf.get("calibrationMode", value, 0);
  calibrationMode = value;
  updateColorspace(whiteTemperature, whiteBrightness);
  AbstractDimmer::configLoad(cf);
}
void LedstripDimmer::configSave(IotsaConfigFileSave& cf) {
  cf.put("whiteTemperature", rgbwSpace.WTemperature);
  cf.put("whiteBrightness", rgbwSpace.WBrightness);
  cf.put("focalPoint", focalPoint);
  cf.put("focalSpread", focalSpread);
  cf.put("calibrationMode", (int)calibrationMode);
  AbstractDimmer::configSave(cf);
}
String LedstripDimmer::handlerForm() {
  return AbstractDimmer::handlerForm();
}
String LedstripDimmer::handlerConfigForm() {
  String message = AbstractDimmer::handlerConfigForm();
  String s_num = String(num);
  String s_name = "dimmer" + s_num;
  message += "<h2>Dimmer " + s_num + " extended configuration</h2><form method='get'>";
  message += "White LED temperature: <input type='text' name='whiteTemperature' value='" + String(rgbwSpace.WTemperature) +"' ><br>";
  message += "White LED brightness: <input type='text' name='whiteBrightness' value='" + String(rgbwSpace.WBrightness) +"' ><br>";
  message += "Focal point: <input type='text' name='focalPoint' value='" + String(focalPoint) +"' > (0.0 is first LED, 1.0 is last LED)<br>";
  message += "Focal spread: <input type='text' name='focalSpread' value='" + String(focalSpread) +"' > (0.0 is narrow, 1.0 is as full width)<br>";
  String checkedOn = calibrationMode ? "checked" : "";
  String checkedOff = calibrationMode ? "" : "checked";
  message += "RGBW calibration mode: <input type='radio' name='calibrationMode' value='1' " + checkedOn + "> On <input type='radio' name='calibrationMode' value='0' " + checkedOff + "> Off<br>";
  message += "<input type='submit'></form>";
  message += "<p>Maximum level with correct color: " + String(maxLevelCorrectColor()) + " (at temperature " + String(temperature) + ")</p>";
  return message;
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
  if (pixelLevels != NULL) free(pixelLevels);
  pixelLevels = (float *)calloc(count, sizeof(float));
  stripHandler = _handler;
  updateDimmer();
}

void LedstripDimmer::loop() {
  // Quick return if we have nothing to do
  if (millisAnimationStart == 0 || millisAnimationEnd == 0) return;

  // Compute current level (taking into account isOn and animation progress)
  calcCurLevel();

  // Enable power to the led strip, if needed
  if (curLevel > 0 || prevLevel > 0) stripHandler->powerOn();

  // The color we want to go to
  TempFColor curTFColor = TempFColor(temperature, curLevel);
  RgbFColor curRgbCalibrationColor(curTFColor);
  RgbwFColor curRgbwColor = rgbwSpace.toRgbw(curTFColor);
#ifdef COMPUTE_8_BIT
  RgbwColor curColor = curRgbwColor;
#endif
#if 0
  if (millisAnimationStart == 0) {
    IFDEBUG IotsaSerial.printf("LedstripDimmer: isOn=%d r=%d, g=%d, b=%d, w=%d count=%d\n", isOn, color.R, color.G, color.B, color.W, count);
  }
#endif
  if (buffer != NULL && count != 0 && stripHandler != NULL) {
    bool change = false;
    uint8_t *p = buffer;
    for (int i=0; i<count; i++) {
#ifdef COMPUTE_8_BIT
      RgbwColor thisPixelColor = curColor;
      thisPixelColor = thisPixelColor.Dim((uint8_t)(255.0*pixelLevels[i]));
#else
      RgbwFColor thisPixelFColor = curRgbwColor;
      if (calibrationMode && (i&1)) {
        // In calibration mode, odd pixels show the RGB color and even pixels the RGBW color. This
        // checking the white led temperature and intensity, because for levels that are attainable
        // using only the RGB LEDs the resulting light should be the same intensity and color.
        thisPixelFColor = curRgbCalibrationColor;
      }
      thisPixelFColor = thisPixelFColor.Dim(pixelLevels[i]);
      RgbwColor thisPixelColor = thisPixelFColor;
#endif
      //IotsaSerial.printf("xxxjack i=%d level=%f curW=%f thisW=%f\n", i, pixelLevels[i], curRgbwColor.W, thisPixelFColor.W);
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
  if ( millisAnimationStart == 0 && curLevel == 0) stripHandler->powerOff();
}

}