#include "iotsa.h"
#include "display.h"

#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#define PIN_SDA 5
#define PIN_SCL 4
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 128
static Adafruit_SSD1306 *oled;

#define STRIPS_X 5
#define STRIPS_Y 2
#define STRIPS_HEIGHT 12
#define STRIPS_WIDTH 55
#define N_STRIPS 7

#define SEPARATOR_Y (STRIPS_Y+STRIPS_HEIGHT*N_STRIPS+2)
#define SEPARATOR_WIDTH 64
#define LEVEL_X 16
#define LEVEL_Y (SEPARATOR_Y+3)
#define LEVEL_WIDTH 33
#define LEVEL_HEIGHT 12
#define TEMP_X 16
#define TEMP_Y (LEVEL_Y + LEVEL_HEIGHT+2)
#define TEMP_WIDTH 33
#define TEMP_HEIGHT 12

#include "icons/on.h"
#include "icons/off.h"
#include "icons/sun.h"
#include "icons/lightbulb.h"

Display::Display()
{
  Wire.begin(PIN_SDA, PIN_SCL);
  oled = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
  if (!oled->begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) {
    IFDEBUG IotsaSerial.println("OLED init failed");
    return;
  }
  oled->clearDisplay();
  oled->setRotation(1);
  oled->setTextSize(1);
  oled->setTextColor(WHITE);

  oled->drawFastHLine(0, SEPARATOR_Y, SEPARATOR_WIDTH, WHITE);

  oled->drawXBitmap(LEVEL_X-off_width, LEVEL_Y, off_bits, off_width, off_height, WHITE);
  oled->drawRect(LEVEL_X, LEVEL_Y, LEVEL_WIDTH, LEVEL_HEIGHT, WHITE);
  oled->drawXBitmap(LEVEL_X+LEVEL_WIDTH+1, LEVEL_Y, on_bits, on_width, on_height, WHITE);

  oled->drawXBitmap(TEMP_X-off_width, TEMP_Y, lightbulb_bits, lightbulb_width, lightbulb_height, WHITE);
  oled->drawRect(TEMP_X, TEMP_Y, TEMP_WIDTH, TEMP_HEIGHT, WHITE);
  oled->drawXBitmap(TEMP_X+TEMP_WIDTH+1, TEMP_Y, sun_bits, sun_width, sun_height, WHITE);
  oled->display();
}

void Display::flash() {
  oled->fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, INVERSE);
  oled->display();
  delay(200);
  oled->fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, INVERSE);
  oled->display();
}

void Display::clearStrips() {
  // xxxjack clear strip area
  // xxxjack show all in 0
  oled->fillRect(STRIPS_X-2, STRIPS_Y-2, STRIPS_WIDTH+4, STRIPS_HEIGHT*N_STRIPS+4, BLACK);
  //addStrip(0, "ALL", true);
  selectedStripOnDisplay = -1;
  selectedModeOnDisplay = dm_sleep;
}

void Display::addStrip(int index, String name, bool available) {
  // xxxjack show name in index
  int x = STRIPS_X;
  int y = STRIPS_Y + index*STRIPS_HEIGHT;
  oled->setCursor(x, y);
  oled->print(name.c_str());
  if (!available) {
    int16_t x1, y1;
    uint16_t w, h;
    oled->getTextBounds(name, x, y, &x1, &y1, &w, &h);
      oled->writeFastHLine(x1, y1+h/2, w, WHITE);
  }
}

void Display::selectStrip(int index) {
  // xxxjack clear ring around selected
  if (selectedStripOnDisplay >= 0) {
    int x = STRIPS_X-2;
    int y = STRIPS_Y + selectedStripOnDisplay*STRIPS_HEIGHT - 2;
    oled->drawRoundRect(x, y, STRIPS_WIDTH, STRIPS_HEIGHT, 4, BLACK);
  }

  selectedStripOnDisplay = index;
  if (selectedStripOnDisplay >= 0) {
    // xxxjack draw ring around selected
    int x = STRIPS_X-2;
    int y = STRIPS_Y + selectedStripOnDisplay*STRIPS_HEIGHT - 2;
    oled->drawRoundRect(x, y, STRIPS_WIDTH, STRIPS_HEIGHT, 4, WHITE);
  }
}

void Display::selectMode(DisplayMode mode) {
  oled->drawFastVLine(0, 0, DISPLAY_HEIGHT, BLACK);
  selectedModeOnDisplay = mode;
  switch(selectedModeOnDisplay) {
  case dm_select:
    oled->drawFastVLine(0, STRIPS_Y, SEPARATOR_Y, WHITE);
    break;
  case dm_level:
    oled->drawFastVLine(0, LEVEL_Y, LEVEL_HEIGHT, WHITE);
    break;
  case dm_temp:
    oled->drawFastVLine(0, TEMP_Y, TEMP_HEIGHT, WHITE);
    break;
  default:
    break;
  }
}

void Display::setLevel(float level, bool on) {
  oled->fillRect(LEVEL_X, LEVEL_Y, LEVEL_WIDTH, LEVEL_HEIGHT, BLACK);
  oled->drawRect(LEVEL_X, LEVEL_Y, LEVEL_WIDTH, LEVEL_HEIGHT, WHITE);
  if (on) {
    oled->fillRect(LEVEL_X, LEVEL_Y, int(level*LEVEL_WIDTH), LEVEL_HEIGHT, WHITE);
  } else {
    oled->drawFastVLine(LEVEL_X+int(level*LEVEL_WIDTH), LEVEL_Y, LEVEL_HEIGHT, WHITE);
  }
}

void Display::clearTemp() {
  // xxxjack clear color area
  oled->fillRect(TEMP_X, TEMP_Y, TEMP_WIDTH, TEMP_HEIGHT, BLACK);
  oled->drawRect(TEMP_X, TEMP_Y, TEMP_WIDTH, TEMP_HEIGHT, WHITE);
}

void Display::setTemp(float color) {
  oled->drawFastVLine(TEMP_X+int(color*TEMP_WIDTH), TEMP_Y, TEMP_HEIGHT, WHITE);
}

void Display::show() {
  oled->display();
}
