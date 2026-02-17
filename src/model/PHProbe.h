#pragma once
#include <Arduino.h>

/**
 * wrapper class for EZO pH Circuit | Atlas Scientific
 *
 * https://www.atlas-scientific.com/files/pH_EZO_Datasheet.pdf
 *
 * Issuing the "Cal,mid,n\r" command will
 * clear the other calibration points.
 *
 * While the data sheet uses "Slope" the actual string is "SLOPE"
 * Similarly, "Cal,?" is actually "CAL,?" and responses are "?CAL,2" for example.
 */

const uint32_t THERMAL_COMPENSATION_INTERVAL = 60000;  // 1 min

class PHProbe {
public:
  static PHProbe* instance();

  float getPh() {
    return value;
  }
  void clearCalibration();
  void getCalibration(char* buffer, int size);
  void getSlope(char* buffer, int size);
  void loop();
  void sendCalibrationRequest();
  void sendSlopeRequest();
  void serialEvent1();
  void setHighpointCalibration(float highpoint);
  void setLowpointCalibration(float lowpoint);
  void setMidpointCalibration(float midpoint);
  void setThermalCompensation(float temperature);
  bool shouldWarnAboutCalibration();
  bool slopeIsBad() {
    return slopeIsOutOfRange;
  }
#if defined(ARDUINO_CI_COMPILATION_MOCKS)
  const char* getCalibrationResponse() const {
    return calibrationResponse;
  }
  const char* getSlopeResponse() const {
    return slopeResponse;
  }
  void setCalibration(int calibrationPoints = 0);
  void setPh(float newValue);
  void setPhSlope(const char* slope = "?SLOPE,99.7,100.3,-0.89\r");
#endif
private:
  // Class variable
  static PHProbe* _instance;
  // instance variable
  float value = 0;
  uint32_t nextThermalCompensationTime = THERMAL_COMPENSATION_INTERVAL;
  char calibrationResponse[17] = "";
  char slopeResponse[32] = "";
  bool slopeIsOutOfRange = false;
  // Methods
  PHProbe();
};
