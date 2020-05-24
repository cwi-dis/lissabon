#ifndef _DIMMERBLESERVER_H_
#define _DIMMERBLESERVER_H_
#include "iotsa.h"
#include "iotsaBLEServer.h"
#include "AbstractDimmer.h"
#include "LissabonBLE.h"

namespace Lissabon {

class DimmerBLEServer : public IotsaBLEApiProvider {
public:
  DimmerBLEServer(AbstractDimmer& _dimmer) : dimmer(_dimmer) {};
  void setup();
protected:
  AbstractDimmer& dimmer;
#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID);
  bool bleGetHandler(UUIDstring charUUID);
#endif // IOTSA_WITH_BLE
};
}
#endif // _DIMMERBLESERVER_H_