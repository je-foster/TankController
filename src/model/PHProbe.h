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

// getValue() function is for testing purposes
class PHProbe {
public:
  static PHProbe* instance();
  float getPh() {
    return pHValue;
  }
  void clearCalibration();
  void getCalibration(char* buffer, int size);
  void getCalibrationString(char* buffer, int size);
  void getSlope(char* buffer, int size);
  void sendCalibrationRequest();
  void sendSlopeRequest();
  void serialEvent1();
  void setHighpointCalibration(float highpoint);
  void setLowpointCalibration(float lowpoint);
  void setMidpointCalibration(float midpoint);
  void setTemperatureCompensation(float temperature);
  bool shouldWarnAboutCalibration();
  bool slopeIsBad() {
    return slopeIsOutOfRange;
  }
#if defined(ARDUINO_CI_COMPILATION_MOCKS)
  const char* getCalibrationResponse() const {
    return calibrationResponse;
  }
  void getCalibrationStringValue(char* buffer, int size) const;
  bool getReceivingCalibrationString() const {
    return receivingCalibrationString;
  }
  const char* getSlopeResponse() const {
    return slopeResponse;
  }
  void resetCalibrationString() {
    calibrationString[0] = '\0';
  }
  void sendCalibrationStringSegment(const char* segment);
  void setCalibration(int calibrationPoints = 0);
  void setCalibrationString(const char* value =
                                "596F7520617265206120636F33333333333344444444444455555555555566666666666677777777777788"
                                "88888888889999999999996F6C"
                                "20677579");
  void setPh(float newValue);
  void setPhSlope(const char* slope = "?SLOPE,99.7,100.3,-0.89\r");
  void setReceivingCalibrationString(bool value) {
    receivingCalibrationString = value;
  }
#endif
private:
  // Class variable
  static PHProbe* _instance;
  // instance variable
  char calibrationResponse[17] = "";
  char calibrationString[121] = "";
  float pHValue = 0;
  bool receivingCalibrationString = false;
  void requestCalibrationString();
  char slopeResponse[32] = "";
  bool slopeIsOutOfRange = false;
  // Methods
  PHProbe();
};
