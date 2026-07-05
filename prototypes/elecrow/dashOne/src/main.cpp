#include "pins_config.h"
#include "LovyanGFX_Driver.h"

#include <Arduino.h>
#include <lvgl.h>
#include <Wire.h>
#include <SPI.h>

#include <stdbool.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#include "ui/ui.h"

LGFX gfx;

/* Change to your screen resolution */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
static lv_color_t *buf1;

uint16_t touch_x, touch_y;

static constexpr uint32_t kDisplayUpdateMs = 33;
static constexpr uint32_t kSensorPodTimeoutMs = 500;
static constexpr uint32_t kRfPodTimeoutMs = 1500;
static constexpr size_t kSerialLineMax = 128;
static constexpr int16_t kSpeedNeedleMinAngle = -1250;
static constexpr int16_t kSpeedNeedleMaxAngle = 1250;
static constexpr int16_t kSpeedNeedleMaxMph = 60;
static constexpr int16_t kAttitudeMaxDegrees = 30;
static constexpr int16_t kAttitudeMaxAngle = 300;
static constexpr int16_t kTempMaxF = 250;
static constexpr uint32_t kColorTextActive = 0xE8F1F2;
static constexpr uint32_t kColorMuted = 0x6C858D;
static constexpr uint32_t kColorDisabled = 0x3A454A;
static constexpr uint32_t kColorTempActive = 0xCE0A0A;
#if ARDUINO_USB_CDC_ON_BOOT
#if ARDUINO_USB_MODE
static constexpr const char *kSerialModeName = "HWCDC";
#else
static constexpr const char *kSerialModeName = "USBCDC";
#endif
#else
static constexpr const char *kSerialModeName = "UART0";
#endif

struct OptionalFloat {
  bool present = false;
  float value = 0.0f;
};

struct SensorPodSnapshot {
  OptionalFloat cvt_temp_f;
  OptionalFloat aux_temp_f;
  OptionalFloat wheel_speed_mph;
  OptionalFloat wheel_rpm;
  OptionalFloat roll_deg;
  OptionalFloat pitch_deg;
  uint32_t last_update_ms = 0;
  bool has_frame = false;
};

struct RfPodSnapshot {
  OptionalFloat gps_speed_mph;
  OptionalFloat latitude;
  OptionalFloat longitude;
  OptionalFloat gps_fix;
  OptionalFloat lap_current_ms;
  OptionalFloat lap_last_ms;
  OptionalFloat lap_best_ms;
  bool current_time_present = false;
  char current_time[9] = "--:--:--";
  uint32_t last_update_ms = 0;
  bool has_frame = false;
};

SensorPodSnapshot sensor_pod;
RfPodSnapshot rf_pod;
uint32_t serial_bytes_seen = 0;
uint32_t serial0_bytes_seen = 0;
uint32_t spod_frames_seen = 0;
uint32_t rfod_frames_seen = 0;
uint32_t parse_errors = 0;
uint32_t display_snapshot_count = 0;
bool sensor_pod_display_enabled = false;
bool rf_pod_display_enabled = false;
char serial_line[kSerialLineMax];
size_t serial_line_length = 0;

