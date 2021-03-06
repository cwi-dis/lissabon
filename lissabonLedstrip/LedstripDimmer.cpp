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
  // Reset RGBW-based color levels.
  if (calibrationMode == calibration_hard) calibrationMode = calibration_normal;
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

void LedstripDimmer::getHandler(JsonObject& reply) {
  reply["whiteTemperature"] = rgbwSpace.WTemperature;
  reply["whiteBrightness"] = rgbwSpace.WBrightness;
  reply["focalPoint"] = focalPoint;
  reply["focalSpread"] = focalSpread;
  reply["maxLevelCorrectColor"] = maxLevelCorrectColor();
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
    if (request.containsKey("calibrationData")) {
      JsonArray _calibrationData = request["calibrationData"];
      int size = _calibrationData.size();
      if (_calibrationData && (size == 4 || size == 8)) {
        for (int i =0; i<8; i++) {
          calibrationData[i] = _calibrationData[i % size];
        }
        calibrationMode = calibration_hard;
        millisAnimationStart = millisAnimationEnd = millis();
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

  argName = f_name + ".calibrationMode";
  if( server->hasArg(argName)) {
    calibrationMode = (CalibrationMode)server->arg(argName).toInt();
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
#if 1
  calibrationMode = calibration_normal;
#else
  int value;
  cf.get(f_name + ".calibrationMode", value, 0);
  calibrationMode = (CalibrationMode)value;
#endif
  updateColorspace(whiteTemperature, whiteBrightness);
  AbstractDimmer::configLoad(cf, f_name);
  return true;
}

void LedstripDimmer::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  cf.put(f_name + ".whiteTemperature", rgbwSpace.WTemperature);
  cf.put(f_name + ".whiteBrightness", rgbwSpace.WBrightness);
  cf.put(f_name + ".focalPoint", focalPoint);
  cf.put(f_name + ".focalSpread", focalSpread);
#if 0
  cf.put(f_name + ".calibrationMode", (int)calibrationMode);
#endif
  AbstractDimmer::configSave(cf, f_name);
}

void LedstripDimmer::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  AbstractDimmer::formHandler_fields(message, text, f_name, includeConfig);
  // Configuration parameters
  message += "White LED temperature: <input type='text' name='" + f_name + ".whiteTemperature' value='" + String(rgbwSpace.WTemperature) +"' ><br>";
  message += "White LED brightness: <input type='text' name='" + f_name + ".whiteBrightness' value='" + String(rgbwSpace.WBrightness) +"' ><br>";
  message += "Focal point: <input type='text' name='" + f_name + ".focalPoint' value='" + String(focalPoint) +"' > (0.0 is first LED, 1.0 is last LED)<br>";
  message += "Focal spread: <input type='text' name='" + f_name +" .focalSpread' value='" + String(focalSpread) +"' > (0.0 is narrow, 1.0 is as full width)<br>";
  String checkedNormal = calibrationMode == calibration_normal ? "checked" : "";
  String checkedRGB = calibrationMode == calibration_rgb ? "checked" : "";
  String checkedAlternate = calibrationMode ==calibration_alternating ? "checked" : "";
  message += "RGBW calibration mode: <input type='radio' name='" + f_name + ".calibrationMode' value='0' " + checkedNormal + "> Normal mode <input type='radio' name='calibrationMode' value='1' " + checkedRGB + "> RGB only <input type='radio' name='calibrationMode' value='2' " + checkedAlternate + "> Alternate RGB and RGBW LEDs<br>";
  message += "(hard color calibration can only be set through REST interface calibrationData)<br>";
  message += colorDump();
}

void LedstripDimmer::formHandler_TD(String& message, bool includeConfig) {

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
      if (calibrationMode == calibration_hard) {
        if (i&1) {
          thisPixelFColor = RgbwFColor(calibrationData[4], calibrationData[5], calibrationData[6], calibrationData[7]);
        } else {
          thisPixelFColor = RgbwFColor(calibrationData[0], calibrationData[1], calibrationData[2], calibrationData[3]);
        }
      } else if (calibrationMode == calibration_rgb || (calibrationMode == calibration_alternating && (i&1))) {
        // In calibration_rgb mode we do not use the white channel.
        // In calibration_alternating mode, odd pixels show the RGB color and even pixels the RGBW color. This
        // checking the white led temperature and intensity, because for levels that are attainable
        // using only the RGB LEDs the resulting light should be the same intensity and color.
        thisPixelFColor = curRgbCalibrationColor;
      }
      thisPixelFColor = thisPixelFColor.Dim(pixelLevels[i]);
      RgbwColor thisPixelColor = thisPixelFColor;
      if (calibrationMode && i <= 1) {
        IotsaSerial.printf("pixel %d: r=%d g=%d b=%d w=%d\n", i, thisPixelColor.R, thisPixelColor.G, thisPixelColor.B, thisPixelColor.W);
      }
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