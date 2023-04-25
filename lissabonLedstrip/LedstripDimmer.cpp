#include "LedstripDimmer.h"
#include <cmath>

//const float PI = atan(1)*4;
const float SQRT_HALF = sqrt(0.5);

#define DEBUG_LEDSTRIP if(1)

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
  calcLevel();
  // xxxjack calling calcPixelLevels here (in stead of in loop) means the curve
  // remains the same during fades. Calling it in loop() would fix that.
  calcPixelLevels(level);
  // Compute animation duration, which signals to loop() that things neeed to change.
  AbstractDimmer::updateDimmer();
}

void LedstripDimmer::calcLevel() {
  float maxOutputLevel = maxLevelCorrectColor();
  if (level > maxOutputLevel) {
    IotsaSerial.printf("LodstripDimmer.calcLevel: level clamped to %f\n", maxOutputLevel);
    level = maxOutputLevel;
  }
}

void LedstripDimmer::calcPixelLevels(float wantedLevel) {
  //
  // Compute curve.
  // 
  // If spread==0 we use a uniform curve.
  if (focalSpread < 0.01 || focalSpread > 0.99) {
    for(int i=0; i<count; i++) {
      pixelLevels[i] = 1.0;
    }
    return;
  }
  //
  // Determine how much light we can produce with the preferred curve
  //
  float curSpread = focalSpread;
  float beginValue = levelFuncCumulative(0, curSpread);
  float endValue = levelFuncCumulative(1, curSpread);
  float cumulativeValue = endValue - beginValue;
  DEBUG_LEDSTRIP IotsaSerial.printf("LedstripDimmer.calcPixelLevels: wantedLevel=%f curSpread=%f cumulativeValue=%f\n", wantedLevel, curSpread, cumulativeValue);
  //
  // With this curve we can produce cumulativeValue light.
  // If that is not enough we widen the curve.
  // xxxjack there must be a mathematical to do this, in stead of approximating.
  //
  if (wantedLevel > cumulativeValue) {
    while (wantedLevel > cumulativeValue && curSpread < 1) {
      curSpread = (curSpread + 0.05)*1.05;
      beginValue = levelFuncCumulative(0, curSpread);
      endValue = levelFuncCumulative(1, curSpread);
      cumulativeValue = endValue - beginValue;
    }
    DEBUG_LEDSTRIP IotsaSerial.printf("LedstripDimmer.calcPixelLevels: wantedLevel=%f adjusted_curSpread=%f cumulativeValue=%f\n", wantedLevel, curSpread, cumulativeValue);
  }

  //
  // cumulativeValue is the amount of light produced. We need to
  // correct this so level is what is actually produced.
  float correction = wantedLevel / cumulativeValue;
  float sumLevel = 0;
  if (pixelLevels != NULL) {
    for(int i=0; i<count; i++) {
      float thisValue = levelFunc((float)(i+1)/count, curSpread) * correction;
      pixelLevels[i] = thisValue;
      DEBUG_LEDSTRIP IotsaSerial.printf("LedstripDimmer.calcPixelLevels: pixelLevel[%d] = %f\n", i, thisValue);
      sumLevel += thisValue;
    }
  }
  DEBUG_LEDSTRIP IotsaSerial.printf("LedstripDimmer.calcPixelLevels: sum(pixelLevel)=%f avg=%f\n", sumLevel, sumLevel/configNUM_THREAD_LOCAL_STORAGE_POINTERS);
  DEBUG_LEDSTRIP IotsaSerial.printf("LedstripDimmer.calcPixelLevels: pixelLevels updated\n");
}

void LedstripDimmer::updateColorspace(float whiteTemperature, float whiteBrightness) {
  rgbwSpace = Colorspace(whiteTemperature, whiteBrightness, true, false);
}

float LedstripDimmer::maxLevelCorrectColor() {
    // Check the maximum brightness we can correctly render and remember that
  Colorspace rgbwSpaceCorrect = Colorspace(rgbwSpace.WTemperature, rgbwSpace.WBrightness, true, false);
  TempFColor maxTFColor(temperature, 1.0);
  correctRgbwColor = rgbwSpaceCorrect.toRgbw(maxTFColor);
  float maxLevel = correctRgbwColor.CalculateTrueBrightness(rgbwSpace.WBrightness);
  DEBUG_LEDSTRIP IotsaSerial.printf("LedstripDimmer.maxLevelCorrectColor: wTemp=%f wBright=%f wtdTemp=%f R=%f G=%f B=%f W=%f maxCorrect=%f\n", rgbwSpace.WTemperature, rgbwSpace.WTemperature, temperature, correctRgbwColor.R, correctRgbwColor.G, correctRgbwColor.B, correctRgbwColor.W, maxLevel);
  return maxLevel;
}

