#pragma once

#include "driver/gpio.h"

namespace sensorRf::boardConfig {

// Heltec WiFi LoRa 32 V4, ESP32-S3R2 hardware assignment.
inline constexpr gpio_num_t canTxPin = GPIO_NUM_6;
inline constexpr gpio_num_t canRxPin = GPIO_NUM_4;

inline constexpr gpio_num_t gnssPowerControlPin = GPIO_NUM_34;
inline constexpr gpio_num_t statusLedPin = GPIO_NUM_35;
inline constexpr gpio_num_t externalPowerControlPin = GPIO_NUM_36;
inline constexpr gpio_num_t adcControlPin = GPIO_NUM_37;
inline constexpr gpio_num_t gnssResetPin = GPIO_NUM_42;

static_assert(canTxPin != canRxPin);

} // namespace sensorRf::boardConfig
