#ifndef _DIMMERBLESERVER_H_
#define _DIMMERBLESERVER_H_
#include "iotsa.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
#include "AbstractDimmer.h"
#include "LissabonBLE.h"

namespace Lissabon {

class DimmerBLEServer : public IotsaBLEProvider {
public:
  DimmerBLEServer(AbstractDimmer& _dimmer) : dimmer(_dimmer), auxDimmer(nullptr) {};
  void setup();
  // The aux dimmer is deliberately not exposed over BLE on its own. Think of a
  // standing lamp with a big lamp on top and a small reading light at the bottom:
  // switching it on over BLE should only light the main dimmer, but switching it
  // off should turn off both. So: writing isOn=0 also turns off the aux dimmer,
  // and reading isOn reports "on" if either is on (otherwise a remote would see
  // "off" while the reading light is on, and could not turn it off).
  void setAuxDimmer(AbstractDimmer* _auxDimmer) { auxDimmer = _auxDimmer; }
protected:
  AbstractDimmer& dimmer;
  AbstractDimmer* auxDimmer;
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  // Not a module, so no module name: "iotsaApi: ble: write dimmer ..." (cwi-dis/iotsa#182).
  const char *bleLogName() override { return "dimmer"; }
};
}
#endif // IOTSA_WITH_BLE
#endif // _DIMMERBLESERVER_H_