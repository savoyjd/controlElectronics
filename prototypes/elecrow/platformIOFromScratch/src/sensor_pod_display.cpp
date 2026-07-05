#include "sensor_pod_display.h"

#include <lvgl.h>

#include "ui.h"

namespace {

constexpr uint32_t kSensorPodStaleMs = 250;
constexpr uint32_t kDisplayRefreshMinMs = 100;
constexpr uint32_t kTransitionRefreshMinMs = 40;
constexpr uint8_t kRenderGroupCount = 5;

constexpr int kTempMinF = 0;
constexpr int kTempMaxF = 250;

constexpr float kWheelSpeedMinMph = 0.0f;
constexpr float kWheelSpeedMaxMph = 60.0f;
constexpr int16_t kWheelNeedleMinAngle = -1350;
constexpr int16_t kWheelNeedleMaxAngle = 1350;
constexpr int16_t kWheelNeedleStepAngle = 50;

constexpr float kAttitudeMinDeg = -45.0f;
constexpr float kAttitudeMaxDeg = 45.0f;
constexpr int16_t kAttitudeStepAngle = 10;

constexpr lv_opa_t kValidOpacity = LV_OPA_COVER;
constexpr lv_opa_t kMissingOpacity = LV_OPA_40;
constexpr uint32_t kValidTextColor = 0xE8F1F2;
constexpr uint32_t kMissingTextColor = 0x6C858D;

SensorPodData sensor_data;
uint32_t last_update_ms = 0;
bool have_update = false;
bool rendered_missing = false;
bool rendered_once = false;
uint32_t last_render_ms = 0;
bool transition_active = false;
bool transition_target_missing = true;
uint8_t transition_group = 0;

struct RenderCache {
  bool cvt_bar_valid = false;
  bool cvt_icon_valid = false;
  int32_t cvt_temperature_f = -1;
  bool aux_bar_valid = false;
  bool aux_icon_valid = false;
  int32_t aux_temperature_f = -1;
  bool speed_image_valid = false;
  int32_t speed_mph = -1;
  int16_t speed_angle = 0;
  bool roll_label_valid = false;
  bool roll_image_valid = false;
  int32_t roll_tenths_deg = 0;
  int16_t roll_angle = 0;
  bool pitch_label_valid = false;
  bool pitch_image_valid = false;
  int32_t pitch_tenths_deg = 0;
  int16_t pitch_angle = 0;
};

RenderCache rendered;

float clampFloat(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

int32_t mapFloatToInt(float value, float in_min, float in_max, int32_t out_min, int32_t out_max) {
  const float clamped = clampFloat(value, in_min, in_max);
  const float ratio = (clamped - in_min) / (in_max - in_min);
  return static_cast<int32_t>(out_min + ratio * (out_max - out_min));
}

int32_t roundFloatToInt(float value) {
  return static_cast<int32_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

int32_t roundFloatToTenths(float value) {
  return roundFloatToInt(value * 10.0f);
}

int16_t quantizeAngle(int32_t angle, int16_t step) {
  return static_cast<int16_t>((angle / step) * step);
}

void setObjectMissing(lv_obj_t *obj, bool missing) {
  if (obj == nullptr) {
    return;
  }
  if (missing) {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
    lv_obj_set_style_opa(obj, kMissingOpacity, LV_PART_MAIN | LV_STATE_DEFAULT);
  } else {
    lv_obj_clear_state(obj, LV_STATE_DISABLED);
    lv_obj_set_style_opa(obj, kValidOpacity, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void setLabelMissing(lv_obj_t *label, bool missing) {
  if (label == nullptr) {
    return;
  }
  lv_obj_set_style_text_color(label, lv_color_hex(missing ? kMissingTextColor : kValidTextColor),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  setObjectMissing(label, missing);
}

void setBarValueOrMissing(lv_obj_t *bar, bool valid, int32_t value_f, bool &rendered_valid, int32_t &rendered_value) {
  if (bar == nullptr) {
    return;
  }

  if (valid == rendered_valid && value_f == rendered_value) {
    return;
  }

  if (valid != rendered_valid) {
    setObjectMissing(bar, !valid);
    rendered_valid = valid;
  }

  if (valid) {
    const int32_t value = mapFloatToInt(static_cast<float>(value_f), kTempMinF, kTempMaxF, kTempMinF, kTempMaxF);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    rendered_value = value_f;
  } else {
    lv_bar_set_value(bar, kTempMinF, LV_ANIM_OFF);
    rendered_value = -1;
  }
}

void setIconMissing(lv_obj_t *icon, bool valid, bool &rendered_valid) {
  if (icon == nullptr || valid == rendered_valid) {
    return;
  }

  setObjectMissing(icon, !valid);
  rendered_valid = valid;
}

void setSpeedLabel(lv_obj_t *label, bool valid, int32_t speed_mph) {
  static bool rendered_valid = false;
  static int32_t rendered_speed_mph = -1;

  if (label == nullptr) {
    return;
  }

  if (valid == rendered_valid && speed_mph == rendered_speed_mph) {
    return;
  }

  if (valid != rendered_valid) {
    setLabelMissing(label, !valid);
    rendered_valid = valid;
  }

  if (valid) {
    lv_label_set_text_fmt(label, "%ld", static_cast<long>(speed_mph));
    rendered_speed_mph = speed_mph;
  } else {
    lv_label_set_text(label, "-");
    rendered_speed_mph = -1;
  }
}

void setAngleLabel(lv_obj_t *label, bool valid, int32_t angle_tenths_deg, bool &rendered_valid,
                   int32_t &rendered_angle_tenths_deg) {
  if (label == nullptr) {
    return;
  }

  if (valid == rendered_valid && angle_tenths_deg == rendered_angle_tenths_deg) {
    return;
  }

  if (valid != rendered_valid) {
    setLabelMissing(label, !valid);
    rendered_valid = valid;
  }

  if (valid) {
    lv_label_set_text_fmt(label, "%+.1f", static_cast<double>(angle_tenths_deg) / 10.0);
    rendered_angle_tenths_deg = angle_tenths_deg;
  } else {
    lv_label_set_text(label, "-");
    rendered_angle_tenths_deg = 0;
  }
}

void setImageAngleOrMissing(lv_obj_t *image, bool valid, int16_t angle, bool &rendered_valid,
                            int16_t &rendered_angle) {
  if (image == nullptr) {
    return;
  }

  if (valid == rendered_valid && angle == rendered_angle) {
    return;
  }

  if (valid != rendered_valid) {
    setObjectMissing(image, !valid);
    rendered_valid = valid;
  }

  if (valid) {
    lv_img_set_angle(image, angle);
    rendered_angle = angle;
  } else {
    lv_img_set_angle(image, 0);
    rendered_angle = 0;
  }
}

void applySensorPodData(bool force_missing, uint8_t group) {
  const bool data_missing = force_missing || !have_update;

  const bool cvt_valid = !data_missing && sensor_data.cvt_temperature_valid;
  const bool aux_valid = !data_missing && sensor_data.aux_temperature_valid;
  const bool speed_valid = !data_missing && sensor_data.wheel_speed_valid;
  const bool roll_valid = !data_missing && sensor_data.roll_valid;
  const bool pitch_valid = !data_missing && sensor_data.pitch_valid;

  const int32_t cvt_temperature_f = roundFloatToInt(sensor_data.cvt_temperature_f);
  const int32_t aux_temperature_f = roundFloatToInt(sensor_data.aux_temperature_f);
  const int32_t speed_mph = roundFloatToInt(sensor_data.wheel_speed_mph);
  const int16_t speed_angle = quantizeAngle(mapFloatToInt(sensor_data.wheel_speed_mph, kWheelSpeedMinMph,
                                                         kWheelSpeedMaxMph, kWheelNeedleMinAngle,
                                                         kWheelNeedleMaxAngle),
                                            kWheelNeedleStepAngle);
  const int32_t roll_tenths_deg = roundFloatToTenths(sensor_data.roll_deg);
  const int16_t roll_angle = quantizeAngle(mapFloatToInt(sensor_data.roll_deg, kAttitudeMinDeg, kAttitudeMaxDeg,
                                                        static_cast<int16_t>(kAttitudeMinDeg * 10.0f),
                                                        static_cast<int16_t>(kAttitudeMaxDeg * 10.0f)),
                                           kAttitudeStepAngle);
  const int32_t pitch_tenths_deg = roundFloatToTenths(sensor_data.pitch_deg);
  const int16_t pitch_angle = quantizeAngle(mapFloatToInt(sensor_data.pitch_deg, kAttitudeMinDeg, kAttitudeMaxDeg,
                                                         static_cast<int16_t>(kAttitudeMinDeg * 10.0f),
                                                         static_cast<int16_t>(kAttitudeMaxDeg * 10.0f)),
                                            kAttitudeStepAngle);

  switch (group) {
    case 0:
      setBarValueOrMissing(ui_tempCVT, cvt_valid, cvt_temperature_f, rendered.cvt_bar_valid,
                           rendered.cvt_temperature_f);
      setIconMissing(ui_iconCVTTemp, cvt_valid, rendered.cvt_icon_valid);
      break;

    case 1:
      setBarValueOrMissing(ui_tempAux, aux_valid, aux_temperature_f, rendered.aux_bar_valid,
                           rendered.aux_temperature_f);
      setIconMissing(ui_iconAuxTemp, aux_valid, rendered.aux_icon_valid);
      break;

    case 2:
      setSpeedLabel(ui_speedWheel, speed_valid, speed_mph);
      setImageAngleOrMissing(ui_indicatorSpeedWheel, speed_valid, speed_angle, rendered.speed_image_valid,
                             rendered.speed_angle);
      break;

    case 3:
      setAngleLabel(ui_angleRoll, roll_valid, roll_tenths_deg, rendered.roll_label_valid,
                    rendered.roll_tenths_deg);
      setImageAngleOrMissing(ui_idicatorRoll, roll_valid, roll_angle, rendered.roll_image_valid,
                             rendered.roll_angle);
      break;

    case 4:
      setAngleLabel(ui_anglePitch, pitch_valid, pitch_tenths_deg, rendered.pitch_label_valid,
                    rendered.pitch_tenths_deg);
      setImageAngleOrMissing(ui_indicatorPitch, pitch_valid, pitch_angle, rendered.pitch_image_valid,
                             rendered.pitch_angle);
      break;
  }
}

void applyAllSensorPodData(bool force_missing) {
  for (uint8_t group = 0; group < kRenderGroupCount; ++group) {
    applySensorPodData(force_missing, group);
  }
}

bool applyTransitionStep(bool force_missing, uint32_t now_ms) {
  if (!transition_active || transition_target_missing != force_missing) {
    transition_active = true;
    transition_target_missing = force_missing;
    transition_group = 0;
  }

  if (last_render_ms != 0 && now_ms - last_render_ms < kTransitionRefreshMinMs) {
    return false;
  }

  applySensorPodData(force_missing, transition_group);
  ++transition_group;
  last_render_ms = now_ms;

  if (transition_group >= kRenderGroupCount) {
    transition_group = 0;
    transition_active = false;
    return true;
  }

  return false;
}

}  // namespace

void sensorPodDisplayInit() {
  sensorPodDisplayMarkMissing();
}

void sensorPodDisplaySetData(const SensorPodData &data, uint32_t now_ms) {
  sensor_data = data;
  last_update_ms = now_ms;
  have_update = true;
}

void sensorPodDisplayMarkMissing() {
  have_update = false;
  if (!rendered_once || !rendered_missing) {
    if (applyTransitionStep(true, millis())) {
      rendered_missing = true;
      rendered_once = true;
    }
  }
}

void sensorPodDisplayRefresh(uint32_t now_ms) {
  const bool stale = !have_update || now_ms - last_update_ms > kSensorPodStaleMs;

  if (stale) {
    if (!rendered_once || !rendered_missing) {
      if (applyTransitionStep(true, now_ms)) {
        rendered_missing = true;
        rendered_once = true;
      }
    }
    return;
  }

  if (rendered_missing || !rendered_once) {
    if (applyTransitionStep(false, now_ms)) {
      rendered_missing = false;
      rendered_once = true;
    }
    return;
  }

  if (now_ms - last_render_ms >= kDisplayRefreshMinMs) {
    applyAllSensorPodData(false);
    rendered_missing = false;
    rendered_once = true;
    last_render_ms = now_ms;
  }
}

bool sensorPodDisplayTransitionActive() {
  return transition_active;
}
