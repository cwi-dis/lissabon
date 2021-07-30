#include "iotsa.h"
#include "display.h"

#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#define PIN_SDA 5
#define PIN_SCL 4
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
static Adafruit_SSD1306 *oled;

#define STRIPS_X 2
#define STRIPS_Y 2
#define STRIPS_HEIGHT 12
#define STRIPS_WIDTH 60
#define N_STRIPS 7

#define SEPARATOR_Y (STRIPS_Y+STRIPS_HEIGHT*N_STRIPS+2)
#define SEPARATOR_WIDTH 64
#define INTENSITY_X 13
#define INTENSITY_Y (SEPARATOR_Y+3)
#define INTENSITY_WIDTH 38
#define INTENSITY_HEIGHT 12
#define COLOR_X 13
#define COLOR_Y (INTENSITY_Y + INTENSITY_HEIGHT+2)
#define COLOR_WIDTH 38
#define COLOR_HEIGHT 12

#include "icons/on.h"
#include "icons/off.h"
#include "icons/sun.h"
#include "icons/lightbulb.h"

Display::Display()
: selected(-1)
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
  oled->setCursor(0, 0);
  oled->println("ILC ready");

  oled->drawFastHLine(0, SEPARATOR_Y, SEPARATOR_WIDTH, WHITE);

  oled->drawXBitmap(0, INTENSITY_Y, off_bits, off_width, off_height, WHITE);
  oled->drawRect(INTENSITY_X, INTENSITY_Y, INTENSITY_WIDTH, INTENSITY_HEIGHT, WHITE);
  oled->drawXBitmap(INTENSITY_X+INTENSITY_WIDTH+1, INTENSITY_Y, on_bits, on_width, on_height, WHITE);

  oled->drawXBitmap(0, COLOR_Y, lightbulb_bits, lightbulb_width, lightbulb_height, WHITE);
  oled->drawRect(COLOR_X, COLOR_Y, COLOR_WIDTH, COLOR_HEIGHT, WHITE);
  oled->drawXBitmap(COLOR_X+COLOR_WIDTH+1, COLOR_Y, sun_bits, sun_width, sun_height, WHITE);
  oled->display();
}

void Display::clearStrips() {
  // xxxjack clear strip area
  // xxxjack show all in 0
  oled->fillRect(STRIPS_X-2, STRIPS_Y-2, STRIPS_WIDTH+4, STRIPS_HEIGHT*N_STRIPS+4, BLACK);
  //addStrip(0, "ALL", true);
  selected = -1;
}

void Display::addStrip(int index, String name, bool available) {
  // xxxjack show name in index
  int x = STRIPS_X;
  int y = STRIPS_Y + index*STRIPS_HEIGHT;
  oled->setCursor(x, y);
  oled->print(name.c_str());
  if (!available) {
      oled->writeFastHLine(x, y+STRIPS_HEIGHT/2-2, STRIPS_WIDTH, WHITE);
  }
}

void Display::selectStrip(int index) {
  // xxxjack clear ring around selected
  if (selected >= 0) {
    int x = STRIPS_X-2;
    int y = STRIPS_Y + selected*STRIPS_HEIGHT - 2;
    oled->drawRoundRect(x, y, STRIPS_WIDTH, STRIPS_HEIGHT, 4, BLACK);
  }

  selected = index;
  if (selected >= 0) {
    // xxxjack draw ring around selected
    int x = STRIPS_X-2;
    int y = STRIPS_Y + selected*STRIPS_HEIGHT - 2;
    oled->drawRoundRect(x, y, STRIPS_WIDTH, STRIPS_HEIGHT, 4, WHITE);
  }
}

void Display::setIntensity(float intensity, bool on) {
  oled->fillRect(INTENSITY_X, INTENSITY_Y, INTENSITY_WIDTH, INTENSITY_HEIGHT, BLACK);
  oled->drawRect(INTENSITY_X, INTENSITY_Y, INTENSITY_WIDTH, INTENSITY_HEIGHT, WHITE);
  if (on) {
    oled->fillRect(INTENSITY_X, INTENSITY_Y, int(intensity*INTENSITY_WIDTH), INTENSITY_HEIGHT, WHITE);
  } else {
    oled->drawFastVLine(INTENSITY_X+int(intensity*INTENSITY_WIDTH), INTENSITY_Y, INTENSITY_HEIGHT, WHITE);
  }
}

void Display::clearColor() {
  // xxxjack clear color area
  oled->fillRect(COLOR_X, COLOR_Y, COLOR_WIDTH, COLOR_HEIGHT, BLACK);
  oled->drawRect(COLOR_X, COLOR_Y, COLOR_WIDTH, COLOR_HEIGHT, WHITE);
}

void Display::addColor(float color) {
  oled->drawFastVLine(COLOR_X+int(color*COLOR_WIDTH), COLOR_Y, COLOR_HEIGHT, WHITE);
}

void Display::show() {
  oled->display();
}
