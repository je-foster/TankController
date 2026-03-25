#include <Arduino.h>
#include <ArduinoUnitTests.h>

#include <string>
#include <vector>

#include "DataLogger.h"
#include "EEPROM_TC.h"
#include "PHProbe.h"
#include "Serial_TC.h"
#include "TC_util.h"
#include "TankController.h"

EEPROM_TC* eeprom = EEPROM_TC::instance();
PHProbe* pHProbe = PHProbe::instance();
TankController* tc = TankController::instance();

unittest(singleton) {
  PHProbe* singleton1 = PHProbe::instance();
  PHProbe* singleton2 = PHProbe::instance();
  assertEqual(singleton1, singleton2);
}

unittest(constructor) {
  assertEqual("*OK,0\rC,1\rSLOPE,?\r", GODMODE()->serialPort[1].dataOut);
}

// tests getPh() and getSlopeResponse as well
unittest(serialEvent1) {
  tc->loop();  // Writes something to EEPROM, triggering a DataLogger warning
  tc->loop();  // DataLogger writes to SD card
  DataLogger* dl = DataLogger::instance();
  dl->reset();
  GodmodeState* state = GODMODE();
  state->reset();
  assertEqual("", state->serialPort[1].dataOut);
  tc->serialEvent1();  // fake interrupt
  assertEqual("", pHProbe->getCalibrationStatus());
  assertEqual(0, pHProbe->getPh());
  assertEqual("Requesting...", pHProbe->getSlopeResponse());
  pHProbe->setCalibration(2);
  pHProbe->setPh(7.125);
  assertFalse(dl->getShouldWriteWarning());
  assertEqual("", dl->getBuffer());
  // Next line calls serialEvent1 (triggering warning) and tc->loop() (sending warning)
  pHProbe->setPhSlope();
  assertFalse(dl->getShouldWriteWarning());  // already false again
  string lastWrittenString(dl->getBuffer());
  assertTrue(lastWrittenString.find("99.7,100.3,-0.89") > 0);  // warning was sent
  assertEqual("PH Calibra: 2 pt", pHProbe->getCalibrationStatus());
  assertEqual(7.125, pHProbe->getPh());
  assertEqual("99.7,100.3,-0.89", pHProbe->getSlopeResponse());
}

unittest(serialEvent1CatchBadSlope) {
  eeprom->setIgnoreBadPHSlope(true);
  assertTrue(eeprom->getIgnoreBadPHSlope());
  tc->serialEvent1();
  assertFalse(pHProbe->slopeIsBad());
  assertFalse(pHProbe->shouldWarnAboutCalibration());

  // Bad slope
  pHProbe->setPhSlope("?SLOPE,-2.7,101.3\r");
  assertTrue(pHProbe->slopeIsBad());
  assertTrue(eeprom->getIgnoreBadPHSlope());
  assertFalse(pHProbe->shouldWarnAboutCalibration());

  // Good slope
  pHProbe->setPhSlope();
  assertFalse(pHProbe->slopeIsBad());
  assertFalse(eeprom->getIgnoreBadPHSlope());
  assertFalse(pHProbe->shouldWarnAboutCalibration());

  // Bad slope
  pHProbe->setPhSlope("?SLOPE,98.7,107.2,-0.89\r");
  assertTrue(pHProbe->slopeIsBad());
}

unittest(clearCalibration) {
  GodmodeState* state = GODMODE();
  state->reset();
  eeprom->setIgnoreBadPHSlope(true);
  pHProbe->setCalibrationString();
  char buffer[121];

  assertTrue(eeprom->getIgnoreBadPHSlope());
  assertEqual("", state->serialPort[1].dataOut);
  pHProbe->clearCalibration();
  assertEqual("Cal,clear\r", state->serialPort[1].dataOut);
  assertFalse(eeprom->getIgnoreBadPHSlope());
  assertFalse(pHProbe->slopeIsBad());
  // Check that the calibration string was erased
  pHProbe->getCalibrationStringValue(buffer, sizeof(buffer));
  assertEqual("", buffer);
}

