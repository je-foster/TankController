#pragma once
#include "Devices/PHProbe.h"
#include "NumberCollectorState.h"

class PHCalibration : public NumCollectorState {
public:
  PHCalibration(TankController* tc) : NumCollectorState(tc) {
  }
  bool isInCalibration() override {
    return true;  // disable controls during calibration
  }
  float getCurrentValue() override {
    return 0;
  }
  void handleKey(char key) {
    if (key == 'D') {  // cancel but show calibration status
      this->setNextState(new Wait(tc, 3000, new SeePHCalibration(tc, true)));
    } else {
      NumberCollectorState::handleKey(key);
    }
  }
  void loop() override {
    printValue();
  }

protected:
  uint16_t getCurrentValuePrecision() override {
    return 3;
  }
  bool showCurrentValue() override {
    return false;
  }
};
