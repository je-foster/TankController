#include "model/PHProbe.h"

#include <avr/wdt.h>
#include <stdlib.h>

#include "model/DataLogger.h"
#include "model/TC_util.h"
#include "wrappers/EEPROM_TC.h"
#include "wrappers/Serial_TC.h"

//  class instance variables
/**
 * static variable for singleton
 */
PHProbe* PHProbe::_instance = nullptr;

//  class methods
/**
 * @brief static member function to return singleton
 *
 */
PHProbe* PHProbe::instance() {
  if (!_instance) {
    _instance = new PHProbe();
  }
  return _instance;
}

//  instance methods
/**
 * @brief constructor (private so clients use the singleton)
 *
 */
PHProbe::PHProbe() {
  Serial1.begin(9600);
  // wait for Serial Monitor to connect. Needed for native USB port boards only:
  while (!Serial1)
    ;
  // Serial1.print(F("*OK,0\r"));  // Turn off the returning of OK after command to EZO pH
  Serial1.print(F("C,1\r"));  // Set probe to measure pH once per second
  waitingForConfirmation = true;
  sendSlopeRequest();
}

/**
 * @brief Reset EZO calibration to default
 *
 */
void PHProbe::clearCalibration() {
  state = CALIBRATION;
  slopeIsOutOfRange = false;                          // Clear warnings about current calibration
  EEPROM_TC::instance()->setIgnoreBadPHSlope(false);  // Watch for new bad calibrations
  calibrationString[0] = '\0';
  Serial1.print(F("Cal,clear\r"));
  waitingForConfirmation = true;
}

/**
 * @brief Ask the EZO probe for its calibration status (single point, three point, etc.)
 *
 */
void PHProbe::sendCalibrationRequest() {
  Serial1.print(F("CAL,?\r"));
  // The new status message will show on the display until the request is answered
  strscpy_P(calibrationStatus, F("PH Calibration"), sizeof(calibrationStatus));
}

/**
 * @brief Put the current calibration status message into buffer
 *
 * @param buffer
 * @param size
 */
void PHProbe::getCalibrationStatus(char* buffer, int size) {
  strscpy(buffer, calibrationStatus, size);
}

// void PHProbe::getCalibrationString(char* buffer, int size) {
//   // If calibrationString is not empty, then we assume it is correct. If calibrationString
//   // is empty, we request it from the EZO probe. Any function that causes a change in the
//   // calibration of the EZO probe should erase calibrationString.
//   if (strnlen(calibrationString, sizeof(calibrationString)) == 0 && !receivingCalibrationString) {
//     Serial1.print(F("C,0\r"));  // Tell EZO probe to stop sending pH measurements
//     this->requestCalibrationString();
//   }
//   // Send the calibration string only if it is complete
//   if (receivingCalibrationString) {
//     strscpy_P(buffer, F("Requesting..."), size);
//   } else {
//     strscpy(buffer, calibrationString, size);
//   }
// }

/**
 * @brief Ask the EZO probe for its slope
 *
 */
void PHProbe::sendSlopeRequest() {
  Serial1.print(F("SLOPE,?\r"));
  // The new status message will show on the display until the request is answered
  strscpy_P(slopeResponse, F("Requesting..."), sizeof(slopeResponse));
}

/**
 * @brief Put the latest slope data into buffer. Could be "99.7,100.3, -0.89" or "Requesting..."
 *
 * @param buffer
 * @param size
 */
void PHProbe::getSlope(char* buffer, int size) {
  strscpy(buffer, slopeResponse, size);
}

// /**
//  * @brief Ask the EZO probe to export its calibration string
//  *
//  */
// void PHProbe::requestCalibrationString() {
//   receivingCalibrationString = true;
//   Serial1.print(F("EXPORT\r"));
// }

/**
 * interrupt handler for data arriving from probe
 */
