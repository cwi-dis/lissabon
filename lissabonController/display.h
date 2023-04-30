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
  void setLevel(float level, bool on, float wantedLevel = -1);
  void clearTemp();
  void setTemp(float temperature);
  void showScanning(bool _isScanning);
  void showConnected(bool _isConnected);
  void showActivity(const char *activity);
private:
  void _updateActivity(const char *activity);
  int selectedStripOnDisplay = -1;
  DisplayMode selectedModeOnDisplay = dm_sleep;
  bool isScanning = false;
  bool isConnected = false;
  bool isActivity = false;
};

#endif // DISPLAY_H