String LedstripDimmer::colorDump() {
  String rv;
  float maxLevel = maxLevelCorrectColor();
  rv += "<p>Maximum level with correct color: " + String(maxLevel) + " (at temperature " + String(temperature) + ")<br>";
  TempFColor maxColor(temperature, maxLevel);
  RgbFColor maxFColor(maxColor);
  RgbColor maxColor8bit(maxFColor);
  HtmlColor htmlColor(maxColor8bit);
  rv += "RGB Levels: r=" + String(maxColor8bit.R) + ", g=" + String(maxColor8bit.G) + ", b=" + String(maxColor8bit.B) + ", hex 0x" + String(htmlColor.Color, HEX) + "<br>";
  rv += "Looks like: <svg width='40' height='40'><rect width='40' height='40' style='fill:#" + String(htmlColor.Color, HEX) + ";stroke-width:23;stroke:rgb(0,0,0)' /></svg>";
  rv += "</p>";
  return rv;
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
  if (spread > 0.99) return x;
  // We turn spread into a [0, inf) number
  spread = (1/(1-powf(spread, 2)))-1;
  // Now we can compute the level function. Similar shape to a normal distribution.
  float xprime = (x-focalPoint)/spread;
  return -0.5*sqrt(PI)*spread*erf(-xprime);
}


bool LedstripDimmer::available() {
  return true;
}

void LedstripDimmer::getHandler(JsonObject& reply) {
  reply["whiteTemperature"] = rgbwSpace.WTemperature;
  reply["whiteBrightness"] = rgbwSpace.WBrightness;
  reply["focalPoint"] = focalPoint;
  reply["focalSpread"] = focalSpread;
  reply["maxLevelCorrectColor"] = maxLevelCorrectColor();
  reply["maxLevelCorrectColorTemperature"] = temperature;
  reply["inCalibrationMode"] = (int)inCalibrationMode;
  AbstractDimmer::getHandler(reply);
}

bool LedstripDimmer::putHandler(const JsonVariant& request) {
  AbstractDimmer::putHandler(request);
  // Handle configuration settings
  if (true) {
    float whiteTemperature = request["whiteTemperature"]|rgbwSpace.WTemperature;
    float whiteBrightness = request["whiteBrightness"]|rgbwSpace.WBrightness;
    focalPoint = request["focalPoint"] | focalPoint;
    focalSpread = request["focalSpread"] | focalSpread;
    inCalibrationMode = request["inCalibrationMode"] | 0;
    if (request.containsKey("calibrationData")) {
      JsonArray _calibrationData = request["calibrationData"];
      int size = _calibrationData.size();
      if (_calibrationData && (size == 4 || size == 8)) {
        for (int i =0; i<8; i++) {
          calibrationData[i] = _calibrationData[i % size];
        }
        animationStartMillis = animationEndMillis = millis();
        IFDEBUG IotsaSerial.println("Got calibrationData");
      } else {
        IFDEBUG IotsaSerial.println("Bad calibrationData");
      }
    }
    updateColorspace(whiteTemperature, whiteBrightness);
  }
  return true;
}

bool LedstripDimmer::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  bool anyChanged = AbstractDimmer::formHandler_args(server, f_name, includeConfig);
  // Configuration settings
  float whiteTemperature = rgbwSpace.WTemperature;
  float whiteBrightness = rgbwSpace.WBrightness;

  String argName = f_name + ".whiteTemperature";
  if( server->hasArg(argName)) {
    whiteTemperature = server->arg(argName).toFloat();
    anyChanged = true;
  }

  argName = f_name + ".whiteBrightness";
  if( server->hasArg(argName)) {
    whiteBrightness = server->arg(argName).toFloat();
    anyChanged = true;
  }

  argName = f_name + ".focalPoint";
  if( server->hasArg(argName)) {
    focalPoint = server->arg(argName).toFloat();
    anyChanged = true;
  }

  argName = f_name + ".focalSpread";
  if( server->hasArg(argName)) {
    focalSpread = server->arg(argName).toFloat();
    anyChanged = true;
  }

  if (anyChanged) {
    updateColorspace(whiteTemperature, whiteBrightness);
    updateDimmer();
  }
  return anyChanged;
}

bool LedstripDimmer::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  float whiteTemperature, whiteBrightness;
  cf.get(f_name + ".whiteTemperature", whiteTemperature, 4000);
  cf.get(f_name + ".whiteBrightness", whiteBrightness, 1.0);
  cf.get(f_name + ".focalPoint", focalPoint, 0.5);
  cf.get(f_name + ".focalSpread", focalSpread, 1.0);
  updateColorspace(whiteTemperature, whiteBrightness);
  AbstractDimmer::configLoad(cf, f_name);
  return true;
}

