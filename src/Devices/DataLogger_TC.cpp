#include "DataLogger_TC.h"

#include "Devices/DateTime_TC.h"
#include "Devices/EEPROM_TC.h"
#include "Devices/PHControl.h"
#include "Devices/PHProbe.h"
#include "Devices/PID_TC.h"
#include "Devices/SD_TC.h"
#include "Devices/Serial_TC.h"
#include "Devices/TempProbe_TC.h"
#include "Devices/TemperatureControl.h"
#include "TC_util.h"
#include "TankController.h"

//  class variables
DataLogger_TC* DataLogger_TC::_instance = nullptr;

//  class methods
/**
 * @brief accessor for singleton
 *
 */
DataLogger_TC* DataLogger_TC::instance() {
  if (!_instance) {
    _instance = new DataLogger_TC();
  }
  return _instance;
}

//  instance methods
void DataLogger_TC::loop() {
  // Is it time to log?
  unsigned long msNow = millis();
  if (msNow >= nextSparseDataLogTime) {
    writeToSparseDataLog();
    nextSparseDataLogTime =
        (msNow / (unsigned long)SPARSE_LOGGING_INTERVAL + 1) * (unsigned long)SPARSE_LOGGING_INTERVAL;
  } else if (msNow >= nextComprehensiveDataLogTime) {
    writeToComprehensiveDataLog();
    nextComprehensiveDataLogTime =
        (msNow / (unsigned long)COMPREHENSIVE_LOGGING_INTERVAL + 1) * (unsigned long)COMPREHENSIVE_LOGGING_INTERVAL;
  } else if (msNow >= nextSerialLogTime) {
    writeToSerial();
    nextSerialLogTime = (msNow / (unsigned long)SERIAL_LOGGING_INTERVAL + 1) * (unsigned long)SERIAL_LOGGING_INTERVAL;
  }
}

/**
 * @brief Compare recorded temperature and pH extremes to the provided values. Replace them if current values are more
 * extreme or if 'reset' is true.
 *
 */
void DataLogger_TC::updateExtremes(float currentTemp, float currentPh, bool reset) {
  if (reset) {
    minTemperature = currentTemp;
    maxTemperature = currentTemp;
    minPh = currentPh;
    maxPh = currentPh;
  } else {
    if (currentTemp < minTemperature) {
      minTemperature = currentTemp;
    } else if (currentTemp > maxTemperature) {
      maxTemperature = currentTemp;
    }
    if (currentPh < minPh) {
      minPh = currentPh;
    } else if (currentPh > maxPh) {
      maxPh = currentPh;
    }
  }
}

/**
 * @brief write the current data to the serial port
 *
 */
void DataLogger_TC::writeToSerial() {
  DateTime_TC dtNow = DateTime_TC::now();
  char bufferPh[12];
  char bufferTemperature[11];
  if (TankController::instance()->isInCalibration()) {
    strscpy_P(bufferPh, (PGM_P)F("C"), sizeof(bufferPh));
    strscpy_P(bufferTemperature, (PGM_P)F("C"), sizeof(bufferTemperature));
  } else {
    floattostrf((float)PHProbe::instance()->getPh(), 5, 3, bufferPh, sizeof(bufferPh));
    floattostrf((float)TempProbe_TC::instance()->getRunningAverage(), 5, 2, bufferTemperature,
                sizeof(bufferTemperature));
  }
  serial(F("%02d:%02d pH=%s temp=%s"), (uint16_t)dtNow.hour(), (uint16_t)dtNow.minute(), bufferPh, bufferTemperature);
}

/**
 * @brief write the current data to the huge log file on the SD
 *
 */
