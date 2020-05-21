#ifndef _ABSTRACTDIMMER_H_
#define _ABSTRACTDIMMER_H_
namespace Lissabon {
class DimmerCallbacks {
public:
  ~DimmerCallbacks() {}
  virtual void uiButtonChanged() = 0;
  virtual void dimmerValueChanged() = 0;
};

class AbstractDimmer {
public:
  AbstractDimmer(int _num, DimmerCallbacks* _callbacks)
  : num(_num),
    callbacks(_callbacks)
  {}
  virtual ~AbstractDimmer() {}
  virtual void updateDimmer() = 0;
public:
  int num;
  DimmerCallbacks *callbacks;
  bool isOn;
  float level;
  float minLevel;
};
};
#endif // _ABSTRACTDIMMER_H_