int16_t clampInt16(int16_t value, int16_t min_value, int16_t max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

int16_t speedToNeedleAngle(float speed_mph) {
  const int16_t clamped_speed = clampInt16(lroundf(speed_mph), 0, kSpeedNeedleMaxMph);
  const int32_t angle_span = kSpeedNeedleMaxAngle - kSpeedNeedleMinAngle;
  return kSpeedNeedleMinAngle + ((angle_span * clamped_speed) / kSpeedNeedleMaxMph);
}

int16_t attitudeToNeedleAngle(float angle_degrees) {
  const int16_t clamped_angle = clampInt16(lroundf(angle_degrees), -kAttitudeMaxDegrees, kAttitudeMaxDegrees);
  return (kAttitudeMaxAngle * clamped_angle) / kAttitudeMaxDegrees;
}

int16_t temperatureToBarValue(float temperature_f) {
  return clampInt16(lroundf(temperature_f), 0, kTempMaxF);
}

bool parseOptionalFloat(const char *text, OptionalFloat *value) {
  if (text == nullptr || value == nullptr) {
    return false;
  }

  if (strcmp(text, "-") == 0) {
    value->present = false;
    value->value = 0.0f;
    return true;
  }

  char *end = nullptr;
  const float parsed = strtof(text, &end);
  if (end == text || *end != '\0') {
    return false;
  }

  value->present = true;
  value->value = parsed;
  return true;
}

bool parseOptionalTimeText(const char *text, char *value, size_t value_size, bool *present) {
  if (text == nullptr || value == nullptr || value_size < 2 || present == nullptr) {
    return false;
  }

  if (strcmp(text, "-") == 0) {
    *present = false;
    value[0] = '-';
    value[1] = '\0';
    return true;
  }

  if (strlen(text) != 8 || text[2] != ':' || text[5] != ':') {
    return false;
  }

  for (uint8_t i = 0; i < 8; i++) {
    if ((i == 2) || (i == 5)) {
      continue;
    }
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
  }

  strlcpy(value, text, value_size);
  *present = true;
  return true;
}

bool parseSensorPodFrame(char *line) {
  SensorPodSnapshot parsed;
  char *context = nullptr;
  char *token = strtok_r(line, ",", &context);

  if (token == nullptr || strcmp(token, "SPOD") != 0) {
    return false;
  }

  OptionalFloat *fields[] = {
      &parsed.cvt_temp_f,
      &parsed.aux_temp_f,
      &parsed.wheel_speed_mph,
      &parsed.wheel_rpm,
      &parsed.roll_deg,
      &parsed.pitch_deg,
  };

  for (OptionalFloat *field : fields) {
    token = strtok_r(nullptr, ",", &context);
    if (!parseOptionalFloat(token, field)) {
      return false;
    }
  }

  if (strtok_r(nullptr, ",", &context) != nullptr) {
    return false;
  }

  parsed.last_update_ms = millis();
  parsed.has_frame = true;
  sensor_pod = parsed;
  spod_frames_seen++;
  return true;
}

bool parseRfPodFrame(char *line) {
  RfPodSnapshot parsed;
  char *context = nullptr;
  char *token = strtok_r(line, ",", &context);

  if (token == nullptr || strcmp(token, "RFOD") != 0) {
    return false;
  }

  OptionalFloat *fields[] = {
      &parsed.gps_speed_mph,
      &parsed.latitude,
      &parsed.longitude,
      &parsed.gps_fix,
      &parsed.lap_current_ms,
      &parsed.lap_last_ms,
      &parsed.lap_best_ms,
  };

  for (OptionalFloat *field : fields) {
    token = strtok_r(nullptr, ",", &context);
    if (!parseOptionalFloat(token, field)) {
      return false;
    }
  }

  token = strtok_r(nullptr, ",", &context);
  if (!parseOptionalTimeText(token, parsed.current_time, sizeof(parsed.current_time), &parsed.current_time_present)) {
    return false;
  }

  if (strtok_r(nullptr, ",", &context) != nullptr) {
    return false;
  }

  parsed.last_update_ms = millis();
  parsed.has_frame = true;
  rf_pod = parsed;
  rfod_frames_seen++;
  return true;
}

bool parseSerialFrame(char *line) {
  if (strncmp(line, "SPOD,", 5) == 0) {
    return parseSensorPodFrame(line);
  }
  if (strncmp(line, "RFOD,", 5) == 0) {
    return parseRfPodFrame(line);
  }
  return false;
}

void processSerialByte(char c) {
  if (c == '\r') {
    return;
  }

  if (c == '\n') {
    serial_line[serial_line_length] = '\0';
    if (serial_line_length > 0) {
      if (!parseSerialFrame(serial_line)) {
        parse_errors++;
      }
    }
    serial_line_length = 0;
    return;
  }

  if (serial_line_length < (kSerialLineMax - 1)) {
    serial_line[serial_line_length++] = c;
  } else {
    serial_line_length = 0;
  }
}
void processSerialInput() {
  while (Serial.available() > 0) {
    serial_bytes_seen++;
    processSerialByte(static_cast<char>(Serial.read()));
  }

  while (Serial0.available() > 0) {
    serial0_bytes_seen++;
    processSerialByte(static_cast<char>(Serial0.read()));
  }
}

void setLabelEnabled(lv_obj_t *label, bool enabled) {
  if (label == nullptr) {
    return;
  }

  lv_obj_set_style_text_color(label, lv_color_hex(enabled ? kColorTextActive : kColorDisabled), LV_PART_MAIN | LV_STATE_DEFAULT);
  if (enabled) {
    lv_obj_clear_state(label, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(label, LV_STATE_DISABLED);
  }
}

void setImageEnabled(lv_obj_t *image, bool enabled) {
  if (image == nullptr) {
    return;
  }

  lv_obj_set_style_img_recolor(image, lv_color_hex(enabled ? 0x000000 : kColorDisabled), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(image, enabled ? 0 : 180, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_opa(image, enabled ? 255 : 120, LV_PART_MAIN | LV_STATE_DEFAULT);
  if (enabled) {
    lv_obj_clear_state(image, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(image, LV_STATE_DISABLED);
  }
}

void setBarEnabled(lv_obj_t *bar, bool enabled) {
  if (bar == nullptr) {
    return;
  }

  lv_obj_set_style_bg_color(bar, lv_color_hex(enabled ? kColorTempActive : kColorDisabled), LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(bar, enabled ? 255 : 120, LV_PART_INDICATOR | LV_STATE_DEFAULT);
  if (enabled) {
    lv_obj_clear_state(bar, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(bar, LV_STATE_DISABLED);
  }
}

void setRfStatusLabel(const RfPodSnapshot &snapshot) {
  if (ui_statusRF == nullptr) {
    return;
  }

  char text[64];
  if (rfod_frames_seen == 0) {
    snprintf(text, sizeof(text), "RF:no data S:%lu R:%lu u0:%lu", static_cast<unsigned long>(spod_frames_seen), static_cast<unsigned long>(rfod_frames_seen), static_cast<unsigned long>(serial0_bytes_seen));
    lv_obj_set_style_text_color(ui_statusRF, lv_color_hex(kColorDisabled), LV_PART_MAIN | LV_STATE_DEFAULT);
  } else if ((millis() - snapshot.last_update_ms) > kRfPodTimeoutMs) {
    snprintf(text, sizeof(text), "RF:stale R:%lu e:%lu", static_cast<unsigned long>(rfod_frames_seen), static_cast<unsigned long>(parse_errors));
    lv_obj_set_style_text_color(ui_statusRF, lv_color_hex(kColorMuted), LV_PART_MAIN | LV_STATE_DEFAULT);
  } else {
    snprintf(text, sizeof(text), "RF:live R:%lu e:%lu", static_cast<unsigned long>(rfod_frames_seen), static_cast<unsigned long>(parse_errors));
    lv_obj_set_style_text_color(ui_statusRF, lv_color_hex(kColorTextActive), LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  lv_label_set_text(ui_statusRF, text);
}

void setGpsStatusLabel(const RfPodSnapshot &snapshot, bool rf_data_live) {
  if (ui_statusGPS == nullptr) {
    return;
  }

  if (!rf_data_live) {
    lv_label_set_text(ui_statusGPS, rfod_frames_seen == 0 ? "GPS: no data" : "GPS: stale");
    lv_obj_set_style_text_color(ui_statusGPS, lv_color_hex(kColorDisabled), LV_PART_MAIN | LV_STATE_DEFAULT);
    return;
  }

  if (!snapshot.gps_fix.present) {
    lv_label_set_text(ui_statusGPS, "GPS: -");
    lv_obj_set_style_text_color(ui_statusGPS, lv_color_hex(kColorMuted), LV_PART_MAIN | LV_STATE_DEFAULT);
    return;
  }

  const bool has_fix = snapshot.gps_fix.value >= 0.5f;
  lv_label_set_text(ui_statusGPS, has_fix ? "GPS: fix" : "GPS: no fix");
  lv_obj_set_style_text_color(ui_statusGPS, lv_color_hex(has_fix ? kColorTextActive : kColorMuted), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void setIntegerLabel(lv_obj_t *label, const OptionalFloat &value) {
  if (label == nullptr) {
    return;
  }

  if (!value.present) {
    lv_label_set_text(label, "-");
    setLabelEnabled(label, false);
    return;
  }

  char text[8];
  snprintf(text, sizeof(text), "%d", static_cast<int>(lroundf(value.value)));
  lv_label_set_text(label, text);
  setLabelEnabled(label, true);
}

void setSignedTenthsLabel(lv_obj_t *label, const OptionalFloat &value) {
  if (label == nullptr) {
    return;
  }

  if (!value.present) {
    lv_label_set_text(label, "-");
    setLabelEnabled(label, false);
    return;
  }

  const int16_t tenths = lroundf(value.value * 10.0f);
  const char sign = (tenths < 0) ? '-' : '+';
  const int16_t magnitude = abs(tenths);
  char text[8];
  snprintf(text, sizeof(text), "%c%d.%d", sign, magnitude / 10, magnitude % 10);
  lv_label_set_text(label, text);
  setLabelEnabled(label, true);
}

void setLapTimeLabel(lv_obj_t *label, const OptionalFloat &value) {
  if (label == nullptr) {
    return;
  }

  if (!value.present) {
    lv_label_set_text(label, "-");
    setLabelEnabled(label, false);
    return;
  }

  long rounded_centiseconds = lroundf(value.value / 10.0f);
  if (rounded_centiseconds < 0) {
    rounded_centiseconds = 0;
  }
  const uint32_t total_centiseconds = static_cast<uint32_t>(rounded_centiseconds);
  const uint32_t minutes = total_centiseconds / 6000UL;
  const uint32_t seconds = (total_centiseconds / 100UL) % 60UL;
  const uint32_t centiseconds = total_centiseconds % 100UL;
  char text[12];
  snprintf(text, sizeof(text), "%lu:%02lu.%02lu", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds), static_cast<unsigned long>(centiseconds));
  lv_label_set_text(label, text);
  setLabelEnabled(label, true);
}

void setCurrentTimeLabel(lv_obj_t *label, const RfPodSnapshot &snapshot) {
  if (label == nullptr) {
    return;
  }

  if (!snapshot.current_time_present) {
    lv_label_set_text(label, "-");
    setLabelEnabled(label, false);
    return;
  }

  lv_label_set_text(label, snapshot.current_time);
  setLabelEnabled(label, true);
}
void setNeedleAngle(lv_obj_t *needle, const OptionalFloat &value, bool is_speed) {
  if (needle == nullptr) {
    return;
  }

  if (!value.present) {
    setImageEnabled(needle, false);
    return;
  }

  lv_img_set_angle(needle, is_speed ? speedToNeedleAngle(value.value) : attitudeToNeedleAngle(value.value));
  setImageEnabled(needle, true);
}

void setTemperatureBar(lv_obj_t *bar, const OptionalFloat &value) {
  if (bar == nullptr) {
    return;
  }

  if (!value.present) {
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    setBarEnabled(bar, false);
    return;
  }

  lv_bar_set_value(bar, temperatureToBarValue(value.value), LV_ANIM_OFF);
  setBarEnabled(bar, true);
}

SensorPodSnapshot sensorPodDisplaySnapshot() {
  display_snapshot_count++;
  SensorPodSnapshot snapshot = sensor_pod;
  if (!snapshot.has_frame || (millis() - snapshot.last_update_ms) > kSensorPodTimeoutMs) {
    snapshot.cvt_temp_f.present = false;
    snapshot.aux_temp_f.present = false;
    snapshot.wheel_speed_mph.present = false;
    snapshot.roll_deg.present = false;
    snapshot.pitch_deg.present = false;
  }
  return snapshot;
}

RfPodSnapshot rfPodDisplaySnapshot() {
  RfPodSnapshot snapshot = rf_pod;
  if (!snapshot.has_frame || (millis() - snapshot.last_update_ms) > kRfPodTimeoutMs) {
    snapshot.gps_speed_mph.present = false;
    snapshot.latitude.present = false;
    snapshot.longitude.present = false;
    snapshot.gps_fix.present = false;
    snapshot.lap_current_ms.present = false;
    snapshot.lap_last_ms.present = false;
    snapshot.lap_best_ms.present = false;
    snapshot.current_time_present = false;
  }
  return snapshot;
}

void disableSensorPodDisplay() {
  OptionalFloat missing;

  setNeedleAngle(ui_indicatorSpeedWheel, missing, true);
  setNeedleAngle(ui_indicatorPitch, missing, false);
  setNeedleAngle(ui_indicatorRoll, missing, false);
  setIntegerLabel(ui_speedWheel, missing);
  setSignedTenthsLabel(ui_anglePitch, missing);
  setSignedTenthsLabel(ui_angleRoll, missing);
  setTemperatureBar(ui_tempCVT, missing);
  setTemperatureBar(ui_tempAux, missing);
}

void disableRfPodDisplay() {
  OptionalFloat missing;
  RfPodSnapshot missing_snapshot;

  setNeedleAngle(ui_indicatorSpeedGPS, missing, true);
  setIntegerLabel(ui_speedGPS, missing);
  setCurrentTimeLabel(ui_timeCurrent, missing_snapshot);
  setLapTimeLabel(ui_timeCurrentLap, missing);
  setLapTimeLabel(ui_timeLastLap, missing);
  setLapTimeLabel(ui_timeBestLap, missing);
}

void updateDashboardFromSerial(lv_timer_t *timer) {
  (void)timer;

  const SensorPodSnapshot sensor_snapshot = sensorPodDisplaySnapshot();
  const RfPodSnapshot rf_snapshot = rfPodDisplaySnapshot();
  const bool sensor_data_live = sensor_snapshot.has_frame && ((millis() - sensor_snapshot.last_update_ms) <= kSensorPodTimeoutMs);
  const bool rf_data_live = rf_snapshot.has_frame && ((millis() - rf_snapshot.last_update_ms) <= kRfPodTimeoutMs);

  setRfStatusLabel(rf_snapshot);
  setGpsStatusLabel(rf_snapshot, rf_data_live);

  if (!sensor_data_live) {
    if (sensor_pod_display_enabled) {
      disableSensorPodDisplay();
      sensor_pod_display_enabled = false;
    }
  } else {
    sensor_pod_display_enabled = true;
    setNeedleAngle(ui_indicatorSpeedWheel, sensor_snapshot.wheel_speed_mph, true);
    setNeedleAngle(ui_indicatorPitch, sensor_snapshot.pitch_deg, false);
    setNeedleAngle(ui_indicatorRoll, sensor_snapshot.roll_deg, false);
    setIntegerLabel(ui_speedWheel, sensor_snapshot.wheel_speed_mph);
    setSignedTenthsLabel(ui_anglePitch, sensor_snapshot.pitch_deg);
    setSignedTenthsLabel(ui_angleRoll, sensor_snapshot.roll_deg);
    setTemperatureBar(ui_tempCVT, sensor_snapshot.cvt_temp_f);
    setTemperatureBar(ui_tempAux, sensor_snapshot.aux_temp_f);
  }

  if (!rf_data_live) {
    if (rf_pod_display_enabled) {
      disableRfPodDisplay();
      rf_pod_display_enabled = false;
    }
    return;
  }

  rf_pod_display_enabled = true;
  setNeedleAngle(ui_indicatorSpeedGPS, rf_snapshot.gps_speed_mph, true);
  setIntegerLabel(ui_speedGPS, rf_snapshot.gps_speed_mph);
  setCurrentTimeLabel(ui_timeCurrent, rf_snapshot);
  setLapTimeLabel(ui_timeCurrentLap, rf_snapshot.lap_current_ms);
  setLapTimeLabel(ui_timeLastLap, rf_snapshot.lap_last_ms);
  setLapTimeLabel(ui_timeBestLap, rf_snapshot.lap_best_ms);
}
//  Display refresh
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  if (gfx.getStartCount() > 0) {
    gfx.endWrite();
  }
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (lgfx::rgb565_t *)&color_p->full);

  lv_disp_flush_ready(disp);  //	Tell lvgl that the refresh is complete
}

//  Read touch
void my_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
  data->state = LV_INDEV_STATE_REL;// The state of data existence when releasing the finger
  bool touched = gfx.getTouch( &touch_x, &touch_y );
  if (touched)
  {
    data->state = LV_INDEV_STATE_PR;

    //  Set coordinates
    data->point.x = touch_x;
    data->point.y = touch_y;
  }
}

bool i2cScanForAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

// Wrapper function for sending I2C commands
void sendI2CCommand(uint8_t command) {
  uint8_t error;
  // Start sending commands to the specified address
  Wire.beginTransmission(0x30);
  // Send command
  Wire.write(command);
  //  End transmission and return status
  error = Wire.endTransmission();

  if (error == 0) {
    Serial.print("command 0x");
    Serial.print(command, HEX);
    Serial.println(" Sent successfully");
  } else {
    Serial.print("Command sent error, error code:");
    Serial.println(error);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial0.begin(115200);

  pinMode(19, OUTPUT);

  Wire.begin(15, 16);
  delay(50);
  while (1) {
    if (i2cScanForAddress(0x30) && i2cScanForAddress(0x5D)) {
      Serial.print("The microcontroller is detected: address 0x");
      Serial.println(0x30, HEX);
      Serial.print("The microcontroller is detected: address 0x");
      Serial.println(0x5D, HEX);
      break;
    } else {
      Serial.print("No microcontroller was detected: address 0x");
      Serial.println(0x30, HEX);
      Serial.print("No microcontroller was detected: address 0x");
      Serial.println(0x5D, HEX);
      //Prevent the microcontroller did not start to adjust the bright screen
      sendI2CCommand(0x19);    // 0x19 : Activate touch screen
      pinMode(1, OUTPUT);
      digitalWrite(1, LOW);
      
      delay(120);
      pinMode(1, INPUT);

      delay(100);
    }
  }
  // Start sending command 0x10 to address 0x30
  sendI2CCommand(0x10);  // 0x10 is the brightest backlight.   

  // Init Display
  gfx.init();
  gfx.initDMA();
  gfx.startWrite();
  gfx.fillScreen(TFT_BLACK);

  lv_init();
  size_t buffer_size = sizeof(lv_color_t) * LCD_H_RES * LCD_V_RES;
  buf = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);

  lv_disp_draw_buf_init(&draw_buf, buf, buf1, LCD_H_RES * LCD_V_RES);

  // Initialize display
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  // Change the following lines to your display resolution
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Initialize input device driver program
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  delay(100);
  gfx.fillScreen(TFT_BLACK);
  // lv_demo_widgets();// Main UI interface
  ui_init();
  disableSensorPodDisplay();
  disableRfPodDisplay();
  lv_timer_create(updateDashboardFromSerial, kDisplayUpdateMs, nullptr);

  Serial.println( "Setup done" );
}

void loop()
{
  processSerialInput();
  lv_timer_handler(); /* let the GUI do its work */
  delay(1);
}
