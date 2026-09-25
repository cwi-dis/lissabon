#ifndef DISPLAY_H
#define DISPLAY_H

// "available" used to conflate two distinct facts: address-known-but-never-synced,
// and successfully-synced-but-currently-idle-disconnected (the connect/sync/release
// pattern from cwi-dis/iotsa#106 means a dimmer spends most of its life in the
// latter, not actually link-connected). Split into "seen" and "synced" so the
// display can tell them apart. "synced" is the common/healthy steady state, so it's
// drawn blank (see Display::addStrip()) -- every other state gets a distinct icon.
enum StripStatus {
  unavailable,  // address not known at all -- '?' icon
  seen,         // address known, never successfully synced -- '...' icon
  connecting,   // connect attempt in progress right now -- arrow icon
  connected,    // link open right now -- doublearrow icon
  synced        // have valid cached data, idle -- blank (the normal/good case)
};

class Display {
public:
  
  Display();
  void dim(bool _dim);
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