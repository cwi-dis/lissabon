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

  void getHandler(JsonObject& reply);
  bool putHandler(const JsonVariant& request);
  void setup();
  void loop();
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& f_name);
  virtual void configSave(IotsaConfigFileSave& cf, const String& f_name);
  bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig);
  void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig);
  String info();
protected:
  ItemVectorType dimmers;
};
}
#endif // _DIMMERCOLLECTION_H_