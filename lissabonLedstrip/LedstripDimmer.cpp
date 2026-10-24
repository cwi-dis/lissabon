#include "LedstripDimmer.h"
#include <cmath>

//const float PI = atan(1)*4;
const float SQRT_HALF = sqrt(0.5);

// The ledstrip's own debug output: every pixel, every frame. Compile-time,
// like iotsa's subsystems (iotsaLog.h): -DLISSABON_DEBUG_LEDSTRIP.
#ifdef LISSABON_DEBUG_LEDSTRIP
#define LOG_LEDSTRIP(fmt, ...) IOTSA_LOG("LedstripDimmer", fmt, ##__VA_ARGS__)
#else
#define LOG_LEDSTRIP(fmt, ...) do { if (0) IOTSA_LOG("LedstripDimmer", fmt, ##__VA_ARGS__); } while (0)
#endif

// Calculating the curve every loop is more cpu-intensitive but animations look much better.
#define LEDSTRIP_CALCULATE_LEVELS_EVERY_LOOP

namespace Lissabon {

LedstripDimmer::LedstripDimmer(int _num, IotsaPixelstripMod& _mod, DimmerCallbacks *_callbacks)
: AbstractDimmer(_num, _callbacks),
  mod(_mod)
{

}

void LedstripDimmer::setup() {
  mod.setPixelsource(this);
}

void LedstripDimmer::updateDimmer() {
  LOG_LEDSTRIP("updateDimmer()");
  clampLevel();
#ifndef LEDSTRIP_CALCULATE_LEVELS_EVERY_LOOP
  // xxxjack calling calcPixelLevels here (in stead of in loop) means the curve
  // remains the same during fades. Calling it in loop() would fix that.
  calcPixelLevels(level);
#endif
  // Compute animation duration, which signals to loop() that things neeed to change.
  AbstractDimmer::updateDimmer();
}

void LedstripDimmer::clampLevel() {
#if 0
  float maxOutputLevel = maxLevelCorrectColor();
  if (level > maxOutputLevel) {
    IOTSA_LOG("LedstripDimmer", "calcLevel: level clamped to %f", maxOutputLevel);
    level = maxOutputLevel;
  }
#endif
}

void LedstripDimmer::calcPixelLevels(float wantedLevel) {
 
  //
  // Determine how much light we can produce with the preferred curve
  //
  float cumulativeValue = levelFuncCumulative(0, count, 1);
  LOG_LEDSTRIP("calcPixelLevels: wantedLevel=%f focalSpread=%f focalPoint=%f cumulativeValue=%f", wantedLevel, focalSpread, focalPoint, cumulativeValue);
  //
  // We may need to increase spread, because the current curve may result in brightness > 1 for some
  // pixels.
  //
  float spreadCorrection = 1;
  float correction = count / cumulativeValue;
  if (pixelLevels != NULL) {
    for(int i=0; i<count; i++) {
      float thisValue = levelFuncCumulative(i, i+1, 1) * correction;
      LOG_LEDSTRIP("calcPixelLevels: uncorrected pixelLevel[%d] = %f", i, thisValue);
      if (thisValue*wantedLevel > spreadCorrection) spreadCorrection = thisValue*wantedLevel;
    }
  }
  cumulativeValue = levelFuncCumulative(0, count, spreadCorrection);
  LOG_LEDSTRIP("calcPixelLevels: spreadCorrection=%f cumulativeValue=%f", spreadCorrection, cumulativeValue);
  //
  // cumulativeValue is the amount of light produced. We need to
  // correct this so level is what is actually produced.
  //
  // This will still produce some curve values slightly bigger than 1.0 but we simply  clamp
  // those in loop().
  //
  correction = count / cumulativeValue;
  float sumLevel = 0;
  if (pixelLevels != NULL) {
    for(int i=0; i<count; i++) {
      float thisValue = levelFuncCumulative(i, i+1, spreadCorrection) * correction;
      pixelLevels[i] = thisValue;
      LOG_LEDSTRIP("calcPixelLevels: pixelLevel[%d] = %f", i, thisValue);
      sumLevel += thisValue;
    }
  }
  LOG_LEDSTRIP("calcPixelLevels: sum(pixelLevel)=%f avg=%f", sumLevel, sumLevel/count);
  LOG_LEDSTRIP("calcPixelLevels: pixelLevels updated");
}

void LedstripDimmer::updateColorspace(float whiteTemperature, float whiteBrightness) {
  // Gamma value is ignored: the Colorspace converter uses a fixed gamma value (2.2, I think)
  bool doGamma = gamma > 1.1;
  rgbwSpace = Colorspace(whiteTemperature, whiteBrightness, true, doGamma);
}

float LedstripDimmer::maxLevelCorrectColor() {
    // Check the maximum brightness we can correctly render and remember that
  TempFColor maxTFColor(temperature, 1.0);
  RgbwFColor correctRgbwColor = rgbwSpace.toRgbw(maxTFColor);
  float maxLevel = correctRgbwColor.CalculateTrueBrightness(rgbwSpace.WBrightness);
  LOG_LEDSTRIP("maxLevelCorrectColor: wTemp=%f wBright=%f wtdTemp=%f R=%f G=%f B=%f W=%f maxCorrect=%f", rgbwSpace.WTemperature, rgbwSpace.WTemperature, temperature, correctRgbwColor.R, correctRgbwColor.G, correctRgbwColor.B, correctRgbwColor.W, maxLevel);
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

float LedstripDimmer::levelFuncCumulative(int left, int right, float spreadFactor) {
  // Cumulative light function. left and right are 0..1 values.
  // We assume a range of -8..8 is "close enough" that erf(8)-erf(-8) is 2.
  float leftFraction = (float)left / count;
  float rightFraction = (float) right / count;
  float range  = (1 - focalSpread)/spreadFactor;
  if (range <= 0) range = 0.001;
  float leftEdge = (0-focalPoint*16) * range;
  float rightEdge =(16-focalPoint*16) * range;
  float leftScaled = leftEdge + leftFraction * (rightEdge-leftEdge);
  float rightScaled = leftEdge + rightFraction * (rightEdge-leftEdge);
  float erfLeft = erf(leftScaled);
  float erfRight = erf(rightScaled);
  return (erfRight - erfLeft) / 2;
}


bool LedstripDimmer::available() {
  return true;
}

template<class Ctx> bool LedstripDimmer::_stripArgs(Ctx& ctx) {
  // Tuning, as the dimmer's own settings.
  IotsaContext::RightsScope rights(ctx);
  ctx.argsNeedRight("lighttuning");
  float whiteTemperature = rgbwSpace.WTemperature;
  float whiteBrightness = rgbwSpace.WBrightness;
  bool changed = false;
  changed |= ctx.arg("whiteTemperature", whiteTemperature, IotsaContext::Saved);
  changed |= ctx.arg("whiteBrightness", whiteBrightness, IotsaContext::Saved);
  changed |= ctx.arg("focalPoint", focalPoint, IotsaContext::Saved);
  changed |= ctx.arg("focalSpread", focalSpread, IotsaContext::Saved);
  if (changed) updateColorspace(whiteTemperature, whiteBrightness);
  return changed;
}

void LedstripDimmer::getHandler(IotsaApiContext& ctx) {
  ctx.show("whiteTemperature", rgbwSpace.WTemperature);
  ctx.show("whiteBrightness", rgbwSpace.WBrightness);
  ctx.show("focalPoint", focalPoint);
  ctx.show("focalSpread", focalSpread);
  ctx.show("inCalibrationMode", inCalibrationMode);
  if (inCalibrationMode) {
    JsonArray _calibrationData = ctx.reply()["calibrationData"].to<JsonArray>();
    for(int i=0; i <8; i++) {
      _calibrationData.add<float>(calibrationData[i]);
    }
  } else {
    ctx.show("ccMaxLevel", maxLevelCorrectColor());
    ctx.show("ccTemperature", temperature);
    TempFColor maxTFColor(temperature, level);
    RgbwFColor correctRgbwColor = rgbwSpace.toRgbw(maxTFColor);
    ctx.show("ccLevel_r", correctRgbwColor.R);
    ctx.show("ccLevel_g", correctRgbwColor.G);
    ctx.show("ccLevel_b", correctRgbwColor.B);
    ctx.show("ccLevel_w", correctRgbwColor.W);
  }
  AbstractDimmer::getHandler(ctx);
}

// Calibration (lissabonCalibrate) is REST only: calibrationData, 4 or 8
// values (RGBW, or two RGBW sets for alternating pixels), sets the pixels
// directly; empty, or a PUT without it, goes back to normal.
void LedstripDimmer::putHandler(IotsaApiContext& ctx) {
  AbstractDimmer::putHandler(ctx);
  if (_stripArgs(ctx)) updateDimmer();
  ctx.argsNeedRight("lighttuning");
  if (ctx.argAction("calibrationData", IotsaContext::Transient)) {
    JsonArray _calibrationData = ctx.request()["calibrationData"].as<JsonArray>();
    int size = _calibrationData.size();
    if (_calibrationData && (size == 4 || size == 8)) {
      for (int i =0; i<8; i++) {
        calibrationData[i] = _calibrationData[i % size];
      }
      animationStartMillis = animationEndMillis = millis();
      inCalibrationMode = true;
      IOTSA_LOG_DEBUG("LedstripDimmer", "Got calibrationData, in calibration mode");
    } else if (_calibrationData && size == 0) {
      animationStartMillis = animationEndMillis = millis();
      inCalibrationMode = false;
      IOTSA_LOG_DEBUG("LedstripDimmer", "Got empty calibrationData, left calibrationMode");
    } else {
      ctx.error(400, "calibrationData: 4 or 8 values, or none");
    }
  } else if (inCalibrationMode && !ctx.failed()) {
    IOTSA_LOG_DEBUG("LedstripDimmer", "No calibrationData, left calibration mode");
    animationStartMillis = animationEndMillis = millis();
    inCalibrationMode = false;
  }
}

#ifdef IOTSA_WITH_WEB
void LedstripDimmer::formHandler_args(IotsaWebContext& ctx) {
  AbstractDimmer::formHandler_args(ctx);
  if (_stripArgs(ctx)) updateDimmer();
}

void LedstripDimmer::formHandler_fields(IotsaWebContext& ctx) {
  AbstractDimmer::formHandler_fields(ctx);
  if (!ctx.includesSettings()) return;
  {
    IotsaContext::RightsScope rights(ctx);
    ctx.argsNeedRight("lighttuning");
    IotsaWebContext::Form form(ctx, "Save", "LED strip");
    ctx.showField("White LED temperature", "whiteTemperature", rgbwSpace.WTemperature, IotsaContext::Saved, "kelvin");
    ctx.showField("White LED brightness", "whiteBrightness", rgbwSpace.WBrightness, IotsaContext::Saved);
    ctx.showField("Focal point", "focalPoint", focalPoint, IotsaContext::Saved, "0.0 is the first LED, 1.0 the last");
    ctx.showField("Focal spread", "focalSpread", focalSpread, IotsaContext::Saved, "0.0 is narrow, 1.0 the full width");
  }
  ctx << "<p>Color calibration can only be done through the REST interface, with calibrationData.</p>";
  ctx << colorDump();
}
#endif // IOTSA_WITH_WEB

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

void LedstripDimmer::applyStatusPixel(uint32_t statusColor) {
  if (!statusOverlayActive) {
    // First frame of the overlay: stash pixel 0's real value so it can be
    // put back once the status signal clears and we go idle (loop() only
    // recomputes pixel 0 from scratch while an animation is running).
    memcpy(savedPixel0, pixelBuffer, bpp);
    statusOverlayActive = true;
  }
  pixelBuffer[0] = (statusColor >> 16) & 0xff;
  pixelBuffer[1] = (statusColor >> 8) & 0xff;
  pixelBuffer[2] = statusColor & 0xff;
  if (bpp == 4) pixelBuffer[3] = 0;
}

void LedstripDimmer::restoreStatusPixel() {
  memcpy(pixelBuffer, savedPixel0, bpp);
  statusOverlayActive = false;
}

void LedstripDimmer::loop() {
  unsigned long loopStart = micros();
  // If we are not completely setup we return.
  if (pixelBuffer == NULL || count == 0 || stripHandler == NULL) return;

  // Status-indicator overlay on pixel 0 (cwi-dis/lissabon#28). Checked
  // against the semantic signal accessors, not statusColor()'s instantaneous
  // render, so the strip's power stays latched on through the dark phases of
  // a blink/breathe/gap cycle -- see IotsaPixelstripMod::keepPowered().
  // Suppressed during calibration: a rogue lit pixel would corrupt the
  // colour-sensor reading lissabonCalibrate is trying to take.
  bool statusActive = !inCalibrationMode &&
    (iotsaStatus.overrideSignal().colour != 0 ||
     iotsaStatus.modeSignal().colour != 0 ||
     iotsaStatus.wifiSignal().colour != 0);
  mod.keepPowered(statusActive);

  bool haveAnimation = (animationStartMillis != 0 && animationEndMillis != 0);
  if (!haveAnimation) {
    // Nothing for the real light to do -- but pixel 0 may still need to show
    // (or stop showing) a status signal.
    if (statusActive) {
      applyStatusPixel(iotsaStatus.statusColor());
      stripHandler->pixelSourceCallback();
    } else if (statusOverlayActive) {
      restoreStatusPixel();
      stripHandler->pixelSourceCallback();
    }
    return;
  }
  //
  // If we are in calibration mode we simply set the pixels and be done
  //
  if (inCalibrationMode) {
    statusOverlayActive = false;  // pixel 0 is about to legitimately hold calibration data
    IOTSA_LOG_DEBUG("LedstripDimmer", "loop: calibration r1 %f g2 %f b1 %f w1 %f, r1 %f g2 %f b1 %f w1 %f",
      calibrationData[0], calibrationData[1], calibrationData[2], calibrationData[3],
      calibrationData[4], calibrationData[5], calibrationData[6], calibrationData[7]
    );
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
    animationStartMillis = animationEndMillis = 0;
    return;
  }
  //
  // Compute current level (taking into account isOn and animation progress)
  //
  calcCurLevel();
#ifdef LEDSTRIP_CALCULATE_LEVELS_EVERY_LOOP
  calcPixelLevels(curLevel);
#endif
  
  uint8_t *p = pixelBuffer;
  
  for (int i=0; i<count; i++) {
    float thisLevel = curLevel*pixelLevels[i];
    if (thisLevel > 1) thisLevel = 1;
    TempFColor thisTFColor(temperature, thisLevel);
    RgbwFColor thisPixelFColor = rgbwSpace.toRgbw(thisTFColor);
    RgbwColor thisPixelColor = thisPixelFColor;
    LOG_LEDSTRIP("loop: pixel %d: level=%f r=%d g=%d b=%d w=%d", i, thisLevel, thisPixelColor.R, thisPixelColor.G, thisPixelColor.B, thisPixelColor.W);
    *p++ = thisPixelColor.R;
    *p++ = thisPixelColor.G;
    *p++ = thisPixelColor.B;
    if (bpp == 4) *p++ = thisPixelColor.W;
  }
  statusOverlayActive = false;  // pixel 0 just got its freshly-computed real value
  if (statusActive) {
    applyStatusPixel(iotsaStatus.statusColor());
  }
  stripHandler->pixelSourceCallback();
  LOG_LEDSTRIP("loop: %lu us", micros()-loopStart);
}

}