void PHProbe::serialEvent1() {
  // if we see that the Atlas Scientific product has sent a character
  while (Serial1.available() > 0) {
    String string = Serial1.readStringUntil('\r');  // read the string until we see a <CR>
    if (string.length() > 0 && string[string.length() - 1] == '\r') {
      // We should not see the CR (https://github.com/Arduino-CI/arduino_ci/pull/302)
      string.remove(string.length() - 1);
    }
    if (string.length() > 0) {
      // switch (state) {

      // }

      if (receivingCalibrationString) {
        if (string.length() >= 5 && memcmp_P(string.c_str(), F("*DONE"), 5) == 0) {
          DataLogger::instance()->writeWarningSoon();
          receivingCalibrationString = false;
          Serial1.print(F("C,1\r"));  // Reset pH stamp to continuous measurement: once per second
        } else {                      // append the received string to calibrationString
          int writeIndex = strnlen(calibrationString, sizeof(calibrationString));
          strscpy(calibrationString + writeIndex, string.c_str(), sizeof(calibrationString) - writeIndex);
          this->requestCalibrationString();
        }
      } else if (isdigit(string[0])) {  // if the first character in the string is a digit
        // convert the string to a floating point number so it can be evaluated by the Arduino
        pHValue = string.toFloat();
        if (pHValue < 0) {
          pHValue = 0;
        } else if (pHValue > 14) {
          pHValue = 14;
        }
      } else if (string[0] == '?') {  // answer to a previous query
        serial(F("PHProbe serialEvent1: \"%s\""), string.c_str());
        if (string.length() > 7 && memcmp_P(string.c_str(), F("?SLOPE,"), 7) == 0) {
          // for example "?SLOPE,16.1,100.0"
          DataLogger::instance()->writeWarningSoon();
          strscpy(slopeResponse, string.c_str() + 7, sizeof(slopeResponse));
          char acidSlopePercentString[7];
          char baseSlopePercentString[7];
          char millivoltOffsetString[7];
          sscanf_P(slopeResponse, PSTR(" %[^,] , %[^,] , %s"), acidSlopePercentString, baseSlopePercentString,
                   millivoltOffsetString);
          if ((95.0 <= strtofloat(acidSlopePercentString)) && (strtofloat(acidSlopePercentString) <= 105.0) &&
              (95.0 <= strtofloat(baseSlopePercentString)) && (strtofloat(baseSlopePercentString) <= 105.0)) {
            slopeIsOutOfRange = false;
            EEPROM_TC::instance()->setIgnoreBadPHSlope(false);
            serial(F("pH slopes are within 5%% of ideal"));
          } else {
            slopeIsOutOfRange = true;
            serial(F("BAD CALIBRATION? pH slopes are more than 5%% from ideal"));
          }
          // TankController::instance()->checkPhSlope();
        } else if (string.length() > 5 && memcmp_P(string.c_str(), F("?CAL,"), 5) == 0) {
          // for example "?CAL,2"
          snprintf_P(calibrationStatus, sizeof(calibrationStatus), PSTR("PH Calibra: %s pt"), string.c_str() + 5);
        }
      }
    }
  }
}

// "pH decreases with increase in temperature. But this does not mean that
//  water becomes more acidic at higher temperatures."
// https://www.westlab.com/blog/2017/11/15/how-does-temperature-affect-ph
void PHProbe::setTemperatureCompensation(float temperature) {
  char buffer[10];
  if (temperature > 0 && temperature < 100) {
    snprintf_P(buffer, sizeof(buffer), (PGM_P)F("T,%i.%02i\r"), (int)temperature, (int)(temperature * 100 + 0.5) % 100);
  } else {
    snprintf_P(buffer, sizeof(buffer), (PGM_P)F("T,20\r"));
  }
  serial(F("PHProbe::setTemperatureCompensation() - %s"), buffer);
  Serial1.print(buffer);  // send that string to the Atlas Scientific product
}

