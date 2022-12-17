#pragma once
#include <Arduino.h>

// Logging intervals (1 sec, 1 min, 5 min)
#define COMPREHENSIVE_LOGGING_INTERVAL 1000
#define SERIAL_LOGGING_INTERVAL 60000
#define SPARSE_LOGGING_INTERVAL 300000

class DataLogger_TC {
public:
  // class methods
  static DataLogger_TC *instance();

  // instance methods
  void loop();

private:
  // class variables
  static DataLogger_TC *_instance;
  float minTemperature;
  float maxTemperature;
  float minPh;
  float maxPh;

  // instance variables
  uint32_t nextComprehensiveDataLogTime = 0;
  // 70 sec delay is from PushingBox (https://github.com/Open-Acidification/TankController/issues/179)
  uint32_t nextSparseDataLogTime = 70000;
  uint32_t nextSerialLogTime = 0;

  // instance method
  void updateExtremes(float currentTemp, float currentPh, bool reset);
  void writeToComprehensiveDataLog();
  void writeToSparseDataLog();
  void writeToSerial();
};
