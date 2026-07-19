#ifndef _BLEDIMMER_H_
#define _BLEDIMMER_H_
//
// LED Lighting control module. 
//
#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEClient.h"
#include "AbstractDimmer.h"
#include "LissabonBLE.h"

#define IOTSA_WITH_BLE_TASKS

namespace Lissabon {

class BLEDimmer : public AbstractDimmer {
public:
  BLEDimmer(int _num, IotsaBLEClientMod &_bleClientMod, DimmerCallbacks *_callbacks, int _stayConnectedMillis=0);
  ~BLEDimmer();
  void followDimmerChanges(bool follow);
  void updateDimmer();
  bool available() override;
  bool isConnected();
  bool isConnecting() { return _isConnecting || needSyncFromDevice || needSyncToDevice; }
  void refresh();
  bool dataValid() override { return _dataValid; }
  bool setName(String value);
  void setup() override;
  void loop() override;
  void identify() override;
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) override;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) override;
  virtual void getHandler(JsonObject& reply) override;
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
protected:
#ifdef IOTSA_WITH_BLE_TASKS
  static void _connectionTask(void *arg);
  void connectionTask();
  TaskHandle_t connectionTaskHandle;
  bool _availableChanged;
  bool _dataValidChanged;
#endif
  IotsaBLEClientConnection *dimmer = nullptr;
  bool _ensureConnection();
  void _syncToDevice();
  bool _syncFromDevice();
  IotsaBLEClientMod& bleClientMod;
  bool listenForDeviceChanges = false;
  bool needSyncToDevice = false;
  bool needSyncFromDevice = false;
  bool _dataValid = false;
  bool needIdentify = false;
  bool _isConnecting = false;
  bool _isDisconnecting = false;
  uint32_t needTransmitTimeoutAtMillis = 0;
  // How long to keep a pending updateDimmer()/followDimmerChanges() sync
  // request alive while the device's address is still unknown (i.e. it
  // hasn't yet been found by a discovery scan) before giving up on it.
  // In practice this should be comfortably above any live device's real
  // sleep/wake or advertise cadence, so it firing means the device is
  // genuinely gone (powered down, out of range), not just slow to find --
  // though note BLE itself puts no ceiling on a peer's sleep cycle, so
  // that's an assumption about today's fleet, not a protocol guarantee.
  // Deliberately not configurable yet -- see cwi-dis/iotsa#144, which
  // proposes moving this (and the rest of connectionTask()'s generic
  // connection-lifecycle orchestration) into iotsa core, where it would
  // apply to any IotsaBLEClientConnection consumer, not just dimmers.
  const uint32_t unreachableGiveUpMillis = 10000;
  uint32_t disconnectAtMillis = 0;
  uint32_t noWarningPrintBefore = 0;
public:
  // How long to stay connected after a command, in case another one
  // follows immediately (e.g. dragging a brightness slider) -- avoids
  // paying the full discovery+connect cost again for a quick follow-up.
  // Also deliberately not configurable yet, same reasoning as
  // unreachableGiveUpMillis above (see cwi-dis/iotsa#144).
  uint32_t stayConnectedMillis = 0;
};
};
#endif // _BLEDIMMER_H_