#ifdef IOTSA_WITH_BLE
#include "DimmerBLEClient.h"
#include "iotsaBLEClient.h"
#include "LissabonBLE.h"

#define DIMMERBLECLIENT_DEBUG if(1)

namespace Lissabon {

// How long we keep open a ble connection (in case we have a quick new command)
// #define IOTSA_BLEDIMMER_KEEPOPEN_MILLIS 1000

DimmerBLEClient::DimmerBLEClient(int _num, IotsaBLEClientMod &_bleClientMod, DimmerCallbacks *_callbacks, int _stayConnectedMillis)
: AbstractDimmer(_num, _callbacks),
  IotsaRunmodeBLEClient(std::string()), // real name arrives later, via setName()
  bleClientMod(_bleClientMod),
  stayConnectedMillis(_stayConnectedMillis)
{
#ifdef IOTSA_WITH_BLE_TASKS
  xTaskCreate(DimmerBLEClient::_connectionTask, name.c_str(), 5000, this, 1, &connectionTaskHandle);
#endif
}

DimmerBLEClient::~DimmerBLEClient() {
#ifdef IOTSA_WITH_BLE_TASKS
  if (connectionTaskHandle != nullptr) {
    vTaskDelete(connectionTaskHandle);
    connectionTaskHandle = nullptr;
  }
#endif
  // We're our own connection now (not a separate object bleClientMod owns),
  // so we must unregister ourselves before we go away -- otherwise
  // bleClientMod's devices/devicesByAddress maps are left holding a dangling
  // pointer to freed memory, reachable the next time a scan/advertisement
  // matches against them.
  if (name) bleClientMod.delDevice(name);
}

// available()/isConnected() exist on both unrelated base classes
// (AbstractDimmer's is pure virtual, IotsaBLEClientDevice's is a plain
// method) -- DimmerBLEClient's own declaration hides the latter, so it must
// be reached by explicit qualification from in here, not by an unqualified
// self-call (which would just call these same overrides again).
bool DimmerBLEClient::available() {
  return IotsaBLEClientDevice::available();
}

bool DimmerBLEClient::isConnected() {
  return IotsaBLEClientDevice::isConnected() && !_isDisconnecting;
}

void DimmerBLEClient::updateDimmer() {
  if (!available()) {
    IotsaSerial.printf("%s.updateDimmer() called but not available\n", name.c_str());
    return;
  }
  DIMMERBLECLIENT_DEBUG IotsaSerial.printf("%s.updateDimmer() called\n", name.c_str());
  needSyncToDevice = true;
  needTransmitTimeoutAtMillis = millis() + unreachableGiveUpMillis;
  if (callbacks) callbacks->dimmerValueChanged();
}

bool DimmerBLEClient::setName(String value) {
  if (value == name) return false;
  if (name) bleClientMod.delDevice(name);
  clearDevice(); // reset our own inherited connection state, not a separate object's
  name = value;
  if (value) {
    setKnownName(std::string(value.c_str())); // keep the inherited BLE identity in sync
    bleClientMod.addDevice(name, this); // register ourselves, not a mod-constructed stand-in
  }
  return true;
}
void DimmerBLEClient::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  AbstractDimmer::formHandler_fields(message, text, f_name, includeConfig);
  if (!_dataValid) {
    message += "<i>(data may be outdated or invalid)</i><br>";
  }
  if (includeConfig) {
    message += "BLE device name: <input name='" + f_name +".name' value='" + name + "'><br>";
    if (available()) {
      message += "BLE device address: " + String(getAddress().c_str()) + "<br>";
    } else {
      message += "<em>BLE device not available</em><br>";
    }
  }
}

bool DimmerBLEClient::configLoad(IotsaConfigFileLoad& cf, const String& n_name) {
  bool rv = AbstractDimmer::configLoad(cf, n_name);
  String address;
  cf.get(n_name + ".address", address, "");
  if (address != "") {
    bleClientMod.noteKnownAddress(name, address);
  }
  return rv;
}

void DimmerBLEClient::configSave(IotsaConfigFileSave& cf, const String& n_name) {
//xxxjack  String s_num = String(num);
//xxxjack  String s_name = "dimmer" + s_num;
  String address = String(getAddress().c_str());
  if (address != "") cf.put(n_name + ".address", address);
  AbstractDimmer::configSave(cf, n_name);
}

