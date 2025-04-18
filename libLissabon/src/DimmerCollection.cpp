//
// A Neopixel strip server, which allows control over a strip of neopixels, intended
// to be used as lighting. Color can be set as fraction RGB or HSL, gamma can be changed,
// interval between lit pixels can be changed. Control is through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//

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

void DimmerCollection::getHandler(JsonObject& reply) {
  for (auto d : dimmers) {
    String ident = "dimmer" + String(d->num);
    JsonObject dimmerReply = reply[ident].as<JsonObject>();
    d->getHandler(dimmerReply);
  }
}

bool DimmerCollection::putHandler(const JsonVariant& request) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  for (auto d : dimmers) {
    String ident = "dimmer" + String(d->num);
    JsonVariant dimmerRequest = reqObj[ident];
    if (dimmerRequest) {
      if (d->putHandler(dimmerRequest)) anyChanged = true;
    }
  }
  return anyChanged;
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

bool DimmerCollection::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  bool anyChanged = false;
  for(auto d : dimmers) {
    String ident(d->num);
    if (f_name != "") ident = f_name + "." + ident;
    anyChanged |= d->formHandler_args(server, ident, includeConfig);
  }
  return anyChanged;
}

void DimmerCollection::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  for (auto d : dimmers) {
    String ident(d->num);
    if (f_name != "") ident = f_name + "." + ident;
    String name = "Dimmer#" + ident;
    if (d->hasName()) {
      name += " " + d->getUserVisibleName();
    }
    d->formHandler_fields(message, name, ident, includeConfig);
  }
}

String DimmerCollection::info() {
  String message;
  for (auto d : dimmers) {
    message += d->info();
  }
  return message;
}
