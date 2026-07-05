#pragma once

#include <Arduino.h>

struct SensorPodData {
  bool cvt_temperature_valid = false;
  float cvt_temperature_f = 0.0f;

  bool aux_temperature_valid = false;
  float aux_temperature_f = 0.0f;

  bool wheel_speed_valid = false;
  float wheel_speed_mph = 0.0f;
  float wheel_rpm = 0.0f;

  bool roll_valid = false;
  float roll_deg = 0.0f;

  bool pitch_valid = false;
  float pitch_deg = 0.0f;
};

void sensorPodDisplayInit();
void sensorPodDisplaySetData(const SensorPodData &data, uint32_t now_ms);
void sensorPodDisplayMarkMissing();
void sensorPodDisplayRefresh(uint32_t now_ms);
bool sensorPodDisplayTransitionActive();