// getHandler() also exists on both unrelated bases (both virtual, same
// signature) -- one override here legitimately satisfies both, but has to
// call each explicitly since the compiler won't chain them on its own. Both
// happen to write a "name" field into reply -- harmless (not a bug): calling
// AbstractDimmer's second means its value always wins, and setKnownName()
// (see setName() above) keeps it identical to the inherited bleName anyway.
void DimmerBLEClient::getHandler(JsonObject& reply) {
  IotsaBLEClientDevice::getHandler(reply);
  AbstractDimmer::getHandler(reply);
}

// All three methods below used to force _dataValid=true in their "not
// following" branch. Harmless for years because nothing ever actually called
// followDimmerChanges(false) or refresh()/setup() on a not-followed dimmer --
// every consumer (lissabonRemote, lissabonSimpleRemote, and this app before
// cwi-dis/lissabon#31) called followDimmerChanges(true) unconditionally on
// every dimmer, so the branch was dead code. lissabonController's #31 change
// (only the selected dimmer follows) made it live for the first time: every
// not-yet-selected dimmer now hits it, and got its icon forced to "synced"
// (the blank icon) regardless of whether it had ever actually been synced --
// confirmed live 2026-09-26, "usually empty" icons. _dataValid should reflect
// whether we genuinely have current data, not "we've stopped trying to get
// it, so pretend it's fine" -- removed.
void DimmerBLEClient::setup() {
  if (listenForDeviceChanges && available()) {
    needSyncFromDevice = listenForDeviceChanges;
    _dataValid = false;
    needTransmitTimeoutAtMillis = millis() + unreachableGiveUpMillis;
  }
}

void DimmerBLEClient::refresh() {
  if (listenForDeviceChanges) {
    needSyncFromDevice = listenForDeviceChanges;
  }
}

void DimmerBLEClient::followDimmerChanges(bool follow) {
  listenForDeviceChanges = follow;
  if (listenForDeviceChanges && available()) {
    needSyncFromDevice = listenForDeviceChanges;
    _dataValid = false;
    needTransmitTimeoutAtMillis = millis() + unreachableGiveUpMillis;
  }
}

void DimmerBLEClient::identify() {
  needIdentify = true;
  updateDimmer();
}


#ifdef IOTSA_WITH_BLE_TASKS
void DimmerBLEClient::_connectionTask(void *arg) {
  DimmerBLEClient *_this = reinterpret_cast<DimmerBLEClient *>(arg);
  _this->connectionTask();
}

void DimmerBLEClient::connectionTask() {
  IotsaSerial.printf("DimmerBLEClient: %d: connection task created\n", num);
  uint32_t maxWaitMs = 1000;
  while(true) {
    auto v = ulTaskNotifyTake(0, pdMS_TO_TICKS(maxWaitMs));
    maxWaitMs = 1000; // May be lowered by the code below.
    if (!needSyncToDevice && !needSyncFromDevice) {

    // But we first disconnect if we are connected-idle for long enough.
      if (disconnectAtMillis > 0 && millis() > disconnectAtMillis) {
        _isDisconnecting = true;
        _isConnecting = false;
        // release(), not disconnect(): also gives the NimBLEClient slot
        // back to the shared pool, so a device with more dimmers than
        // NIMBLE_MAX_CONNECTIONS can still round-robin through all of them
        // instead of permanently starving whichever ones connected last.
        release();
        DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: disconnect from %s\n", name.c_str());
        _availableChanged = true;
        disconnectAtMillis = 0;
      }
      continue; // Nothing to do, next time through the loop
    }
    // Check that we have enough information to connect.
    if (!available()) {
      //IotsaSerial.println("xxxjack dimmer not available");
      if (millis() > needTransmitTimeoutAtMillis) {
        IotsaSerial.printf("DimmerBLEClient: Giving up on connecting to %s\n", name.c_str());
        needSyncToDevice = false;
        needSyncFromDevice = false;
        continue;
      }
      maxWaitMs = 20;
    }
    if (!isConnected()) {
      if (isDisconnecting()) {
        // Previous disconnect() hasn't been confirmed complete yet.
        // NimBLEClient::connect() hard-rejects while it's still settling --
        // wait for it rather than trying and failing.
        maxWaitMs = 20;
        continue;
      }
      // Connecting and scanning are mutually exclusive on this stack;
      // canConnect() requests any in-progress scan to stop and reports
      // whether it's worth attempting a connect right now (cwi-dis/iotsa#143).
      if (!canConnect()) {
        if (millis() > noWarningPrintBefore) {
          IotsaSerial.printf("DimmerBLEClient: BLE busy, cannot connect to %s\n", name.c_str());
          noWarningPrintBefore = millis() + 4000;
        }
        continue;
      }
      noWarningPrintBefore = 0;
      // If all that is correct, try to connect.
      _isDisconnecting = false;
      _isConnecting = true;
      _availableChanged = true;
      DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: connecting to %s\n", getName().c_str());
      if (!connect()) {
        DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: connect to %s failed\n", getName().c_str());
        _isConnecting = false;
        needSyncFromDevice = false;
        needSyncToDevice = false;
        _availableChanged = true;
        continue;
      }
      DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: connected to %s\n", getName().c_str());
      _availableChanged = true;
    }
    
    if (needSyncFromDevice) {
      _syncFromDevice();
    }
    if (needSyncToDevice) {
      _syncToDevice();
    }
    uint32_t keepOpen = min(stayConnectedMillis, (uint32_t)bleClientMod.maxConnectionKeepOpen());
    disconnectAtMillis = millis() + keepOpen;
    iotsaController.postponeSleep(keepOpen+1000);
    DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: keepopen %d\n", keepOpen);
  }
}
#endif

