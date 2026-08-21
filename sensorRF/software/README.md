# sensorRF firmware

Native ESP-IDF firmware for the Heltec WiFi LoRa 32 V4 with ESP32-S3R2,
2 MB PSRAM, and 16 MB flash.

## Hardware assignments

- TWAI TX: GPIO6 to SN65HVD230 D/TXD
- TWAI RX: GPIO4 from SN65HVD230 R/RXD
- GNSS power control: GPIO34
- Status LED: GPIO35
- External power control: GPIO36
- ADC control: GPIO37
- GNSS reset: GPIO42

## Build

Open an ESP-IDF terminal in VS Code, then run:

```powershell
idf.py set-target esp32s3
idf.py build
```

The generated `sdkconfig` is intentionally ignored. Reproducible project
settings belong in `sdkconfig.defaults`.

## Interface documents

- [System requirements](docs/sensorRfRequirements.md)
- [CAN ICD](../../common/system/canIcd.md)
- [LoRa ICD](../../common/system/loraIcd.md)
- [Web API ICD](../../common/system/webApiIcd.md)
- [Schema identification](../../common/system/schemaIdentification.md)
- [Schema history](../../common/system/schemaHistory.md)
