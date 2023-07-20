#include <RTClib.h>
#include <Serial_TC.h>

#include "ArduinoUnitTests.h"
#include "Devices/DataLogger_TC.h"
#include "Devices/DateTime_TC.h"
#include "Devices/Serial_TC.h"
#include "TC_util.h"
#include "TankController.h"

unittest(AlertTest) {
  DateTime_TC::now();  // this puts stuff on the serial port that we want to ignore
  serial(F("foo"));    // "Unable to create directory"

  // send an alert
  alert(F("Tank %s temperature is %.1f above setpoint"), "4", 2.21);
  assertEqual("ALERT: Tank 4 temperature is 2.2 above setpoint", Serial_TC::instance()->buffer);
}

unittest_main()