void DimmerBLEClient::loop() {
#ifdef IOTSA_WITH_BLE_TASKS
  if (_availableChanged) {
    _availableChanged = false;
    callbacks->dimmerAvailableChanged();
  }
  if (_dataValidChanged) {
    _dataValidChanged = false;
    callbacks->dimmerValueChanged();
  }
#else
  // If we don't have anything to transmit we bail out quickly...
  if (!needSyncToDevice && !needSyncFromDevice) {

    // But we first disconnect if we are connected-idle for long enough.
    if (disconnectAtMillis > 0 && millis() > disconnectAtMillis) {
      _isDisconnecting = true;
      _isConnecting = false;
      // release(), not disconnect(): see the IOTSA_WITH_BLE_TASKS version
      // of this same logic above.
      release();
      DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: disconnect from %s\n", name.c_str());
      callbacks->dimmerAvailableChanged();
      disconnectAtMillis = 0;
    }
    return;
  }
  // Check that we have enough information to connect.
  if (!available()) {
    //IotsaSerial.println("xxxjack dimmer not available");
    if (millis() > needTransmitTimeoutAtMillis) {
      IotsaSerial.printf("DimmerBLEClient: Giving up on connecting to %s\n", name.c_str());
      needSyncToDevice = false;
      needSyncFromDevice = false;
      return;
    }
    // iotsaBLEClient should be listening for advertisements
    return;
  }
  // Now we can connect, unless we are already connected
  if (!isConnected()) {
    if (isDisconnecting()) {
      // Previous disconnect() hasn't been confirmed complete yet.
      // NimBLEClient::connect() hard-rejects while it's still settling --
      // wait for it rather than trying and failing.
      return;
    }
    // Connecting and scanning are mutually exclusive on this stack;
    // canConnect() requests any in-progress scan to stop and reports
    // whether it's worth attempting a connect right now (cwi-dis/iotsa#143).
    // (This non-tasks path previously never requested the scan stop at all,
    // so it could get stuck behind a full discovery scan -- fixed as a side
    // effect of moving this into IotsaBLEClientDevice.)
    if (!canConnect()) {
      IotsaSerial.println("DimmerBLEClient: BLE busy, cannot connect");
      if (millis() > noWarningPrintBefore) {
        IotsaSerial.printf("DimmerBLEClient: BLE busy, cannot connect to %s\n", name.c_str());
        noWarningPrintBefore = millis() + 4000;
      }
      return;
    }
    noWarningPrintBefore = 0;
    // If all that is correct, try to connect.
    callbacks->dimmerAvailableChanged();
    _isDisconnecting = false;
    _isConnecting = true;
    callbacks->dimmerAvailableChanged();
    DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: conecting to %s\n", getName().c_str());
    if (!connect()) {
      DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: connect to %s failed\n", getName().c_str());
      _isConnecting = false;
      callbacks->dimmerAvailableChanged();
      return;
    }
    DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: connected to %s\n", getName().c_str());
    callbacks->dimmerAvailableChanged();
    return; // Return: next time through the loop we will send/receive data.
  }
  
  if (needSyncFromDevice) {
    if(_syncFromDevice()) {
      callbacks->dimmerValueChanged();
    }
  }
  if (needSyncToDevice) {
    _syncToDevice();
  }
  int keepOpen = min(stayConnectedMillis, bleClientMod.maxConnectionKeepOpen());
  disconnectAtMillis = millis() + keepOpen;
  iotsaController.postponeSleep(keepOpen+1000);
  DIMMERBLECLIENT_DEBUG IotsaSerial.printf("DimmerBLEClient: keepopen %d\n", keepOpen);
#endif
}

