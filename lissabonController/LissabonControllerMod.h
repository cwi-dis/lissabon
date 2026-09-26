#ifndef _LISSABONCONTROLLERMOD_H_
#define _LISSABONCONTROLLERMOD_H_
#include "iotsa.h"
#include "iotsaBLEClient.h"
#include "DimmerDynamicCollection.h"
#include "DimmerBLEClient.h"
#include "display.h"
#include "buttons.h"
#include "iotsaBattery.h"

extern Display *display;
extern IotsaBatteryMod batteryMod;

//
// The lissabonController appliance's own module: a dynamic collection of
// Lissabon::DimmerBLEClient devices, selected/controlled through the OLED
// display and rotary encoder (Buttons), plus the REST/web UI for managing
// which BLE dimmers are in the collection. Was IotsaLedstripControllerMod --
// renamed to match every sibling appliance's own module naming
// (LissabonDimmerMod, LissabonLedstripMod, LissabonRemoteMod,
// LissabonSimpleRemoteMod) instead of a name inherited from the archived
// iotsaLedstripController repo this descended from. Controls arbitrary BLE
// dimmers (including plain PWM dimmers, e.g. keuken/tafel/spot/bank), not
// specifically ledstrips, despite the old name.
//
// Deliberately still lives here, not in libLissabon -- it's specific to this
// appliance's own UI (OLED + rotary encoder). Split into its own .h/.cpp
// pair (out of what used to be mainLedstripController.cpp) so a future move,
// if the collection-management part of this ever gets generalized, is a
// smaller step.
//
class LissabonControllerMod : public IotsaBLEClientMod, public Lissabon::DimmerCallbacks, public ButtonsCallbacks {
public:
  LissabonControllerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaBLEClientMod(_app, _auth, early),
    buttons(this)
  {}
  void setup();
  void lateSetup() override;
  String info();
  void configLoad();
  void configSave();
  void loop();
  void selectDimmer(bool next, bool prev) override;
  float getTemperature() override;
  void setTemperature(float temperature) override;
  float getLevel() override;
  void setLevel(float level) override;
  void toggle() override;
  void sleepWakeupNotification(bool sleep) override;

protected:
  void _setupDisplay();
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void unknownDimmerBLEClientFound(const NimBLEAdvertisedDevice& device);
  void knownDimmerBLEClientChanged(const NimBLEAdvertisedDevice& device);
  virtual String formHandler_field_perdevice(const char *deviceName) override;
  virtual void scanningChanged() override;
  virtual void showMessage(const char *message) override;
private:
  void dimmerOnOffChanged();
  void dimmerValueChanged();
  void dimmerAvailableChanged();
  void webHandler() override;
  void clearAllDimmersAndReboot();
  Buttons buttons;
  Lissabon::DimmerDynamicCollection::ItemType* getDimmerForCommand(int num);
  void updateDisplay(bool clear);
  void setDimmerFollowed(int index, bool follow);
  typedef std::pair<std::string, NimBLEAddress> unknownDimmerInfo;
  Lissabon::DimmerDynamicCollection dimmers;
  Lissabon::DimmerDynamicCollection::ItemType* dimmerFactory(int num);
  int selectedDimmerIndex = 0; // currently selected dimmer on display
  int savedSelectedDimmerIndex = 0;
  bool selectedDimmerIsAvailable = false;
  int stayConnectedMillis = 3000; // deliberately not configurable yet, see cwi-dis/iotsa#144
  bool saveNeeded = false;
};
#endif // _LISSABONCONTROLLERMOD_H_
