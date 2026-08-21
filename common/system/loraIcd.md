# sensor family LoRa interface control document

Status: Draft 0.3  
Schema ID: `0x8035`

The machine-readable source of truth is `schema/loraSchema.json`.

## Regional and radio constraints

The production region is US915. All configured carriers shall remain within
902–928 MHz. A compliant operating mode involves more than choosing a center
frequency; power, bandwidth, antenna gain, channel behavior, emissions, and
transmission schedule remain system-level obligations.

Initial bench power shall not exceed 10 dBm. No released firmware shall inherit
the Heltec example's 868 MHz default.

The channel plan, spreading factor, bandwidth, coding rate, transmit power,
and airtime limits remain open pending selection of proprietary point-to-point
operation versus LoRaWAN. Those values are deliberately outside schema 0.1.

## Application frame

All multibyte fields are little-endian.

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `magic` | `0x5352` (bytes `52 53`, ASCII `RS`) |
| 2 | `protocolVersion` | 1 |
| 3 | `packetType` | Enumeration below |
| 4–5 | `schemaId` | LoRa schema identifier |
| 6–7 | `sourceNode` | Provisioned family node ID |
| 8–9 | `sequence` | Increments per transmitted frame |
| 10–13 | `timestamp` | Monotonic milliseconds modulo 2³² |
| 14 | `flags` | Bit field below |
| 15 | `payloadLength` | Payload bytes, excluding header and CRC |
| 16… | `payload` | Packet-type-specific data |
| final 4 | `frameCrc` | CRC-32C over header and payload |

Maximum application payload is 192 bytes, including the 16-byte header and
4-byte CRC. Receivers shall reject incorrect magic, version, length, schema,
or CRC before interpreting payload data.

Packet types: 1 navigation, 2 dynamic telemetry, 3 thermal telemetry, 4 status
telemetry, 5 command, and 6 command response. Values 0 and 7–255 are reserved.

Flags: bit 0 acknowledgement requested, bit 1 acknowledgement, bit 2 encrypted,
and bit 3 GNSS time valid. Bits 4–7 are reserved and zero. Encryption is not
defined by schema 0.1; therefore bit 2 shall be zero in this draft.

## Navigation payload — packet type 1

Nominal publication rate is 2 Hz, reduced from the 10 Hz CAN publication rate.

Coordinates use the WGS 84 reference datum. Latitude and longitude are signed
decimal degrees scaled by 10,000,000 (E7 encoding). Positive latitude is
north, negative latitude is south, positive longitude is east, and negative
longitude is west.

| Payload byte | Field | Encoding |
|---:|---|---|
| 0–3 | `latitude` | signed, degrees × 10,000,000 |
| 4–7 | `longitude` | signed, degrees × 10,000,000 |
| 8–9 | `altitude` | signed, 0.1 m per bit |
| 10–11 | `groundSpeed` | unsigned, 0.01 m/s per bit |
| 12–13 | `courseOverGround` | unsigned, 0.01 degree per bit |
| 14–15 | `solutionAge` | milliseconds, saturates at 65535 |
| 16 | `satellitesUsed` | count; 255 unavailable |
| 17 | `fixType` | same enumeration as CAN |
| 18 | `solutionFlags` | same bit allocation as CAN |
| 19 | `solutionCounter` | same solution identity as CAN |

Valid encoded ranges are `-900000000` through `+900000000` for latitude and
`-1800000000` through `+1800000000` for longitude. Values outside these ranges
shall be rejected. One latitude count is approximately 0.0111 m; one longitude
count is approximately `0.0111 × cos(latitude)` meters. Encoding resolution
does not imply equivalent GNSS measurement accuracy. Invalid coordinates use
the validity flags rather than sentinel numeric values.

## Reduced-rate relay behavior

sensorRF receives CAN data at rates selected for vehicle control and local
acquisition, but the paddock link is a telemetry service. It shall not attempt
to forward every CAN occurrence. The initial nominal LoRa rates are:

| Packet | Nominal rate | Purpose |
|---|---:|---|
| Navigation | 2 Hz | Latest position, motion, and fix state |
| Dynamic telemetry | 2 Hz | Latest speed data plus windowed suspension and acceleration extrema |
| Thermal telemetry | 0.5 Hz | Both thermocouple channels |
| Status telemetry | 0.2 Hz | Node identity, system status, navigation quality, and CAN/LoRa schema announcement |

These are scheduler targets, not permission to exceed the configured regional
airtime budget. The scheduler may reduce rates or omit lower-priority packets
to remain within that budget. It shall not increase a rate above this table
without an explicitly validated configuration. Priority is status/fault event,
navigation, dynamic telemetry, thermal telemetry, then routine status.

Every relayed CAN group includes its age at the time the LoRa payload snapshot
is captured. Age begins when sensorRF receives and validates the corresponding
CAN frame and saturates at 65535 ms. A saturated age means 65535 ms or older,
not exactly 65535 ms. A LoRa receiver shall apply both CAN-field validity flags
and age limits; receiving a fresh radio packet does not make its contents fresh.