void DimmerBLEClient::_syncToDevice() {
  bool ok;
#ifdef DIMMER_WITH_LEVEL
  // Connected to dimmer.
  if (level < 0) level = 0;
  if (level > 1) level = 1;
  Lissabon::Type_brightness levelValue = level * ((1<<sizeof(Lissabon::Type_brightness)*8)-1);
  IFDEBUG IotsaSerial.printf("%s.syncToDevice: Transmit brightness %f (%d)\n", name.c_str(), level, levelValue);
  ok = set(Lissabon::serviceUUID, Lissabon::brightnessUUID, levelValue);
  if (ok) {
    _dataValid = true;
  } else {
    IFDEBUG IotsaSerial.println("DimmerBLEClient: set(brightness) failed");
    _dataValid = false;
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  Lissabon::Type_temperature temperatureValue = temperature;
  IFDEBUG IotsaSerial.printf("DimmerBLEClient: Transmit temperature %d\n", temperatureValue);
  ok = set(Lissabon::serviceUUID, Lissabon::temperatureUUID, (Lissabon::Type_temperature)temperatureValue);
  if (!ok) {
    IFDEBUG IotsaSerial.println("DimmerBLEClient: set(temperature) failed");
  }
#endif // DIMMER_WITH_TEMPERATURE
  IFDEBUG IotsaSerial.printf("%s.syncToDevice: Transmit ison %d\n", name.c_str(), (int)isOn);
  ok = set(Lissabon::serviceUUID, Lissabon::isOnUUID, (Lissabon::Type_isOn)isOn);
  if (!ok) {
    IFDEBUG IotsaSerial.println("DimmerBLEClient: set(isOn) failed");
  }
  if (needIdentify) {
    IFDEBUG IotsaSerial.printf("%s.syncToDevice: Transmit identify\n", name.c_str());
    // Generic core runmode identify, not Lissabon's own identifyUUID -- any
    // iotsa BLE device supports this, not just dimmers. Qualified: identify()
    // unqualified would hit DimmerBLEClient's own AbstractDimmer-facing
    // override instead (just re-sets needIdentify), not the real BLE write.
    ok = IotsaRunmodeBLEClient::identify();
    needIdentify = false;
    if (!ok) {
      IFDEBUG IotsaSerial.println("DimmerBLEClient: identify failed");
    }

  }
  needSyncToDevice = false;
}

bool DimmerBLEClient::_syncFromDevice() {
  bool ok;
  bool _gotAllData = true;
#ifdef DIMMER_WITH_LEVEL
  // Connected to dimmer.
  Lissabon::Type_brightness levelValue;
  ok = get(Lissabon::serviceUUID, Lissabon::brightnessUUID, levelValue);
  if (ok) {
    level = (float)levelValue / (float)((1<<sizeof(Lissabon::Type_brightness)*8)-1);
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: Received brightness %f (%d)\n", name.c_str(), level, levelValue);
  } else {
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: get(brightness) failed\n", name.c_str());
    _gotAllData = false;
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  Lissabon::Type_temperature temperatureValue;
  ok = get(Lissabon::serviceUUID, Lissabon::temperatureUUID, temperatureValue);
  if (ok) {
    temperature = (float)temperatureValue;
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: Received temperature %f (%d)\n", name.c_str(), temperature, temperatureValue);
  } else {
    // Temperature is optional
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: get(temperature) failed\n", name.c_str());
  }
#endif // DIMMER_WITH_TEMPERATURE
  uint8_t isOnValue;
  ok = get(Lissabon::serviceUUID, Lissabon::isOnUUID, isOnValue);
  if (ok) {
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: received isOn %d\n", name.c_str(), isOnValue);
    isOn = isOnValue;
  } else {
    _gotAllData = false;
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: get(isOn) failed\n", name.c_str());
  }
  _dataValid = _gotAllData;
  needSyncFromDevice = false;
  return _gotAllData;
}

}
#endif // IOTSA_WITH_BLE