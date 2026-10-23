#ifndef _DIMMERCOLLECTION_H_
#define _DIMMERCOLLECTION_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"

#include "AbstractDimmer.h"
#include "DimmerUI.h"

//#include <ArduinoJson.h>
//using namespace ArduinoJson;

namespace Lissabon {
// The dimmers of a remote or controller, in config-file order ("dimmerN").
// Only loading, saving and finding: the web page and REST interface show the
// dimmers through IotsaBLEClientManagerMod, keyed by name (cwi-dis/iotsa#183
// downstream sweep, a step towards cwi-dis/lissabon#37).
class DimmerCollection {
public:
  typedef AbstractDimmer ItemType;
  typedef std::vector<ItemType*> ItemVectorType;
  typedef ItemVectorType::iterator iterator;

  int size();
  void push_back(ItemType* dimmer);
  ItemType* at(int i);
  ItemType* find(const String& name);
  iterator begin();
  iterator end();

  void setup();
  void loop();
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& f_name);
  virtual void configSave(IotsaConfigFileSave& cf, const String& f_name);
  String info();
protected:
  ItemVectorType dimmers;
};
}
#endif // _DIMMERCOLLECTION_H_