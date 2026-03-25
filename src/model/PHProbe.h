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
 * The datasheet often uses lowercase when uppercase is required.
 * For example "Slope,?" should be "SLOPE,?" and so on.
 */

enum pHProbeState {
  BOOT,                 // booting (continuous read disabled)
  CONTINUOUS_READ,      // reporting pH values every second
  CALIBRATION,          // being calibrated (continuous read disabled)
  EXPORT_CALIBRATION,   // exporting its calibration (continuous read disabled)
  IMPORT_CALIBRATION,   // importing a calibration (continuous read disabled)
  SLOPE,                // reporting its slope (continuous read active)
  THERMAL_COMPENSATION  // importing a temperature (continuous read active)
};

class PHProbe {
public:
  static PHProbe* instance();
  void clearCalibration();
  void getCalibrationStatus(char* buffer, int size);
  void getCalibrationString(char* buffer, int size);
  float getPh() {
    return pHValue;
  }
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
  const char* getCalibrationStatus() const {
    return calibrationStatus;
  }
  void getCalibrationStringValue(char* buffer, int size) const;
  bool getReceivingCalibrationString() const {
    return receivingCalibrationString;
  }
  const char* getSlopeResponse() const {
    return slopeResponse;
  }
  pHProbeState getState() {
    return state;
  }
  bool getContinuousReadActive() {
    return continuousReadActive;
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
  void setState(pHProbeState newState) {
    state = newState;
  }
#endif
private:
  // Class variable
  static PHProbe* _instance;
  // instance variable
  char calibrationStatus[17] = "";  // 0-point, 1-point, 2-point, or 3-point
  char calibrationString[121] = "";
  float pHValue = 0;
  bool receivingCalibrationString = false;
  char slopeResponse[32] = "";
  bool slopeIsOutOfRange = false;
  pHProbeState state = BOOT;
  bool continuousReadActive = false;
  // Methods
  PHProbe();
  void requestCalibrationString();
};
