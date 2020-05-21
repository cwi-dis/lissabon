
class DimmerCallbacks {
public:
  ~DimmerCallbacks() {}
  virtual void uiButtonChanged() = 0;
  virtual void dimmerValueChanged() = 0;
};

class AbstractDimmer {
public:
  virtual ~AbstractDimmer() {}
};