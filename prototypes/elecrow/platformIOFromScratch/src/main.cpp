#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <cstring>
#include <cstdlib>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "LovyanGFX_Driver.h"
#include "pins_config.h"
#include "sensor_pod_display.h"
#include "ui.h"

namespace {

constexpr uint8_t kPanelControllerAddress = 0x30;
constexpr uint8_t kTouchControllerAddress = 0x5D;
constexpr int kI2cSdaPin = 15;
constexpr int kI2cSclPin = 16;
constexpr int kBacklightPin = 2;
constexpr uint32_t kSerialBaud = 115200;
constexpr size_t kSerialFrameMax = 128;
constexpr uint8_t kBacklightOnBrightness = 255;
constexpr uint32_t kStartupBacklightHoldMs = 600;
constexpr uint32_t kTransitionBacklightHoldMs = 400;

LGFX gfx;

lv_disp_draw_buf_t draw_buf;
lv_color_t *draw_buf_0 = nullptr;
lv_color_t *draw_buf_1 = nullptr;

bool display_flush_active = false;
char serial_frame[kSerialFrameMax];
size_t serial_frame_len = 0;
bool backlight_enabled = false;
uint32_t backlight_off_until_ms = 0;

bool i2cScanForAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void sendI2CCommand(uint8_t command) {
  Wire.beginTransmission(kPanelControllerAddress);
  Wire.write(command);
  const uint8_t error = Wire.endTransmission();

  Serial.print("I2C command 0x");
  Serial.print(command, HEX);
  if (error == 0) {
    Serial.println(" sent");
  } else {
    Serial.print(" failed, error ");
    Serial.println(error);
  }
}

void wakePanelController() {
  for (uint8_t attempt = 0; attempt < 20; ++attempt) {
    const bool panel_controller_found = i2cScanForAddress(kPanelControllerAddress);
    const bool touch_controller_found = i2cScanForAddress(kTouchControllerAddress);

    if (panel_controller_found && touch_controller_found) {
      Serial.println("Panel controller and GT911 touch controller detected");
      sendI2CCommand(0x10);
      return;
    }

    Serial.println("Waiting for panel/touch controllers...");
    sendI2CCommand(0x19);
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delay(120);
    pinMode(1, INPUT);
    delay(100);
  }

  Serial.println("Panel/touch controller detection timed out; continuing init");
}

void displayFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;

  display_flush_active = true;
  if (gfx.getStartCount() == 0) {
    gfx.startWrite();
  }

  gfx.pushImage(area->x1, area->y1, width, height, reinterpret_cast<lgfx::rgb565_t *>(&color_p->full));

  display_flush_active = false;
  lv_disp_flush_ready(disp);
}

void setBacklight(bool enabled) {
  if (enabled == backlight_enabled) {
    return;
  }

  gfx.setBrightness(enabled ? kBacklightOnBrightness : 0);
  backlight_enabled = enabled;
}

void holdBacklightOff(uint32_t now_ms, uint32_t hold_ms) {
  const uint32_t hold_until_ms = now_ms + hold_ms;
  if (static_cast<int32_t>(hold_until_ms - backlight_off_until_ms) > 0) {
    backlight_off_until_ms = hold_until_ms;
  }
  setBacklight(false);
}

void updateBacklight(uint32_t now_ms) {
  if (static_cast<int32_t>(now_ms - backlight_off_until_ms) < 0) {
    setBacklight(false);
  } else {
    setBacklight(true);
  }
}

