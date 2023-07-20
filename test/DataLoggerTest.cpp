#include <RTClib.h>
#include <Serial_TC.h>

#include "ArduinoUnitTests.h"
#include "Devices/DateTime_TC.h"
#include "Devices/Serial_TC.h"
#include "TC_util.h"
#include "TankController.h"

unittest(AlertTestOfSerial) {
  // set up serial (copied from SerialTest.cpp)
  GodmodeState* state = GODMODE();
  DateTime_TC::now();                 // this puts stuff on the serial port that we want to ignore
  serial(F("foo"));                   // "Unable to create directory"
  state->serialPort[0].dataIn = "";   // the queue of data waiting to be read
  state->serialPort[0].dataOut = "";  // the history of data written

  // send an alert
  alert(F("Tank %s temperature is %.1f above setpoint"), "4", 2.21);
  assertEqual("", state->serialPort[0].dataIn);
  assertEqual("ALERT: Tank 4 temperature is 2.2 above setpoint\r\n", state->serialPort[0].dataOut);
}

unittest_main()
