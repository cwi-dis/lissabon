#include "buttons.h"
#include "iotsaInput.h"

#define LOG_UI if(1)


//
// Device can be rebooted or configuration mode can be requested by quickly tapping any button.
// TAP_DURATION sets the maximum time between press-release-press-etc.
// Note that the controller records both press and release, so we need to double the count.
#define TAP_COUNT_MODE_CHANGE 6
#define TAP_COUNT_REBOOT 12
#define TAP_DURATION 1000

// When using an Alps EC12D rotary encoder with pushbutton here is the pinout:
// When viewed from the top there are pins at northwest, north, northeast, southwest, southeast.
// These pins are named (in Alps terminology) A, E, B, C, D.
// A and B (northwest, northeast) are the rotary encoder pins,
// C is the corresponding ground,
// D and E are the pushbutton pins.
// So, connect E and C to GND, D to GPIO0, A to GPI14, B to GPIO2
RotaryEncoder encoder(14, 2);
#define ENCODER_STEPS 20
Button button(0, true, true, true);
#define SHORT_PRESS_DURATION 500
Button rockerUp(12, true, false, true);
Button rockerDown(13, true, false, true);

Input* inputs[] = {
  &button,
  &encoder,
  &rockerUp,
  &rockerDown,
};

Input** getInputs() { return inputs; }
int getInputCount() { return sizeof(inputs) / sizeof(inputs[0]); }

void Buttons::setup() {
  button.setCallback(std::bind(&Buttons::uiButtonPressed, this));
  encoder.setCallback(std::bind(&Buttons::uiEncoderChanged, this));

  rockerUp.setCallback(std::bind(&Buttons::uiRockerPressed, this));
  rockerDown.setCallback(std::bind(&Buttons::uiRockerPressed, this));
  rockerUp.setRepeat(500,200);
  rockerDown.setRepeat(500,200);
}

void Buttons::_tap() {
  // Called whenever any button changed state.
  // Used to give visual feedback (led turning off) on presses and releases,
  // and to enable config mod after 4 taps and reboot after 8 taps
  uint32_t now = millis();
  if (now >= lastButtonChangeMillis + TAP_DURATION) {
    // Either the first change, or too late. Reset.
    if (lastButtonChangeMillis > 0) {
      IotsaSerial.println("TapCount: reset");
    }
    lastButtonChangeMillis = millis();
    buttonChangeCount = 0;
  }
  if (lastButtonChangeMillis > 0 && now < lastButtonChangeMillis + TAP_DURATION) {
    // A button change that was quick enough for a tap
    lastButtonChangeMillis = now;
    buttonChangeCount++;
    IFDEBUG IotsaSerial.printf("TapCount: %d\n", buttonChangeCount);
    if (buttonChangeCount == TAP_COUNT_MODE_CHANGE) {
      IotsaSerial.println("TapCount: mode change");
      controller->showMessage("mode");
      iotsaConfig.allowRequestedConfigurationMode();
    }
    if (buttonChangeCount == TAP_COUNT_REBOOT) {
      IotsaSerial.println("TapCount: reboot");
      controller->showMessage("reboot");
      iotsaConfig.requestReboot(1000);
    }
  }
}

bool
Buttons::uiRockerPressed() {
  // The rocker switch controls the selected dimmer
  bool next = rockerUp.pressed && rockerUp.repeatCount == 0;
  bool prev = rockerDown.pressed && rockerDown.repeatCount == 0;
  LOG_UI IotsaSerial.printf("LissabonController.Buttons.uiRockerPressed: up: state=%d repeatCount=%d duration=%d next=%d\n", rockerUp.pressed, rockerUp.repeatCount, rockerUp.duration, next);
  LOG_UI IotsaSerial.printf("LissabonController.Buttons.uiRockerPressed: down: state=%d repeatCount=%d duration=%d prev=%d\n", rockerDown.pressed, rockerDown.repeatCount, rockerDown.duration, prev);
  if (next || prev) {
    controller->selectDimmer(next, prev);
  }
  return true;
}

bool
Buttons::uiButtonPressed() {
  LOG_UI IotsaSerial.printf("LissabonController.Buttons.uiButtonPressed: state=%d repeatCount=%d duration=%d\n", button.pressed, button.repeatCount, button.duration);
  _tap();
  refreshEncoder();

  if (!button.pressed && button.repeatCount == 0 && button.duration < SHORT_PRESS_DURATION) {
    // Short press: turn current dimmer on or off
    LOG_UI IotsaSerial.println("LissabonCOntroller.Buttons.uiButtonPressed: dimmer on/off");
    controller->toggle();
  } else {
    // Long press: assume rotary will be used for selecting temperature
  }
  return true;
}

bool
Buttons::uiEncoderChanged() {
  LOG_UI IotsaSerial.printf("LissabonController.Buttons.uiEncoderChanged(%d)\n", encoder.value);
#ifdef DIMMER_WITH_TEMPERATURE
  if (button.pressed) {
    if (encoder.value < 0) encoder.value = 0;
    if (encoder.value >= ENCODER_STEPS) encoder.value = ENCODER_STEPS;
    float f_value = (float)encoder.value / ENCODER_STEPS;
    if (f_value < 0) {
        f_value = 0;
        encoder.value = 0;
    }
    if (f_value > 1.0) {
      f_value = 1.0;
      encoder.value = ENCODER_STEPS;
    }
    LOG_UI IotsaSerial.printf("LissabonController.Buttons: now temperature=%f (int %d)\n", f_value, encoder.value);
    controller->setTemperature(f_value);
  } else 
#endif
  {
    // Button not pressed, encoder controls level.
    if (encoder.value < 0) encoder.value = 0;
    if (encoder.value >= ENCODER_STEPS) encoder.value = ENCODER_STEPS;
    float f_value = (float)encoder.value / ENCODER_STEPS;
    if (f_value <= 0) {
        f_value = 0;
        encoder.value = 0;
    }
    if (f_value >= 1.0) {
      f_value = 1.0;
      encoder.value = ENCODER_STEPS;
    }
    LOG_UI IotsaSerial.printf("LissabonController.Buttons: now level=%f (int %d)\n", f_value, encoder.value);
    controller->setLevel(f_value);
  }
  return true;
}

void
Buttons::refreshEncoder() {
  if (button.pressed) {
    // If we do temperature: pressing the button means the encoder will control temperature
#ifdef DIMMER_WITH_TEMPERATURE
    float t_value = controller->getTemperature();
    if (t_value < 0) t_value = 0;
    if (t_value > 1) t_value = 1;
    encoder.value = (int)(t_value * ENCODER_STEPS);
#endif // DIMMER_WITH_TEMPERATURE
  } else
  {
    // if the button was released the encoder controls level
    float f_value = controller->getLevel();
    encoder.value = (int)(f_value * ENCODER_STEPS);
  }
}