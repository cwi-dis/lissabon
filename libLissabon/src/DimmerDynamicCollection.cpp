#include "DimmerDynamicCollection.h"

using namespace Lissabon;

void DimmerDynamicCollection::push_back_new(const String& name) {
  ItemType *item = factory(dimmers.size());
  item->setName(name);
  push_back(item);
}

void DimmerDynamicCollection::remove(int index) {
  if (index < 0 || index >= (int)dimmers.size()) return;
  delete dimmers[index];
  dimmers.erase(dimmers.begin() + index);
  for (int i = 0; i < (int)dimmers.size(); i++) {
    dimmers[i]->num = i;
  }
}

void DimmerDynamicCollection::clear() {
  for(auto d : dimmers) {
    delete d;
  }
  dimmers.clear();
}

bool DimmerDynamicCollection::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  if (!factory) {
    IotsaSerial.println("DimmerDynamicCollection.configLoad requires factory");
    return false;
  }
  String ident = "n_dimmer";
  if (f_name != "") ident = f_name + "." + ident;
  int n_dimmer;
  cf.get(ident, n_dimmer, 0);
  for(int i=dimmers.size(); i<n_dimmer; i++) {
    ItemType *item = factory(i);
    push_back(item);
  }
  return DimmerCollection::configLoad(cf, f_name);
}

void DimmerDynamicCollection::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  String ident = "n_dimmer";
  if (f_name != "") ident = f_name + "." + ident;
  cf.put(ident, (int)dimmers.size());
  DimmerCollection::configSave(cf, f_name);
}