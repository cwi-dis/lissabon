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
  AbstractDimmer::updateDimmer();
  // Compute curve
  float maxLevel = calculateMaxCorrectColorLevel();
  if (pixelLevels != NULL) {
    for(int i=0; i<count; i++) {
      pixelLevels[i] = levelFunc((float)i/(float)count); 
    }
  }
}

void LedstripDimmer::updateColorspace(float whiteTemperature, float whiteBrightness) {
  rgbwSpace = Colorspace(whiteTemperature, whiteBrightness, false, false);
}

float LedstripDimmer::calculateMaxCorrectColorLevel() {
    // Check the maximum brightness we can correctly render and remember that
  Colorspace rgbwSpaceCorrect = Colorspace(rgbwSpace.WTemperature, rgbwSpace.WBrightness, true, false);
  TempFColor maxTFColor(temperature, 1.0);
  RgbwFColor maxRgbwColor = rgbwSpaceCorrect.toRgbw(maxTFColor);
  float maxLevel = maxRgbwColor.CalculateTrueBrightness(rgbwSpace.WBrightness);
  IotsaSerial.printf("xxxjack wTemp=%f wBright=%f wtdTemp=%f R=%f G=%f B=%f W=%f maxCorrect=%f\n", rgbwSpace.WTemperature, rgbwSpace.WTemperature, temperature, maxRgbwColor.R, maxRgbwColor.G, maxRgbwColor.B, maxRgbwColor.W, maxLevel);
  return maxLevel;
}

float LedstripDimmer::levelFunc(float x) {
  // Light distribution function. Returns light level for position x (0<=x<1).
  if (x < 0) x = 0;
  if (x > 1) x = 1;
  // First try: linear ramp
  if (x < focalPoint - focalSharpness) return 0;
  if (x > focalPoint + focalSharpness) return 0;
  float delta = fabs(x - focalPoint);
  if (focalSharpness == 0) return 1;
  return 1-(delta / focalSharpness);
}

float LedstripDimmer::levelFuncCumulative(float x) {
  // Culumative light function. Called only for x=0 and x=1. Provided
  // so we can use mathematical formulas here and in levelFunc for functions
  // functions that are not bounded to the range [0, 1)
  if (x < 0) x = 0;
  if (x > 1) x = 1;
  // First try: linear ramp. Only correct at 0 and 1
  if (x < focalPoint) return 0;
  return focalSharpness;
}


bool LedstripDimmer::available() {
  return true;
}

bool LedstripDimmer::getHandler(JsonObject& reply) {
  reply["whiteTemperature"] = rgbwSpace.WTemperature;
  reply["whiteBrightness"] = rgbwSpace.WBrightness;
  reply["focalPoint"] = focalPoint;
  reply["focalSharpness"] = focalSharpness;
  return AbstractDimmer::getHandler(reply);
}
bool LedstripDimmer::putHandler(const JsonVariant& request) {
  return AbstractDimmer::putHandler(request);
}
bool LedstripDimmer::putConfigHandler(const JsonVariant& request) {
  float whiteTemperature = request["whiteTemperature"]|rgbwSpace.WTemperature;
  float whiteBrightness = request["whiteBrightness"]|rgbwSpace.WBrightness;
  focalPoint = request["focalPoint"] | focalPoint;
  focalSharpness = request["focalSharpness"] | focalSharpness;
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
  if( server->hasArg("focalSharpness")) {
    focalSharpness = server->arg("focalSharpness").toFloat();
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
  cf.get("focalSharpness", focalSharpness, 0);
  updateColorspace(whiteTemperature, whiteBrightness);
  AbstractDimmer::configLoad(cf);
}
void LedstripDimmer::configSave(IotsaConfigFileSave& cf) {
  cf.put("whiteTemperature", rgbwSpace.WTemperature);
  cf.put("whiteBrightness", rgbwSpace.WBrightness);
  cf.put("focalPoint", focalPoint);
  cf.put("focalSharpness", focalSharpness);
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
  message += "Focal sharpness: <input type='text' name='focalSharpness' value='" + String(focalSharpness) +"' > (0.0 is no focal point, 1.0 is as sharp as possible)<br>";
  message += "<input type='submit'></form>";
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
  updateDimmer(); // xxxjack quick=true
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