void initLvgl() {
  lv_init();

  const size_t buffer_pixels = LCD_H_RES * 80;
  const size_t buffer_size = buffer_pixels * sizeof(lv_color_t);

  draw_buf_0 = static_cast<lv_color_t *>(heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  draw_buf_1 = static_cast<lv_color_t *>(heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (draw_buf_0 == nullptr || draw_buf_1 == nullptr) {
    Serial.println("Failed to allocate LVGL draw buffers in PSRAM");
    while (true) {
      delay(1000);
    }
  }

  Serial.printf("LVGL draw_buf_0 = %p\n", draw_buf_0);
  Serial.printf("LVGL draw_buf_1 = %p\n", draw_buf_1);

  lv_disp_draw_buf_init(&draw_buf, draw_buf_0, draw_buf_1, buffer_pixels);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = displayFlush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
}

void initPlaceholderUi() {
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, "dashOne");
  lv_obj_set_style_text_color(label, lv_color_hex(0xE8F1F2), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_40, LV_PART_MAIN);
  lv_obj_center(label);

  lv_disp_load_scr(screen);
}

bool parseSensorValue(char *token, float &value, bool &valid) {
  if (token == nullptr || token[0] == '\0') {
    return false;
  }

  if (token[0] == '-' && token[1] == '\0') {
    value = 0.0f;
    valid = false;
    return true;
  }

  char *end = nullptr;
  value = strtof(token, &end);
  if (end == token || *end != '\0') {
    return false;
  }

  valid = true;
  return true;
}

void parseSensorPodFrame(char *frame, uint32_t now_ms) {
  char *saveptr = nullptr;
  char *token = strtok_r(frame, ",", &saveptr);
  if (token == nullptr || strcmp(token, "SPOD") != 0) {
    return;
  }

  SensorPodData data;
  bool rpm_valid = false;

  token = strtok_r(nullptr, ",", &saveptr);
  if (!parseSensorValue(token, data.cvt_temperature_f, data.cvt_temperature_valid)) {
    return;
  }

  token = strtok_r(nullptr, ",", &saveptr);
  if (!parseSensorValue(token, data.aux_temperature_f, data.aux_temperature_valid)) {
    return;
  }

  token = strtok_r(nullptr, ",", &saveptr);
  if (!parseSensorValue(token, data.wheel_speed_mph, data.wheel_speed_valid)) {
    return;
  }

  token = strtok_r(nullptr, ",", &saveptr);
  if (!parseSensorValue(token, data.wheel_rpm, rpm_valid)) {
    return;
  }
  if (!rpm_valid) {
    data.wheel_rpm = 0.0f;
  }

  token = strtok_r(nullptr, ",", &saveptr);
  if (!parseSensorValue(token, data.roll_deg, data.roll_valid)) {
    return;
  }

  token = strtok_r(nullptr, ",", &saveptr);
  if (!parseSensorValue(token, data.pitch_deg, data.pitch_valid)) {
    return;
  }

  sensorPodDisplaySetData(data, now_ms);
}

void handleSerialSensorInput(uint32_t now_ms) {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      serial_frame[serial_frame_len] = '\0';
      parseSensorPodFrame(serial_frame, now_ms);
      serial_frame_len = 0;
      continue;
    }

    if (serial_frame_len < kSerialFrameMax - 1) {
      serial_frame[serial_frame_len++] = incoming;
    } else {
      serial_frame_len = 0;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(100);

  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, LOW);

  Wire.begin(kI2cSdaPin, kI2cSclPin);
  delay(50);
  wakePanelController();

  gfx.init();
  gfx.setBrightness(0);
  gfx.initDMA();
  gfx.fillScreen(TFT_BLACK);
  gfx.startWrite();

  initLvgl();
  //initPlaceholderUi();
  ui_init();
  sensorPodDisplayInit();
  holdBacklightOff(millis(), kStartupBacklightHoldMs);
  lv_timer_handler();
  lv_refr_now(nullptr);

  Serial.println("Setup done");
}

void loop() {
  static uint32_t last_tick_ms = millis();
  const uint32_t now_ms = millis();
  lv_tick_inc(now_ms - last_tick_ms);
  last_tick_ms = now_ms;

  handleSerialSensorInput(now_ms);
  const bool transition_was_active = sensorPodDisplayTransitionActive();
  sensorPodDisplayRefresh(now_ms);
  const bool transition_needs_mask = transition_was_active || sensorPodDisplayTransitionActive();
  if (transition_needs_mask) {
    holdBacklightOff(now_ms, kTransitionBacklightHoldMs);
  }

  lv_timer_handler();
  if (transition_needs_mask) {
    lv_refr_now(nullptr);
  }

  const uint32_t after_lvgl_ms = millis();
  if (transition_needs_mask) {
    holdBacklightOff(after_lvgl_ms, kTransitionBacklightHoldMs);
  }
  updateBacklight(after_lvgl_ms);
  delay(5);
}