## Dynamic telemetry payload — packet type 2

Nominal publication rate is 2 Hz. Payload length is 54 bytes.

| Payload byte | Field | Encoding |
|---:|---|---|
| 0–1 | `canSchemaId` | CAN schema used to interpret relayed fields |
| 2–3 | `windowDuration` | aggregation duration in ms; saturates at 65535 |
| 4–5 | `wheelSpeed` | 0.01 m/s per bit; 65535 unavailable |
| 6–9 | `wheelEdgePeriod` | µs; 4294967295 unavailable |
| 10 | `wheelEdgeAge` | 10 ms per bit; saturates at 255 |
| 11 | `wheelFlags` | same allocation as CAN `speedFlags` |
| 12–13 | `wheelCanAge` | ms; saturates at 65535 |
| 14–15 | `shaftSpeed` | RPM; 65535 unavailable |
| 16–19 | `shaftEdgePeriod` | µs; 4294967295 unavailable |
| 20 | `shaftEdgeAge` | 10 ms per bit; saturates at 255 |
| 21 | `shaftFlags` | same allocation as CAN `speedFlags` |
| 22–23 | `shaftCanAge` | ms; saturates at 65535 |
| 24–29 | suspension latest, minimum, maximum | three signed values, 0.1 mm per bit; -32768 unavailable |
| 30 | `displacementFlags` | same allocation as CAN |
| 31–32 | `displacementCanAge` | age of latest contributing CAN frame in ms |
| 33–38 | latest acceleration X, Y, Z | three signed values, 0.01 m/s² per bit |
| 39–44 | minimum acceleration X, Y, Z | per-axis minima over aggregation window |
| 45–50 | maximum acceleration X, Y, Z | per-axis maxima over aggregation window |
| 51 | `accelerationFlags` | accumulated flags described below |
| 52–53 | `accelerationCanAge` | age of latest contributing CAN frame in ms |

Wheel and shaft fields are the latest validated CAN values; raw periods and
stopped semantics are preserved. For suspension, sensorRF shall take the
minimum of all valid CAN `minimumDisplacement` values and the maximum of all
valid CAN `maximumDisplacement` values received during the LoRa window. It
shall not merely forward the final 50 ms CAN window.

Acceleration latest values come from the most recent valid CAN vector. Per-axis
minimum and maximum include every valid acceleration CAN sample received during
the LoRa window. In the LoRa `accelerationFlags`, validity and calibration bits
describe the latest sample, while overrun and clipping bits are the logical OR
of all contributing samples. If no valid sample contributes to an aggregate,
its extrema use -32768 and the applicable validity flag is clear.

Aggregation begins immediately after the preceding dynamic payload is
successfully committed to the radio transmit queue. Snapshot and accumulator
reset shall be atomic with respect to CAN reception, and the next window shall
be seeded with the most recent valid values. If transmission is delayed,
aggregation continues and `windowDuration` reports the extended interval.

## Thermal telemetry payload — packet type 3

Nominal publication rate is 0.5 Hz. Payload length is 22 bytes.

| Payload byte | Field | Encoding |
|---:|---|---|
| 0–1 | `canSchemaId` | CAN schema used by embedded data |
| 2–9 | `thermocouple1Data` | exact eight data bytes from CAN `0x183` |
| 10–11 | `thermocouple1CanAge` | ms; saturates at 65535 |
| 12–19 | `thermocouple2Data` | exact eight data bytes from CAN `0x184` |
| 20–21 | `thermocouple2CanAge` | ms; saturates at 65535 |

## Status telemetry payload — packet type 4

Nominal publication rate is 0.2 Hz. Payload length is 42 bytes. Each `Data`
field is an exact eight-byte CAN payload, paired with its age in milliseconds:

| Payload byte | Field | CAN source |
|---:|---|---|
| 0–1 | `canSchemaId` | Current CAN schema identifier |
| 2–9 | `nodeIdentityData` | `0x081` |
| 10–11 | `nodeIdentityCanAge` | Age |
| 12–19 | `systemStatusData` | `0x103` |
| 20–21 | `systemStatusCanAge` | Age |
| 22–29 | `navigationQualityData` | `0x102` |
| 30–31 | `navigationQualityCanAge` | Age |
| 32–39 | `schemaAnnouncementData` | `0x080` |
| 40–41 | `schemaAnnouncementCanAge` | Age |

The embedded `canSchemaId` must be recognized before decoding any copied CAN
payload. Unknown CAN schemas may be logged as opaque bytes but not interpreted.

## Reliability and commands

Sequence numbers permit duplicate and loss detection. Acknowledgements should
be reserved for commands and exceptional telemetry; periodic navigation shall
normally be unacknowledged.

### Command envelope — packet type 5

Commands are aperiodic. Their payload is:

| Payload byte | Field | Encoding |
|---:|---|---|
| 0 | `commandCode` | 1 capture level, 2 zero suspension displacement, 3 driver text |
| 1 | `targetProduct` | 2 sensorOne, 3 control head |
| 2 | `targetInstance` | configured CAN node instance; 255 prohibited |
| 3 | `commandFlags` | command-specific bits; all unspecified bits zero |
| 4–7 | `commandCounter` | persistent, strictly increasing counter for the paddock sender |
| 8 | `parameterLength` | parameter bytes following this field |
| 9… | `parameters` | command-specific payload |
| final 16 | `authenticationTag` | first 16 bytes of HMAC-SHA-256 |

The application-header `ackRequested` flag shall be one for every command.
The payload length equals `9 + parameterLength + 16` and shall not exceed 157
bytes. Commands with a broadcast target, wrong target product, incorrect
parameter length, reserved flags, or failed authentication shall not reach CAN.

#### `captureLevel` — command 1

Target product shall be sensorOne. `commandFlags` bit 0 requests persistent
storage; bits 1–7 are zero. Parameters are exactly two bytes containing the
CAN `sensorYawOffset` signed value at 0.01 degree per bit. sensorRF shall issue
the CAN `captureLevel` command with the requested instance, persistence flag,
yaw offset, and CAN confirmation token, then map CAN responses back to the
originating LoRa command counter.

#### `zeroSuspensionDisplacement` — command 2

Target product shall be sensorOne. `commandFlags` bit 0 requests persistent
storage; bits 1–7 are zero and `parameterLength` is zero. sensorRF shall issue
the corresponding CAN command and map its response back to LoRa.

#### `driverText` — command 3

Target product shall be control head. `commandFlags` bit 0 marks the message
urgent and bit 1 requests clearing an existing message first. Parameters are:

| Parameter byte | Field | Encoding |
|---:|---|---|
| 0–1 | `displayDuration` | seconds; 0 selects control-head default, otherwise 1–3600 |
| 2 | `textLength` | 0–128 bytes |
| 3… | `text` | exactly `textLength` bytes of well-formed UTF-8, no terminating NUL |

`parameterLength` shall equal `3 + textLength`. The sender shall not split a
UTF-8 code point at the 128-byte limit. After authentication, sensorRF shall
transfer the message through the CAN `driverTextTransfer` ISO-TP service. The
low 16 bits of `commandCounter` become the CAN `messageId`; sensorRF shall not
start another text transfer with the same ID while the previous mapping is
retained.

### Command response — packet type 6

Command responses have a fixed 27-byte payload:

| Payload byte | Field | Encoding |
|---:|---|---|
| 0 | `commandCode` | echoes request |
| 1–4 | `commandCounter` | echoes request |
| 5 | `targetProduct` | echoes request |
| 6 | `targetInstance` | echoes request |
| 7 | `responseStage` | 0 received, 1 downstream accepted, 2 completed |
| 8 | `resultCode` | result enumeration below |
| 9 | `detailCode` | downstream or sensorRF diagnostic |
| 10 | `downstreamTransactionId` | CAN transaction/message ID low byte; 255 unavailable |
| 11–26 | `authenticationTag` | first 16 bytes of HMAC-SHA-256 |

`resultCode`: 0 accepted, 1 completed, 2 rejected, 3 busy, 4 unsupported,
5 authentication failed, 6 replay rejected, 7 downstream timeout, 8 invalid
parameters, and 9 delivery failed. Responses shall set the application-header
`acknowledgement` flag. `received` confirms only radio validation and queueing;
`completed` reports the terminal CAN application result.

Authentication failures shall be recorded locally and shall not generate an
over-the-air response; result 5 is reserved for trusted local diagnostics.

### Authentication and replay protection

Commands are authenticated, not encrypted. The text and command parameters
remain visible over the air. Paddock-to-vehicle commands and vehicle-to-paddock
responses shall use distinct provisioned 256-bit HMAC keys. The authentication
tag is the first 16 bytes of HMAC-SHA-256 over the complete 16-byte application
header followed by the command or response payload excluding its tag. The
CRC-32C remains after the tag and is not included in the HMAC. Implementations
shall compare tags in constant time and shall not disclose whether an unknown
source or a bad tag caused authentication failure.

For each authorized paddock `sourceNode`, sensorRF shall retain the greatest
accepted `commandCounter` in nonvolatile storage with a wear-conscious journal.
A greater authenticated counter is a new command. An equal counter with the
same authenticated contents is a retry and receives the cached response
without another CAN operation. A lower counter, or an equal counter with
different contents, is rejected as replay. Counter wrap requires reprovisioning
before another command is accepted.

The paddock may retry an unanswered command after 500 ms, up to three total
transmissions, using the identical counter and authenticated contents. sensorRF
shall rate-limit authenticated commands to one new command per second with a
burst of three. Authentication, replay handling, downstream acknowledgement,
timeout behavior, and power-loss recovery shall be tested before command
reception is enabled in production.

## Compatibility rule

A receiver shall not decode an unknown schema. Packet types or TLV parameters
unknown within a recognized schema shall be rejected or skipped only as this
document explicitly permits.