void LedstripDimmer::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  cf.put(f_name + ".whiteTemperature", rgbwSpace.WTemperature);
  cf.put(f_name + ".whiteBrightness", rgbwSpace.WBrightness);
  cf.put(f_name + ".focalPoint", focalPoint);
  cf.put(f_name + ".focalSpread", focalSpread);
  AbstractDimmer::configSave(cf, f_name);
}

void LedstripDimmer::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  AbstractDimmer::formHandler_fields(message, text, f_name, includeConfig);
  if (includeConfig) {
    // Configuration parameters
    message += "White LED temperature: <input type='text' name='" + f_name + ".whiteTemperature' value='" + String(rgbwSpace.WTemperature) +"' ><br>";
    message += "White LED brightness: <input type='text' name='" + f_name + ".whiteBrightness' value='" + String(rgbwSpace.WBrightness) +"' ><br>";
    message += "Focal point: <input type='text' name='" + f_name + ".focalPoint' value='" + String(focalPoint) +"' > (0.0 is first LED, 1.0 is last LED)<br>";
    message += "Focal spread: <input type='text' name='" + f_name +".focalSpread' value='" + String(focalSpread) +"' > (0.0 is narrow, 1.0 is as full width)<br>";
    message += "(Color calibration can only be done through REST interface calibrationData)<br>";
    message += colorDump();
  }
}

void LedstripDimmer::formHandler_TD(String& message, bool includeConfig) {

}

void LedstripDimmer::identify() {
  if (!pixelBuffer) return;
  memset(pixelBuffer, 255, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  memset(pixelBuffer, 0, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  memset(pixelBuffer, 255, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  memset(pixelBuffer, 0, count*bpp);
  stripHandler->pixelSourceCallback();
  delay(100);
  updateDimmer();
}


void LedstripDimmer::setHandler(uint8_t *_buffer, size_t _count, int _bpp, IotsaPixelsourceHandler *_handler) {
  pixelBuffer = _buffer;
  count = _count;
  bpp = _bpp;
  if (pixelLevels != NULL) free(pixelLevels);
  pixelLevels = (float *)calloc(count, sizeof(float));
  stripHandler = _handler;
  updateDimmer();
}

void LedstripDimmer::loop() {
  // If we are not completely setup we return.
  if (pixelBuffer == NULL || count == 0 || stripHandler == NULL) return;
  // Quick return if we have nothing to do
  if (animationStartMillis == 0 || animationEndMillis == 0) return;
  //
  // If we are in calibration mode we simply set the pixels and be done
  //
  if (inCalibrationMode) {
    uint8_t *p = pixelBuffer;
    for (int i=0; i<count; i++) {
      RgbwFColor thisPixelFColor;
      if (i&1) {
        thisPixelFColor = RgbwFColor(calibrationData[4], calibrationData[5], calibrationData[6], calibrationData[7]);
      } else {
        thisPixelFColor = RgbwFColor(calibrationData[0], calibrationData[1], calibrationData[2], calibrationData[3]);
      }
      RgbwColor thisPixelColor = thisPixelFColor;
      *p++ = thisPixelColor.R;
      *p++ = thisPixelColor.G;
      *p++ = thisPixelColor.B;
      if (bpp == 4) *p++ = thisPixelColor.W;
    }
    stripHandler->pixelSourceCallback();
    return;
  }
  //
  // Compute current level (taking into account isOn and animation progress)
  //
  calcCurLevel();
  // Only for calibration mode rgb or alternating we also need to color in RGB-only.
  // We adjust for the brightness of the white LED

  uint8_t *p = pixelBuffer;
  
  for (int i=0; i<count; i++) {
    float thisLevel = curLevel*pixelLevels[i];
    RgbwFColor thisPixelFColor = correctRgbwColor.Dim(thisLevel);
    RgbwColor thisPixelColor = thisPixelFColor;
    DEBUG_LEDSTRIP IotsaSerial.printf("LedstripDimmer.loop: pixel %d: level=%f r=%d g=%d b=%d w=%d\n", i, thisLevel, thisPixelColor.R, thisPixelColor.G, thisPixelColor.B, thisPixelColor.W);
    *p++ = thisPixelColor.R;
    *p++ = thisPixelColor.G;
    *p++ = thisPixelColor.B;
    if (bpp == 4) *p++ = thisPixelColor.W;
  }
  stripHandler->pixelSourceCallback();
}

}