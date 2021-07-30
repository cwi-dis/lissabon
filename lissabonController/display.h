#ifndef DISPLAY_H
#define DISPLAY_H

class Display {
public:
  Display();
  void show();
  void clearStrips();
  void addStrip(int index, String name, bool available);
  void selectStrip(int index);

  void setIntensity(float intensity, bool on);
  void clearColor();
  void addColor(float color);
private:
  int selected;
};

#endif // DISPLAY_H