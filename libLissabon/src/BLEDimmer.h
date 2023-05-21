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
  BLEDimmer(int _num, IotsaBLEClientMod &_bleClientMod, DimmerCallbacks *_callbacks, int _keepOpenMillis=0);
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
  uint32_t disconnectAtMillis = 0;
  uint32_t noWarningPrintBefore = 0;
public:
  uint32_t keepOpenMillis = 0;
};
};
#endif // _BLEDIMMER_H_