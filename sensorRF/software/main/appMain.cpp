#include "sensorRf/boardConfig.hpp"

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"

#include <cinttypes>

namespace {

constexpr char logTag[] = "sensorRF";

void logHardwareInfo()
{
    esp_chip_info_t chipInfo{};
    esp_chip_info(&chipInfo);

    uint32_t flashSizeBytes = 0;
    ESP_ERROR_CHECK(esp_flash_get_size(nullptr, &flashSizeBytes));

    ESP_LOGI(logTag, "sensorRF starting");
    ESP_LOGI(logTag, "ESP32-S3 cores: %u, revision: %u.%u",
             chipInfo.cores,
             chipInfo.revision / 100,
             chipInfo.revision % 100);
    ESP_LOGI(logTag, "Flash: %" PRIu32 " bytes", flashSizeBytes);
    ESP_LOGI(logTag, "PSRAM: %u bytes", esp_psram_get_size());
    ESP_LOGI(logTag, "TWAI pins: TX GPIO%d, RX GPIO%d",
             static_cast<int>(sensorRf::boardConfig::canTxPin),
             static_cast<int>(sensorRf::boardConfig::canRxPin));
}

} // namespace

extern "C" void app_main()
{
    logHardwareInfo();
}
