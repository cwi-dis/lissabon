//
// A Neopixel strip server, which allows control over a strip of neopixels, intended
// to be used as lighting. Color can be set as fraction RGB or HSL, gamma can be changed,
// interval between lit pixels can be changed. Control is through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//

#include "DimmerDynamicCollection.h"

using namespace Lissabon;

void DimmerDynamicCollection::push_back_new(const String& name) {
  ItemType *item = factory(dimmers.size());
  item->setName(name);
  push_back(item);
}

bool DimmerDynamicCollection::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  String ident = "n_dimmer";
  if (f_name != "") ident = f_name + "." + ident;
  int n_dimmer;
  cf.get(f_name, n_dimmer, 0);
  for(int i=dimmers.size(); i<n_dimmer; i++) {
    ItemType *item = factory(i);
    push_back(item);
  }
  return DimmerCollection::configLoad(cf, f_name);
}

void DimmerDynamicCollection::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  String ident = "n_dimmer";
  if (f_name != "") ident = f_name + "." + ident;
  cf.put(f_name, dimmers.size());
  DimmerCollection::configSave(cf, f_name);
}