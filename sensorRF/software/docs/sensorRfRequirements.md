# sensorRF system requirements

Status: Draft 0.1

## Purpose and scope

sensorRF is the Baja vehicle RF sensor module. It acquires GNSS navigation
data, publishes navigation and health data to the vehicle control head over
Classic CAN, accepts selected vehicle parameters over CAN, and transmits
configured telemetry to a base station over LoRa.

The keywords **shall**, **should**, and **may** indicate mandatory,
recommended, and optional behavior respectively.

## Hardware baseline

| ID | Requirement |
|---|---|
| SRF-HW-001 | The firmware shall target the Heltec WiFi LoRa 32 V4 ESP32-S3R2 with 16 MB flash and 2 MB PSRAM. |
| SRF-HW-002 | The firmware shall interface to the L76KB-A58 GNSS receiver through a dedicated UART. |
| SRF-HW-003 | The firmware shall interface to Classic CAN through the ESP32-S3 TWAI controller and an SN65HVD230-compatible external transceiver. |
| SRF-HW-004 | TWAI TX shall use GPIO6 and TWAI RX shall use GPIO4. |
| SRF-HW-005 | The firmware shall preserve the Heltec LoRa front-end control assignments, including GPIO7, exclusively for radio use. |
| SRF-HW-006 | The firmware shall detect and report failure to initialize flash, PSRAM, GNSS, CAN, or the LoRa radio. |

## GNSS and navigation

| ID | Requirement |
|---|---|
| SRF-NAV-001 | The firmware shall configure the GNSS receiver for its highest validated navigation update rate. |
| SRF-NAV-002 | The minimum acceptable validated GNSS update rate shall be 5 Hz; 10 Hz is a design goal pending receiver confirmation. |
| SRF-NAV-003 | The firmware shall collect latitude, longitude, altitude, ground speed, course over ground, fix type, satellite count, dilution of precision, and receiver time when available. |
| SRF-NAV-004 | Velocity shall use the GNSS receiver's reported ground speed rather than position differentiation. |
| SRF-NAV-005 | Every navigation solution shall receive an incrementing 8-bit `solutionCounter` and a monotonic receipt timestamp. |
| SRF-NAV-006 | Published navigation data shall include validity and age information so consumers can reject stale solutions. |

## CAN interface

| ID | Requirement |
|---|---|
| SRF-CAN-001 | The CAN interface shall conform to `../../../common/system/canIcd.md` and advertise its 16-bit CAN and LoRa schema identifiers. |
| SRF-CAN-002 | The initial bus configuration shall be Classic CAN, 11-bit identifiers, and 500 kbit/s. |
| SRF-CAN-003 | Navigation position and motion frames shall be published at 10 Hz, repeating the most recent solution when GNSS updates are slower. |
| SRF-CAN-004 | Repeated solutions shall retain the same `solutionCounter`; a new receiver solution shall increment it modulo 256. |
| SRF-CAN-005 | CAN transmission shall not block GNSS acquisition or LoRa processing. |
| SRF-CAN-006 | The firmware shall detect error-passive and bus-off states and shall report recovery attempts and counters. |
| SRF-CAN-007 | Received parameter values shall be range-checked and freshness-checked before telemetry use. |
| SRF-CAN-008 | Driver text shall be transferred to the control head using the ISO-TP service defined by `../../../common/system/canIcd.md`; an incomplete or invalid transfer shall not be displayed. |
| SRF-CAN-009 | LoRa calibration commands shall not be reported complete until their addressed CAN operation returns a terminal result. |

## LoRa telemetry

| ID | Requirement |
|---|---|
| SRF-RF-001 | The LoRa interface shall conform to `../../../common/system/loraIcd.md` and shall operate only under the configured regional profile. |
| SRF-RF-002 | The production default regional profile shall be US915, constrained to 902–928 MHz. |
| SRF-RF-003 | Firmware shall reject a configured transmit frequency outside the active regional profile. |
| SRF-RF-004 | Initial bench testing shall default to no more than 10 dBm conducted transmit power. |
| SRF-RF-005 | The configured power, bandwidth, channel behavior, antenna gain, and transmission schedule shall comply with applicable FCC rules and event requirements. |
| SRF-RF-006 | Telemetry scheduling shall be configurable by parameter and shall prevent unbounded channel utilization. |
| SRF-RF-007 | Every application frame shall include a source node, sequence number, timestamp, schema identifier, and integrity check. |
| SRF-RF-008 | Unsupported schema identifiers or packet types shall be rejected without affecting other interfaces. |
| SRF-RF-009 | CAN data shall be relayed at reduced, class-specific LoRa rates rather than at its original CAN cadence. |
| SRF-RF-010 | Relayed CAN data shall include source-data age so a newly received LoRa frame cannot make stale CAN data appear current. |
| SRF-RF-011 | Reduction of suspension and acceleration data shall preserve per-window extrema rather than select only the latest high-rate sample. |
| SRF-RF-012 | When LoRa transmission is delayed, telemetry aggregation shall continue until a payload is committed to the radio queue and shall report the extended observation duration. |
| SRF-RF-013 | Aperiodic paddock commands shall support capture-level calibration, suspension neutral-position calibration, and a driver text message of up to 128 UTF-8 bytes. |
| SRF-RF-014 | Commands and responses shall use direction-specific HMAC-SHA-256 keys with 128-bit tags and persistent monotonic counters for authorization and replay protection. |
| SRF-RF-015 | A duplicate authenticated command shall return cached status and shall not repeat its CAN operation or driver display action. |

