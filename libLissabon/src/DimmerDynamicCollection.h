#ifndef _DIMMERDYNAMICCOLLECTION_H_
#define _DIMMERDYNAMICCOLLECTION_H_

#include "DimmerCollection.h"


namespace Lissabon {
class DimmerDynamicCollection : public DimmerCollection {
  typedef std::function<ItemType*(int num)> FactoryFunc;
public:
  void setFactory(FactoryFunc _factory) {factory = _factory; }
  void push_back_new(const String& name);
  // Deletes the dimmer at index and renumbers the rest (num == index again),
  // so the "dimmerN" config keys stay contiguous on the next configSave().
  void remove(int index);
  void clear();
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
protected:
  FactoryFunc factory;
};
}
#endif // _DIMMERDYNAMICCOLLECTION_H_