unittest(clearBadCalibration) {
  GodmodeState* state = GODMODE();
  state->reset();
  pHProbe->setCalibrationString();
  char buffer[121];
  eeprom->setIgnoreBadPHSlope(true);
  assertTrue(eeprom->getIgnoreBadPHSlope());
  pHProbe->setPhSlope("?SLOPE,99.7,110.4,-0.89\r");
  assertTrue(pHProbe->slopeIsBad());
  pHProbe->clearCalibration();
  assertFalse(eeprom->getIgnoreBadPHSlope());
  assertFalse(pHProbe->slopeIsBad());
  // Check that the calibration string was erased
  pHProbe->getCalibrationStringValue(buffer, sizeof(buffer));
  assertEqual("", buffer);
}

unittest(sendCalibrationRequest) {
  GodmodeState* state = GODMODE();
  state->reset();
  assertEqual("", state->serialPort[1].dataOut);
  pHProbe->sendCalibrationRequest();
  assertEqual("CAL,?\r", state->serialPort[1].dataOut);
  assertEqual("PH Calibration", pHProbe->getCalibrationStatus());
}

unittest(getCalibration) {
  GodmodeState* state = GODMODE();
  state->reset();
  assertEqual("", state->serialPort[1].dataOut);
  char buffer[17];
  pHProbe->setCalibration(0);
  pHProbe->getCalibrationStatus(buffer, sizeof(buffer));
  assertEqual("PH Calibra: 0 pt", buffer);
  pHProbe->setCalibration(3);
  pHProbe->getCalibrationStatus(buffer, sizeof(buffer));
  assertEqual("PH Calibra: 3 pt", buffer);
}

unittest(receiveCalibrationString) {
  std::vector<std::string> segments = {"596F75206172\r", "65206120636F\r", "333333333333\r", "444444444444\r",
                                       "555555555555\r", "666666666666\r", "777777777777\r", "888888888888\r",
                                       "999999999999\r", "6F6C20677579\r"};

  GodmodeState* state = GODMODE();
  state->reset();
  assertEqual("", state->serialPort[1].dataOut);
  char buffer[121];

  assertFalse(pHProbe->getReceivingCalibrationString());
  // Asking for the calibration string will trigger a request to the EZO probe
  pHProbe->getCalibrationString(buffer, sizeof(buffer));
  assertEqual("Requesting...", buffer);
  assertEqual("C,0\rExport\r", state->serialPort[1].dataOut);  // continuous measurement stopped
  state->serialPort[1].dataOut = "";
  bool stillReceiving = pHProbe->getReceivingCalibrationString();
  // Simulate date being sent from the EZO probe
  for (int i = 0; i < 10; i++) {
    pHProbe->sendCalibrationStringSegment(segments[i].c_str());
    stillReceiving = stillReceiving && pHProbe->getReceivingCalibrationString();
    assertEqual("Export\r", state->serialPort[1].dataOut);
    state->serialPort[1].dataOut = "";
  }
  assertTrue(stillReceiving);  // stayed in receiving state for whole loop
  pHProbe->getCalibrationString(buffer, sizeof(buffer));
  assertEqual("Requesting...", buffer);  // calibration string is presumed to be incomplete
  assertTrue(pHProbe->getReceivingCalibrationString());
  pHProbe->sendCalibrationStringSegment("*DONE");  // EZO probe says it is done
  assertFalse(pHProbe->getReceivingCalibrationString());
  assertEqual("C,1\r", state->serialPort[1].dataOut);  // continuous measurement resumed
  pHProbe->getCalibrationString(buffer, sizeof(buffer));
  assertEqual(
      "596F7520617265206120636F3333333333334444444444445555555555556666666666667777777777778888888888889999999999996F6C"
      "20677579",
      buffer);
}

unittest(setTemperatureCompensation) {
  GodmodeState* state = GODMODE();
  state->reset();
  assertEqual("", state->serialPort[1].dataOut);
  pHProbe->setTemperatureCompensation(30.25);
  assertEqual("T,30.25\r", state->serialPort[1].dataOut);
  state->serialPort[1].dataOut = "";
  pHProbe->setTemperatureCompensation(100.25);
  assertEqual("T,20\r", state->serialPort[1].dataOut);
  state->serialPort[1].dataOut = "";
  pHProbe->setTemperatureCompensation(-1.25);
  assertEqual("T,20\r", state->serialPort[1].dataOut);
}