## Web interface

| ID | Requirement |
|---|---|
| SRF-WEB-001 | Vehicle and paddock web hosts shall implement the common read-only interface defined by `../../../common/system/webApiIcd.md`. |
| SRF-WEB-002 | Every identity and snapshot response shall identify the host as `vehicleMirror` or `paddockReceiver`. |
| SRF-WEB-003 | The vehicle interface shall not represent locally constructed telemetry as proof of LoRa delivery. |
| SRF-WEB-004 | The paddock interface shall expose LoRa receive age and RSSI/SNR when provided by the receiver. |
| SRF-WEB-005 | The built-in page shall use local assets and shall poll the snapshot API no faster than once every five seconds. |
| SRF-WEB-006 | Missing or stale values shall remain distinguishable from valid zero values in both JSON and the built-in page. |
| SRF-WEB-007 | The vehicle host shall provide a WPA2-or-stronger SoftAP that does not depend on external network infrastructure. |
| SRF-WEB-008 | The paddock host shall provide simultaneous SoftAP and station operation, and loss of the station link shall not disable the SoftAP or LoRa receiver. |
| SRF-WEB-009 | Production SoftAP credentials shall be provisioned per device or deployment and shall not use a universal default password. |

## Configuration, diagnostics, and robustness

| ID | Requirement |
|---|---|
| SRF-SYS-001 | Persistent configuration shall be versioned and validated before use. |
| SRF-SYS-002 | Invalid persistent configuration shall cause safe defaults to be loaded and a diagnostic flag to be asserted. |
| SRF-SYS-003 | GNSS, CAN, LoRa, and system supervision shall execute as independent tasks communicating through bounded queues or snapshots. |
| SRF-SYS-004 | A failure or timeout in one external interface shall not indefinitely block another interface. |
| SRF-SYS-005 | The firmware shall enable watchdog supervision for operational tasks. |
| SRF-SYS-006 | Logs shall identify firmware version, reset reason, hardware target, detected flash and PSRAM, and active interface schema identifiers. |
| SRF-SYS-007 | Wire-format serialization shall use explicit integer widths and byte order; raw C or C++ structure layout shall not be transmitted. |
| SRF-SYS-008 | Schema identifiers shall be generated according to `../../../common/system/schemaIdentification.md`. |

## Verification requirements

| ID | Requirement |
|---|---|
| SRF-VER-001 | Every released schema manifest shall have automated identifier, encoding, decoding, boundary, and malformed-input tests. |
| SRF-VER-002 | CAN bus-off recovery shall be tested with a disconnected or fault-injected bus. |
| SRF-VER-003 | GNSS solution age and repeated-solution behavior shall be tested at receiver rates below the CAN publication rate. |
| SRF-VER-004 | LoRa range and packet-loss tests shall record frequency, power, antenna, bandwidth, spreading factor, coding rate, vehicle state, and distance. |
| SRF-VER-005 | Full-power LoRa transmission shall be tested for interference with GNSS reception, CAN operation, and power integrity. |

## Open requirements decisions

- CAN node addressing and arbitration with future family products.
- Exact GNSS maximum rate supported by the installed L76KB-A58 firmware.
- Point-to-point, star, or LoRaWAN network architecture.
- FCC-compliant channel strategy for proprietary LoRa operation.
- HMAC key provisioning, rotation, recovery, and whether command/text
  confidentiality requires authenticated encryption in a later schema.
- Final LoRa packet rates after PHY selection, airtime calculation, and paddock range testing.
- Base-station acknowledgements and fault-event reporting policy.
- Vehicle input-voltage, transient, environmental, and enclosure requirements.
