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