unittest(setLowpointCalibration) {
  GodmodeState* state = GODMODE();
  state->reset();
  pHProbe->setCalibrationString();
  char buffer[121];
  // TODO: the following two lines are commented out in another branch
  eeprom->setIgnoreBadPHSlope(true);
  assertTrue(eeprom->getIgnoreBadPHSlope());
  assertEqual("", state->serialPort[1].dataOut);
  pHProbe->setLowpointCalibration(10.875);
  assertEqual("Cal,low,10.875\r", state->serialPort[1].dataOut);
  // TODO: the following line is commented out in another branch
  assertFalse(eeprom->getIgnoreBadPHSlope());
  // Check that the calibration string was erased
  pHProbe->getCalibrationStringValue(buffer, sizeof(buffer));
  assertEqual("", buffer);
}

unittest(setMidpointCalibration) {
  GodmodeState* state = GODMODE();
  state->reset();
  pHProbe->setCalibrationString();
  char buffer[121];
  DataLogger::instance()->reset();
  assertFalse(DataLogger::instance()->getShouldWriteWarning());
  eeprom->setIgnoreBadPHSlope(true);
  assertTrue(eeprom->getIgnoreBadPHSlope());
  assertTrue(DataLogger::instance()->getShouldWriteWarning());
  DataLogger::instance()->reset();
  assertFalse(DataLogger::instance()->getShouldWriteWarning());
  assertEqual("", state->serialPort[1].dataOut);
  pHProbe->setMidpointCalibration(11.875);
  assertTrue(DataLogger::instance()->getShouldWriteWarning());
  assertEqual("Cal,mid,11.875\r", state->serialPort[1].dataOut);
  assertFalse(eeprom->getIgnoreBadPHSlope());
  // Check that the calibration string was erased
  pHProbe->getCalibrationStringValue(buffer, sizeof(buffer));
  assertEqual("", buffer);
}

unittest(settingMidpointClearsBadCalibration) {
  GodmodeState* state = GODMODE();
  state->reset();
  pHProbe->setCalibrationString();
  char buffer[121];
  eeprom->setIgnoreBadPHSlope(true);
  assertTrue(eeprom->getIgnoreBadPHSlope());
  pHProbe->setPhSlope("?SLOPE,-2.7,100.0,-0.50\r");
  assertTrue(pHProbe->slopeIsBad());
  pHProbe->setMidpointCalibration(11.875);
  assertFalse(eeprom->getIgnoreBadPHSlope());
  assertFalse(pHProbe->slopeIsBad());
  // Check that the calibration string was erased
  pHProbe->getCalibrationStringValue(buffer, sizeof(buffer));
  assertEqual("", buffer);
}

unittest(setHighpointCalibration) {
  GodmodeState* state = GODMODE();
  state->reset();
  pHProbe->setCalibrationString();
  char buffer[121];
  // TODO: the following two lines are commented out in another branch
  eeprom->setIgnoreBadPHSlope(true);
  assertTrue(eeprom->getIgnoreBadPHSlope());
  assertEqual("", state->serialPort[1].dataOut);
  pHProbe->setHighpointCalibration(12.875);
  assertEqual("Cal,High,12.875\r", state->serialPort[1].dataOut);
  // TODO: the following line is commented out in another branch
  assertFalse(eeprom->getIgnoreBadPHSlope());
  // Check that the calibration string was erased
  pHProbe->getCalibrationStringValue(buffer, sizeof(buffer));
  assertEqual("", buffer);
}

unittest(sendSlopeRequest) {
  GodmodeState* state = GODMODE();
  state->reset();
  assertEqual("", state->serialPort[1].dataOut);
  pHProbe->sendSlopeRequest();
  assertEqual("SLOPE,?\r", state->serialPort[1].dataOut);
  assertEqual("Requesting...", pHProbe->getSlopeResponse());
}

// this test assumes that earlier tests have run and that there is a slope available
unittest(getSlope) {
  GodmodeState* state = GODMODE();
  state->reset();
  pHProbe->setPhSlope();
  char buffer[20];
  pHProbe->getSlope(buffer, sizeof(buffer));
  assertEqual("99.7,100.3,-0.89", buffer);
  pHProbe->setPhSlope("?SLOPE,98.7,101.3,-0.89\r");
  pHProbe->getSlope(buffer, sizeof(buffer));
  assertEqual("98.7,101.3,-0.89", buffer);
}

unittest(getPh) {
  GodmodeState* state = GODMODE();
  state->reset();
  pHProbe->setPh(7.25);
  float pH = pHProbe->getPh();
  assertEqual(7.25, pH);
}

unittest_main()
