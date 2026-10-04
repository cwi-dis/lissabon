#ifdef IOTSA_WITH_BLE
#include "DimmerBLEClient.h"
#include "iotsaBLEClient.h"
#include "LissabonBLE.h"

#define DIMMERBLECLIENT_DEBUG if(1)

namespace Lissabon {

DimmerBLEClient::DimmerBLEClient(int _num, IotsaBLEClientMod &_bleClientMod, DimmerCallbacks *_callbacks, int _stayConnectedMillis)
: AbstractDimmer(_num, _callbacks),
  // Real name arrives later, via setName() -- but _bleClientMod is already
  // known, so pass it as owner right away: retarget() (called from
  // setName()) can then self-register on the very first real name, no
  // separate addDevice() call needed.
  IotsaRunmodeBLEClient(std::string(), "", &_bleClientMod),
  bleClientMod(_bleClientMod)
{
  // Stay connected this long after a sync, in case another one follows right
  // away (e.g. dragging a brightness slider) -- avoids paying the full
  // connect cost again for a quick follow-up. Connecting, lingering and
  // disconnecting is the base class's connection state machine
  // (cwi-dis/iotsa#263); it used to be a FreeRTOS task per dimmer.
  keepOpenMillis = _stayConnectedMillis;
}

DimmerBLEClient::~DimmerBLEClient() {
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
  return IotsaBLEClientDevice::isConnected();
}

void DimmerBLEClient::_requestSync() {
  requestWork(unreachableGiveUpMillis);
}

// None of the sync requests below wait for the device's address to be known:
// the connection state machine scans for it as part of the work, and gives up
// after unreachableGiveUpMillis. iotsa only scans for devices with work pending
// (cwi-dis/iotsa#263), so a dimmer that never asked would never be found.
void DimmerBLEClient::updateDimmer() {
  DIMMERBLECLIENT_DEBUG IotsaSerial.printf("%s.updateDimmer() called%s\n", name.c_str(), available() ? "" : " (not found yet)");
  needSyncToDevice = true;
  _requestSync();
  if (callbacks) callbacks->dimmerValueChanged();
}

// Delegates the BLE-identity half (bleName + owner registration) to the
// base class's retarget() -- previously hand-rolled here (delDevice/
// clearDevice/setKnownName/addDevice), now shared with every other
// IotsaBLEClientDevice consumer (cwi-dis/iotsa#268). AbstractDimmer::name
// still needs its own assignment: it's a separate, dimmer-specific field
// (display name / config key) that just happens to always equal bleName in
// this app -- see the "name double-write" comment on getHandler() below.
bool DimmerBLEClient::setName(String value) {
  if (value == name) return false;
  name = value;
  retarget(std::string(value.c_str()));
  return true;
}

// includeConfig branch: AbstractDimmer's own fields, then the generic
// name/address/found status from the base (text="" -- AbstractDimmer
// already printed its own "Dimmer N: " + text header). Unlike the base's
// own default (status shown either way), the pre-existing DimmerBLEClient
// convention only showed it in the config form -- keep that; not worth
// changing existing deployed-device page layout as a side effect of this
// refactor.
void DimmerBLEClient::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  AbstractDimmer::formHandler_fields(message, text, f_name, includeConfig);
  if (!_dataValid) {
    message += "<i>(data may be outdated or invalid)</i><br>";
  }
  if (includeConfig) {
    IotsaBLEClientDevice::formHandler_fields(message, "", f_name, includeConfig);
  }
}

bool DimmerBLEClient::configLoad(IotsaConfigFileLoad& cf, const String& n_name) {
  bool rv = AbstractDimmer::configLoad(cf, n_name);
  IotsaBLEClientDevice::configLoad(cf, n_name);
  return rv;
}

void DimmerBLEClient::configSave(IotsaConfigFileSave& cf, const String& n_name) {
  AbstractDimmer::configSave(cf, n_name);
  IotsaBLEClientDevice::configSave(cf, n_name);
}

// getHandler()/putHandler()/formHandler_TD()/formHandler_args() all exist on
// both unrelated bases now (same signature) -- one override here legitimately
// satisfies both, but has to call each explicitly since the compiler won't
// chain them on its own. Both getHandler()/configSave() happen to write a
// "name" field -- harmless (not a bug): calling AbstractDimmer's second means
// its value always wins, and setName() above keeps it identical to the
// inherited bleName anyway. Same reasoning covers putHandler()/
// formHandler_args() both matching a submitted "name" field: whichever runs
// first (AbstractDimmer's, via its own setName() override) already syncs
// both, so the base's own attempt is a harmless no-op retarget().
//
// Deliberately qualified all the way up to IotsaBLEClientDevice, not
// IotsaRunmodeBLEClient (the immediate parent) -- IotsaRunmodeBLEClient's own
// getHandler()/putHandler()/formHandler_fields()/formHandler_args() now add
// an identify/reboot/promoteMode/setWifiDisabled command-queue surface
// (cwi-dis/iotsa#264's BLEController), which would be redundant here:
// DimmerBLEClient already has its own identify path (needIdentify, fired
// from doWork()) and has no use for the other three.
void DimmerBLEClient::getHandler(JsonObject& reply) {
  IotsaBLEClientDevice::getHandler(reply);
  AbstractDimmer::getHandler(reply);
}

bool DimmerBLEClient::putHandler(const JsonVariant& request) {
  bool any = AbstractDimmer::putHandler(request);
  any |= IotsaBLEClientDevice::putHandler(request);
  return any;
}

void DimmerBLEClient::formHandler_TD(String& message, bool includeConfig) {
  AbstractDimmer::formHandler_TD(message, includeConfig); // unimplemented, kept for forward-compat
  IotsaBLEClientDevice::formHandler_TD(message, includeConfig);
}

bool DimmerBLEClient::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  bool any = AbstractDimmer::formHandler_args(server, f_name, includeConfig);
  any |= IotsaBLEClientDevice::formHandler_args(server, f_name, includeConfig);
  return any;
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
  if (listenForDeviceChanges) {
    needSyncFromDevice = true;
    _dataValid = false;
    _requestSync();
  }
}

void DimmerBLEClient::refresh() {
  if (listenForDeviceChanges) {
    needSyncFromDevice = true;
    _requestSync();
  }
}

void DimmerBLEClient::followDimmerChanges(bool follow) {
  listenForDeviceChanges = follow;
  if (listenForDeviceChanges) {
    needSyncFromDevice = true;
    _dataValid = false;
    _requestSync();
  }
}

void DimmerBLEClient::identify() {
  needIdentify = true;
  updateDimmer();
}


bool DimmerBLEClient::doWork() {
  // Connected. Called by the connection state machine on the main loop, so
  // the callbacks can be called directly.
  bool ok = true;
  if (needSyncFromDevice) {
    if (_syncFromDevice()) {
      if (callbacks) callbacks->dimmerValueChanged();
    } else {
      ok = false;
    }
  }
  if (needSyncToDevice) {
    _syncToDevice();
  }
  return ok;
}

void DimmerBLEClient::workAbandoned() {
  IotsaSerial.printf("DimmerBLEClient: giving up on %s\n", name.c_str());
  needSyncToDevice = false;
  needSyncFromDevice = false;
  needIdentify = false;
}

void DimmerBLEClient::loop() {
  // Report link-state changes (connecting, connected, gone) once each: the
  // controller's display shows them.
  LinkState st = getLinkState();
  if (st != lastReportedLinkState) {
    lastReportedLinkState = st;
    if (callbacks) callbacks->dimmerAvailableChanged();
  }
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