void DataLogger_TC::writeToComprehensiveDataLog() {
  char currentTemperature[10];
  char currentPh[10];
  if (TankController::instance()->isInCalibration()) {
    strscpy_P(currentTemperature, (PGM_P)F("C"), sizeof(currentTemperature));
    strscpy_P(currentPh, (PGM_P)F("C"), sizeof(currentPh));
  } else {
    float currentTempFloat = (float)TempProbe_TC::instance()->getRunningAverage();
    float currentPhFloat = (float)PHProbe::instance()->getPh();
    floattostrf(currentTempFloat, 4, 2, currentTemperature, sizeof(currentTemperature));
    floattostrf(currentPhFloat, 5, 3, currentPh, sizeof(currentPh));
    updateExtremes(currentTempFloat, currentPhFloat, false);
  }
  DateTime_TC dtNow = DateTime_TC::now();
  PID_TC* pPID = PID_TC::instance();
  uint16_t tankId = EEPROM_TC::instance()->getTankID();
  char targetTemp[10];
  char targetPh[10];
  char kp[12];
  char ki[12];
  char kd[12];
  floattostrf(TemperatureControl::instance()->getTargetTemperature(), 4, 2, targetTemp, sizeof(targetTemp));
  floattostrf(PHControl::instance()->getTargetPh(), 5, 3, targetPh, sizeof(targetPh));
  floattostrf(pPID->getKp(), 8, 1, kp, sizeof(kp));
  floattostrf(pPID->getKi(), 8, 1, ki, sizeof(ki));
  floattostrf(pPID->getKd(), 8, 1, kd, sizeof(kd));
  static const char header[] PROGMEM = "time,tankid,temp,temp setpoint,pH,pH setpoint,onTime,Kp,Ki,Kd";
  static const char format[] PROGMEM = "%02i/%02i/%4i %02i:%02i:%02i, %3i, %s, %s, %s, %s, %4lu, %s, %s, %s";
  char header_buffer[sizeof(header)];
  strscpy_P(header_buffer, (PGM_P)header, sizeof(header_buffer));
  char buffer[128];
  int length;
  length = snprintf_P(buffer, sizeof(buffer), (PGM_P)format, (uint16_t)dtNow.month(), (uint16_t)dtNow.day(),
                      (uint16_t)dtNow.year(), (uint16_t)dtNow.hour(), (uint16_t)dtNow.minute(),
                      (uint16_t)dtNow.second(), (uint16_t)tankId, currentTemperature, targetTemp, currentPh, targetPh,
                      (unsigned long)(millis() / 1000), kp, ki, kd);
  if ((length > sizeof(buffer)) || (length < 0)) {
    // TODO: Log a warning that string was truncated
    serial(F("WARNING! String was truncated to \"%s\""), buffer);
  }
  SD_TC::instance()->appendData(header_buffer, buffer);
}

/**
 * @brief Write the current data to the smaller log file on the SD. Also reset recorded extreme temperature and pH
 * values to current values (unless device is in calibration).
 *
 */
void DataLogger_TC::writeToSparseDataLog() {
  char currentTemperature[10];
  char minTemperatureString[10];
  char maxTemperatureString[10];
  char currentPh[10];
  char minPhString[10];
  char maxPhString[10];
  if (TankController::instance()->isInCalibration()) {
    strscpy_P(currentTemperature, (PGM_P)F("C"), sizeof(currentTemperature));
    strscpy_P(currentPh, (PGM_P)F("C"), sizeof(currentPh));
  } else {
    float currentTempFloat = (float)TempProbe_TC::instance()->getRunningAverage();
    float currentPhFloat = (float)PHProbe::instance()->getPh();
    floattostrf(currentTempFloat, 4, 2, currentTemperature, sizeof(currentTemperature));
    floattostrf(currentPhFloat, 5, 3, currentPh, sizeof(currentPh));
    updateExtremes(currentTempFloat, currentPhFloat, false);
  }
  floattostrf(minTemperature, 4, 2, minTemperatureString, sizeof(minTemperatureString));
  floattostrf(maxTemperature, 4, 2, maxTemperatureString, sizeof(maxTemperatureString));
  floattostrf(minPh, 5, 3, minPhString, sizeof(minPhString));
  floattostrf(maxPh, 5, 3, maxPhString, sizeof(maxPhString));
  if (!TankController::instance()->isInCalibration()) {
    updateExtremes(currentTempFloat, currentPhFloat, true);
  }
  DateTime_TC dtNow = DateTime_TC::now();
  static const char format[] PROGMEM = "%02i:%02i,%s,%s,%s,%s,%s,%s";
  char buffer[72];
  int length;
  length =
      snprintf_P(buffer, sizeof(buffer), (PGM_P)format, (uint16_t)dtNow.hour(), (uint16_t)dtNow.minute(),
                 currentTemperature, minTemperatureString, maxTemperatureString, currentPh, minPhString, maxPhString);
  if ((length > sizeof(buffer)) || (length < 0)) {
    // TODO: Log a warning that string was truncated
    serial(F("WARNING! String was truncated to \"%s\""), buffer);
  }
  serial(F("Sparse Data Log: %s"), buffer);
  SD_TC::instance()->appendToSparseDataLog(buffer);
}
