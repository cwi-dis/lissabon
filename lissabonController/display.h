#ifndef DISPLAY_H
#define DISPLAY_H

class Display {
public:
  Display();
  void show();
  void clearStrips();
  void addStrip(int index, std::string name, bool available);
  void selectStrip(int index);

  void setIntensity(float intensity);
  void clearColor();
  void addColor(float color);
private:
  int selected;
};

#endif // DISPLAY_H