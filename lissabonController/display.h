#ifndef DISPLAY_H
#define DISPLAY_H

class Display {
public:
  enum DisplayMode { dm_sleep=0, dm_select, dm_level, dm_temp, dm_MAX=dm_temp };
  
  Display();
  void flash();
  void show();
  void clearStrips();
  void addStrip(int index, String name, bool available);
  void selectStrip(int index);
  void selectMode(DisplayMode mode);
  
  void clearLevel();
  void setLevel(float level, bool on);
  void clearTemp();
  void setTemp(float temperature);
  void showActivity(bool activity);
private:
  int selectedStripOnDisplay = -1;
  DisplayMode selectedModeOnDisplay = dm_sleep;
};

#endif // DISPLAY_H