void PHProbe::setHighpointCalibration(float highpoint) {
  slopeIsOutOfRange = false;                          // Clear warnings about current calibration
  EEPROM_TC::instance()->setIgnoreBadPHSlope(false);  // Watch for new bad calibrations
  char buffer[17];
  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Cal,High,%i.%03i\r"), (int)highpoint,
             (int)(highpoint * 1000 + 0.5) % 1000);
  Serial1.print(buffer);  // send that string to the Atlas Scientific product
  calibrationString[0] = '\0';
  serial(F("PHProbe::setHighpointCalibration(%i.%03i)"), (int)highpoint, (int)(highpoint * 1000) % 1000);
}

void PHProbe::setLowpointCalibration(float lowpoint) {
  slopeIsOutOfRange = false;                          // Clear warnings about current calibration
  EEPROM_TC::instance()->setIgnoreBadPHSlope(false);  // Watch for new bad calibrations
  char buffer[16];
  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Cal,low,%i.%03i\r"), (int)lowpoint, (int)(lowpoint * 1000 + 0.5) % 1000);
  Serial1.print(buffer);  // send that string to the Atlas Scientific product
  calibrationString[0] = '\0';
  serial(F("PHProbe::setLowpointCalibration(%i.%03i)"), (int)lowpoint, (int)(lowpoint * 1000) % 1000);
}

void PHProbe::setMidpointCalibration(float midpoint) {
  slopeIsOutOfRange = false;                          // Clear warnings about current calibration
  EEPROM_TC::instance()->setIgnoreBadPHSlope(false);  // Watch for new bad calibrations
  char buffer[16];
  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Cal,mid,%i.%03i\r"), (int)midpoint, (int)(midpoint * 1000 + 0.5) % 1000);
  Serial1.print(buffer);  // send that string to the Atlas Scientific product
  calibrationString[0] = '\0';
  serial(F("PHProbe::setMidpointCalibration(%i.%03i)"), (int)midpoint, (int)(midpoint * 1000) % 1000);
}

/**
 * @brief whether the user should be warned about a bad PH calibration
 *
 * @return true
 * @return false
 */
bool PHProbe::shouldWarnAboutCalibration() {
  return (slopeIsOutOfRange && !EEPROM_TC::instance()->getIgnoreBadPHSlope());
}

#if defined(ARDUINO_CI_COMPILATION_MOCKS)
#include <Arduino.h>

#include "TankController.h"

void PHProbe::getCalibrationStringValue(char* buffer, int size) const {
  strscpy(buffer, calibrationString, size);
}

void PHProbe::sendCalibrationStringSegment(const char* segment) {
  GODMODE()->serialPort[1].dataIn = String(segment);  // the queue of data waiting to be read
  TankController::instance()->serialEvent1();         // fake interrupt to read the segment
  TankController::instance()->loop();                 // update the controls based on the current readings
}

void PHProbe::setCalibration(int calibrationPoints) {
  char buffer[10];
  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("?CAL,%i\r"), calibrationPoints);
  GODMODE()->serialPort[1].dataIn = buffer;    // the queue of data waiting to be read
  TankController::instance()->serialEvent1();  // fake interrupt to update the calibration reading
  TankController::instance()->loop();          // update the controls based on the current readings
}

void PHProbe::setCalibrationString(const char* value) {
  strscpy(calibrationString, value, sizeof(calibrationString));
}

void PHProbe::setPh(float newValue) {
  char buffer[10];
  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("%i.%03i\r"), (int)newValue, (int)(newValue * 1000 + 0.5) % 1000);
  GODMODE()->serialPort[1].dataIn = buffer;    // the queue of data waiting to be read
  TankController::instance()->serialEvent1();  // fake interrupt to update the current pH reading
  TankController::instance()->loop();          // update the controls based on the current readings
}

void PHProbe::setPhSlope(const char* slope) {
  GODMODE()->serialPort[1].dataIn = String(slope);  // the queue of data waiting to be read
  TankController::instance()->serialEvent1();       // fake interrupt to update the current pH reading
  TankController::instance()->loop();               // update the controls based on the current readings
}
#endif
