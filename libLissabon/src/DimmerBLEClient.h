#ifndef _DIMMERBLECLIENT_H_
#define _DIMMERBLECLIENT_H_
//
// LED Lighting control module. 
//
#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEClient.h"
#include "iotsaRunmodeBLEClient.h"
#include "AbstractDimmer.h"
#include "LissabonBLE.h"


namespace Lissabon {

// Real inheritance, not composition: DimmerBLEClient IS its own BLE
// connection (via IotsaRunmodeBLEClient), not a wrapper holding a pointer to
// a separately-owned one. Two unrelated base classes happen to declare
// same-named methods (available(), isConnected(), identify(), getHandler(),
// and -- since IotsaBLEClientDevice grew a full IotsaApiModObject surface,
// cwi-dis/iotsa#268 -- also configLoad/configSave/putHandler/
// formHandler_fields/formHandler_TD/formHandler_args); see the .cpp for how
// each is resolved (either a single override serving both, or an explicit
// base-qualified call from within DimmerBLEClient's own override to reach
// the other one).
class DimmerBLEClient : public AbstractDimmer, public IotsaRunmodeBLEClient {
public:
  DimmerBLEClient(int _num, IotsaBLEClientMod &_bleClientMod, DimmerCallbacks *_callbacks, int _stayConnectedMillis=0);
  ~DimmerBLEClient();
  void followDimmerChanges(bool follow);
  void updateDimmer();
  bool available() override;
  bool isConnected();
  // Work pending (a sync waiting for a connection) or a connect in progress.
  bool isConnecting() { return hasPendingWork() || getLinkState() == LinkState::Connecting; }
  void refresh();
  bool dataValid() override { return _dataValid; }
  bool setName(String value);
  void setup() override;
  void loop() override;
  void identify() override;
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) override;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) override;
  virtual void getHandler(JsonObject& reply) override;
  virtual bool putHandler(const JsonVariant& request) override;
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
  virtual void formHandler_TD(String& message, bool includeConfig) override;
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) override;
protected:
  // Connecting, lingering and disconnecting is IotsaBLEClientDevice's
  // connection state machine (cwi-dis/iotsa#263, #144); we only say that
  // there's a sync to do (requestWork()) and do it (doWork()).
  bool doWork() override;
  void workAbandoned() override;
  void _requestSync();
  void _syncToDevice();
  bool _syncFromDevice();
  IotsaBLEClientMod& bleClientMod;
  bool listenForDeviceChanges = false;
  bool needSyncToDevice = false;
  bool needSyncFromDevice = false;
  bool _dataValid = false;
  bool needIdentify = false;
  // How long to keep a sync request alive while the device can't be reached
  // before giving up on it. In practice this should be comfortably above any
  // live device's real sleep/wake or advertise cadence, so it firing means
  // the device is genuinely gone (powered down, out of range), not just slow
  // to find -- though BLE itself puts no ceiling on a peer's sleep cycle, so
  // that's an assumption about today's fleet, not a protocol guarantee.
  const uint32_t unreachableGiveUpMillis = 10000;
  // Last link state reported through dimmerAvailableChanged(), so loop()
  // reports each change once (the controller's display shows it).
  LinkState lastReportedLinkState = LinkState::Idle;
};
};
#endif // _DIMMERBLECLIENT_H_