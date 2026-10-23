#include "DimmerCollection.h"

using namespace Lissabon;

int DimmerCollection::size() { 
  return dimmers.size(); 
}

void DimmerCollection::push_back(DimmerCollection::ItemType* dimmer) { 
  dimmers.push_back(dimmer);
}

DimmerCollection::ItemType* DimmerCollection::at(int i) { 
  if (i<0||i>=dimmers.size()) return nullptr; 
  return dimmers.at(i); 
}

DimmerCollection::ItemType* DimmerCollection::find(const String& name) { 
  for(auto d : dimmers) {
    if (d->getUserVisibleName() == name) return d;
  }
  return nullptr;
}

DimmerCollection::iterator DimmerCollection::begin() { return dimmers.begin(); }

DimmerCollection::iterator DimmerCollection::end() { return dimmers.end(); }

void DimmerCollection::setup() {
  for(auto d : dimmers) {
    d->setup();
  }
}

void DimmerCollection::loop() {
  for(auto d : dimmers) {
    d->loop();
  }
}

bool DimmerCollection::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  bool rv = false;
  for (auto d : dimmers) {
    String ident = "dimmer" + String(d->num);
    if (f_name != "") ident = f_name + "." + ident;
    rv |= d->configLoad(cf, ident);
  }
  return rv;
}

void DimmerCollection::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  for (auto d : dimmers) {
    String ident = "dimmer" + String(d->num);
    if (f_name != "") ident = f_name + "." + ident;
    d->configSave(cf, ident);
  }
}

String DimmerCollection::info() {
  String message;
  for (auto d : dimmers) {
    message += d->info();
  }
  return message;
}
