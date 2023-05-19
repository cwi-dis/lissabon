#ifndef DISPLAY_H
#define DISPLAY_H

enum StripStatus {
  unavailable,
  available,
  connecting,
  connected
};

class Display {
public:
  
  Display();
  void flash();
  void show();
  void clearStrips();
  void addStrip(int index, String name, StripStatus status);
  void selectStrip(int index);
  
  void clearLevel();
  void setLevel(float level, bool on, float wantedLevel = -1);
  void clearTemp();
  void setTemp(float temperature);
  void showScanning(bool _isScanning);
  void showActivity(const char *activity);
private:
  void _updateActivity(const char *activity);
  int selectedStripOnDisplay = -1;
  bool isScanning = false;
 };

#endif // DISPLAY_H