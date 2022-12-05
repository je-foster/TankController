#pragma once
#include <Arduino.h>

// Logging intervals in milliseconds
#define INCESSANT_LOGGING_INTERVAL 1000l
#define INTERMITTENT_LOGGING_INTERVAL 1000l * 60l * 5l
#define SERIAL_LOGGING_INTERVAL 1000l * 60l

class DataLogger_TC {
public:
  // class methods
  static DataLogger_TC *instance();

  // instance methods
  void loop();

private:
  // class variables
  static DataLogger_TC *_instance;

  // instance variables
  uint32_t nextIncessantLogTime = 0;
  // 70 sec delay is from PushingBox (https://github.com/Open-Acidification/TankController/issues/179)
  uint32_t nextIntermittentLogTime = 70000;
  uint32_t nextSerialLogTime = 0;

  // instance method
  void writeToSDIncessantly();
  void writeToSDIntermittently();
  void writeToSerial();
}
