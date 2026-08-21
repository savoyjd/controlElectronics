# sensor family CAN interface control document

Status: Draft 0.7  
Schema ID: `0x1A75`

The machine-readable source of truth is `schema/canSchema.json`. This document
explains behavior and integration requirements.

## Physical and data link layer

| Property | Value |
|---|---|
| CAN type | ISO 11898-1 Classic CAN 2.0 |
| Identifier | 11-bit standard |
| Bit rate | 500 kbit/s |
| Payload | 0–8 bytes |
| Byte order | Little-endian |
| Termination | 120 ohms at each physical end only |

The ESP32-S3 TWAI peripheral does not support CAN FD. Reserved bytes shall be
transmitted as zero and ignored on receipt unless a later schema assigns them.
Receivers shall use DLC and range validation before decoding.

## Identifier allocation

| Range | Purpose |
|---|---|
| `0x000–0x07F` | Network control and emergency — reserved |
| `0x080–0x0FF` | Discovery, schema, and time services |
| `0x100–0x17F` | Navigation |
| `0x180–0x2FF` | Sensor data — reserved |
| `0x300–0x3FF` | Commands and responses — reserved |
| `0x400–0x5FF` | Family expansion — reserved |
| `0x600–0x67F` | Diagnostics — reserved |
| `0x680–0x7FF` | Experimental; shall not appear in a release |

## Messages

### `0x080` schemaAnnouncement — 1 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `canSchemaId` | `uint16` |
| 2–3 | `loraSchemaId` | `uint16` |
| 4 | `productType` | 1 = sensorRF, 2 = sensorOne, 3 = controlHead |
| 5 | `hardwareRevision` | Product-specific numeric revision |
| 6 | `firmwareMajor` | Semantic firmware major version |
| 7 | `firmwareMinor` | Semantic firmware minor version |

Consumers shall compare schema identifiers before interpreting other frames.
A schema mismatch is diagnostic; it shall not cause transmission storms.

### `0x081` nodeIdentity — 1 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0 | `productType` | 1 sensorRF, 2 sensorOne, 3 controlHead |
| 1 | `nodeInstance` | configured logical instance, 0–254; 255 unassigned |
| 2 | `hardwareRevision` | product-specific numeric revision |
| 3 | `reserved` | zero |
| 4–7 | `deviceId` | stable product-specific 32-bit identifier |

All sensorOne nodes on one CAN bus shall have unique `nodeInstance` values.
`deviceId` is used for inventory and configuration diagnostics; it is not an
authentication credential.

### `0x100` navigationPosition — 10 Hz

Coordinates use the WGS 84 reference datum. Latitude and longitude are signed
decimal degrees scaled by 10,000,000 (E7 encoding). Positive latitude is
north, negative latitude is south, positive longitude is east, and negative
longitude is west.

| Byte | Field | Encoding |
|---:|---|---|
| 0–3 | `latitude` | signed 32-bit, degrees × 10,000,000 |
| 4–7 | `longitude` | signed 32-bit, degrees × 10,000,000 |

Valid encoded ranges are `-900000000` through `+900000000` for latitude and
`-1800000000` through `+1800000000` for longitude. Values outside these ranges
shall be rejected.

One latitude count is approximately 0.0111 m. One longitude count is
approximately `0.0111 × cos(latitude)` meters: 0.0111 m at the equator and
approximately 0.0085 m at 40 degrees latitude. These are encoding resolutions,
not statements of GNSS measurement accuracy.

Invalid data is indicated in `navigationMotion.solutionFlags`; sentinel
coordinates are not used.

### `0x101` navigationMotion — 10 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `groundSpeed` | unsigned, 0.01 m/s per bit |
| 2–3 | `courseOverGround` | unsigned, 0.01 degree per bit; `[0, 36000)` |
| 4–5 | `altitude` | signed, 0.1 m per bit |
| 6 | `solutionCounter` | increments for each new GNSS solution |
| 7 | `solutionFlags` | bit field below |

`solutionFlags`: bit 0 position valid, bit 1 velocity valid, bit 2 time valid,
bit 3 differential solution. Bits 4–7 are reserved and zero.

### `0x102` navigationQuality — 2 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `solutionAge` | milliseconds, saturates at 65535 |
| 2–3 | `horizontalDilution` | HDOP × 100; 65535 unavailable |
| 4 | `satellitesUsed` | count; 255 unavailable |
| 5 | `fixType` | 0 none, 1 dead reckoning, 2 2D, 3 3D |
| 6 | `gnssUpdateRate` | Hz × 10 |
| 7 | `reserved` | zero |

### `0x103` systemStatus — 1 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0–3 | `uptime` | seconds, wraps modulo 2³² |
| 4–5 | `supplyVoltage` | millivolts; 65535 unavailable |
| 6 | `resetReason` | family enumeration, initially ESP-IDF-compatible values |
| 7 | `statusFlags` | bit field below |

`statusFlags`: bit 0 GNSS present, bit 1 GNSS healthy, bit 2 LoRa healthy,
bit 3 CAN healthy, bit 4 configuration valid. Bits 5–7 are reserved and zero.

### `0x180` wheelSpeed — 20 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `vehicleSpeed` | unsigned, 0.01 m/s per bit; 65535 unavailable |
| 2–5 | `edgePeriod` | unsigned microseconds between the two most recent qualified edges; 4294967295 unavailable; saturates at 4294967294 |
| 6 | `edgeAge` | age of most recent qualified edge, 10 ms per bit; saturates at 255 |
| 7 | `speedFlags` | bit field below |

### `0x181` shaftSpeed — 20 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `shaftSpeed` | unsigned, 1 RPM per bit; 65535 unavailable |
| 2–5 | `edgePeriod` | unsigned microseconds between the two most recent qualified edges; 4294967295 unavailable; saturates at 4294967294 |
| 6 | `edgeAge` | age of most recent qualified edge, 10 ms per bit; saturates at 255 |
| 7 | `speedFlags` | bit field below |

For both speed messages, `speedFlags` is: bit 0 signal electrically valid,
bit 1 derived speed valid, bit 2 stopped, bit 3 period valid, bit 4 mechanical
calibration valid, and bit 5 input fault. Bits 6–7 are reserved and zero.

The raw period is the sensor-edge period, not necessarily one mechanical
revolution. Conversion to vehicle speed or shaft RPM uses the configured
pulses per revolution and, for vehicle speed, loaded tire circumference. A
derived speed shall be marked invalid when its required calibration is absent;
the raw period may remain valid.

#### Stationary and timeout behavior

Absence of an edge does not by itself indicate an electrical fault. Each
channel shall have a configured stop timeout appropriate to its minimum useful
speed and pulses per revolution. Before that timeout expires, the last derived
speed and period may be repeated, `stopped` shall be zero, and `edgeAge` shall
continue increasing. When the timeout expires with an otherwise valid input:

- the derived speed shall be transmitted as zero;
- `speedValid` and `stopped` shall both be one;
- `periodValid` shall be zero because the previous period no longer represents
  current rotational speed;
- `edgePeriod` shall retain the last measured period when one exists, allowing
  diagnostics to distinguish a normal stop from a never-observed signal; and
- transmission shall continue at 20 Hz.

If no complete edge interval has ever been observed since startup,
`edgePeriod` shall be 4294967295 and `periodValid` shall be zero. An input whose
current is outside both valid Hall-switch bands shall set `inputFault`, clear
`signalValid`, `speedValid`, and `stopped`, and use 65535 for derived speed.
Consequently, consumers shall never infer stationary state merely from a frame
timeout, an unavailable speed value, or an old period; they shall require both
`speedValid` and `stopped` with a received, fresh CAN frame.

### `0x182` suspensionDisplacement — 20 Hz

The displacement sensor shall be sampled at a rate appropriate to suspension
dynamics and independently of the 20 Hz CAN publication rate. The summary
frame preserves the observed extrema between publications.

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `latestDisplacement` | signed, 0.1 mm per bit; -32768 unavailable |
| 2–3 | `minimumDisplacement` | signed minimum over the observation window, 0.1 mm per bit; -32768 unavailable |
| 4–5 | `maximumDisplacement` | signed maximum over the observation window, 0.1 mm per bit; -32768 unavailable |
| 6 | `windowDuration` | actual observation-window duration in milliseconds; saturates at 255 |
| 7 | `displacementFlags` | bit field below |

Positive displacement direction and zero reference shall be defined by the
sensor installation. All three displacement fields use the same calibrated
coordinate system. Valid engineering values are -32767 through 32767 counts,
or -3276.7 through 3276.7 mm.

`displacementFlags`: bit 0 latest sample valid, bit 1 window extrema valid,
bit 2 mechanical calibration valid, bit 3 input fault, bit 4 window duration
saturated, and bit 5 one or more samples clipped to the encodable range. Bits
6–7 are reserved and zero.

The observation window begins immediately after the preceding summary is
successfully committed for transmission and ends when the current summary is
captured. Minimum and maximum shall include every valid high-rate sample in
that interval, including the sample used for `latestDisplacement`. Capturing a
frame and resetting the accumulators shall be atomic with respect to sensor
sampling. The new window shall be seeded with the most recent valid sample so
an edge sample cannot be lost at the boundary.

If CAN publication is delayed, accumulation shall continue rather than reset
at the nominal 50 ms boundary. `windowDuration` then reports the longer window;
bit 4 indicates durations of 255 ms or longer. If the window contains no valid
samples, both `windowValid` and `latestValid` shall be zero and all three
displacement fields shall be -32768. Consumers shall treat extrema from a
duration-saturated window as diagnostic rather than as a normal 20 Hz sample.

### `0x183` thermocouple1 — 10 Hz

### `0x184` thermocouple2 — 10 Hz

Both MAX31856 channels use the same payload definition:

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `thermocoupleTemperature` | signed, 0.1 °C per bit; -32768 unavailable |
| 2–3 | `coldJunctionTemperature` | signed, 0.1 °C per bit; -32768 unavailable |
| 4–5 | `sampleAge` | milliseconds since acquisition; saturates at 65535 |
| 6 | `thermocoupleFaults` | MAX31856-compatible fault bits below |
| 7 | `thermocoupleStatus` | configured type and validity bits below |

`thermocoupleFaults`: bit 0 open circuit, bit 1 over/undervoltage, bit 2
thermocouple low threshold, bit 3 thermocouple high threshold, bit 4
cold-junction low threshold, bit 5 cold-junction high threshold, bit 6
thermocouple range, and bit 7 cold-junction range. A set bit indicates the
fault is active or was latched for the reported sample, according to the
sensorOne acquisition configuration.

In `thermocoupleStatus`, bits 0–3 encode thermocouple type: 0 B, 1 E, 2 J, 3 K,
4 N, 5 R, 6 S, 7 T, and 15 unconfigured. Bit 4 indicates thermocouple
temperature valid, bit 5 cold-junction temperature valid, bit 6 converter
present, and bit 7 configuration valid. Unassigned type values 8–14 are
reserved. A nonzero fault byte does not automatically invalidate both
temperatures; the applicable validity bits are authoritative.

### `0x185` accelerationVector — 100 Hz

| Byte | Field | Encoding |
|---:|---|---|
| 0–1 | `accelerationX` | signed, 0.01 m/s² per bit; -32768 unavailable |
| 2–3 | `accelerationY` | signed, 0.01 m/s² per bit; -32768 unavailable |
| 4–5 | `accelerationZ` | signed, 0.01 m/s² per bit; -32768 unavailable |
| 6 | `sampleCounter` | increments modulo 256 for each newly acquired sample |
| 7 | `accelerationFlags` | bit field below |

`accelerationFlags`: bit 0 data valid, bit 1 sensor sample overrun, bit 2 one
or more axes clipped to the encoded or configured sensor range, bit 3
calibration valid, and bit 4 coordinate frame valid. Bits 5–7 are reserved and
zero.

Acceleration is expressed in SI units and includes gravity. X, Y, and Z shall
use the family vehicle/body coordinate frame, not unqualified LIS3DH package
axes. Until the PCB installation transform and body-frame convention are
configured, `coordinateFrameValid` shall be zero. A receiver may log those
samples but shall not interpret their directions as vehicle axes.

### `0x300` sensorCalibrationCommand — event driven

| Byte | Field | Encoding |
|---:|---|---|
| 0 | `commandCode` | 1 capture level; 2 zero suspension displacement |
| 1 | `transactionId` | requester-selected value, incremented modulo 256 for each new operation |
| 2 | `targetInstance` | configured sensorOne node instance, 0–254; 255 broadcast is prohibited |
| 3 | `commandFlags` | bit 0 persist result; bits 1–7 reserved and zero |
| 4–5 | `commandParameter` | command-specific signed 16-bit value below |
| 6–7 | `confirmationToken` | command-specific 16-bit value below |

The confirmation token guards against executing a valid but unintended command;
it is not authentication. `captureLevel` requires `0x564C` (bytes `L`, `V` in
little-endian order). `zeroSuspensionDisplacement` requires `0x455A` (bytes
`Z`, `E`). A node shall reject a command addressed to another instance or
containing reserved flag bits.

`captureLevel` instructs sensorOne to average stationary accelerometer samples,
validate their stability and gravity magnitude, and calculate the gravity-based
portion of the sensor-to-body transform. For this command, `commandParameter`
is `sensorYawOffset`: a signed angle in 0.01 degree per bit, valid from -18000
through +18000. It is the rotation from vehicle-forward (+X body) to the
projection of the sensor's +X axis onto the level plane. Positive rotation is
counterclockwise when viewed from above the vehicle (+Z body looking toward
the origin). Values outside this range shall be rejected.

The body-frame convention for this operation is +X forward, +Y left, and +Z
up. Gravity measurements establish roll and pitch; `sensorYawOffset` supplies
the otherwise unobservable yaw relationship and thereby completes the
sensor-to-body rotation. It is a relative mounting angle, not an absolute
magnetic or true vehicle heading. The sensor's +X axis must have a usable
projection onto the level plane; an installation with +X nearly vertical
requires a different configured reference axis and shall fail validation.

`zeroSuspensionDisplacement` instructs sensorOne to average stable displacement
samples and store their mean as the neutral-position offset. It shall fail
validation if the input is invalid, clipped, or insufficiently stable. Setting
`persist result` requests nonvolatile storage; completion shall not be reported
until that storage operation succeeds. Its `commandParameter` is reserved and
shall be zero.

### `0x301` sensorCalibrationResponse — event driven

| Byte | Field | Encoding |
|---:|---|---|
| 0 | `commandCode` | echoes request |
| 1 | `transactionId` | echoes request |
| 2 | `responderInstance` | responding sensorOne instance |
| 3 | `resultCode` | enumeration below |
| 4 | `calibrationState` | 0 uncalibrated, 1 collecting, 2 valid, 3 failed |
| 5 | `detailCode` | command-specific diagnostic; zero when not applicable |
| 6–7 | `reserved` | zero |

`resultCode`: 0 accepted, 1 completed, 2 rejected, 3 busy, 4 unsupported,
5 invalid token, 6 validation failed, and 7 nonvolatile-storage failure.
Long-running commands shall first respond `accepted` and later respond
`completed` or a terminal failure using the same transaction ID.

Requests may be retried after 100 ms when no response is received, for up to
three total transmissions. The addressed node shall make `(commandCode,
transactionId)` idempotent for at least 5 seconds: a duplicate shall return the
latest response and shall not restart averaging, change the zero again, or
perform another nonvolatile write. The system integrator shall assign unique
sensorOne instance values; calibration broadcasts are prohibited to prevent
simultaneous conflicting responses and unintended multi-node calibration.

### Driver text transport — ISO-TP on `0x310`/`0x311`

Text messages from the paddock exceed one Classic CAN frame. sensorRF and the
control head shall use ISO 15765-2 normal addressing with 11-bit CAN identifiers:

| Identifier | Direction | Use |
|---:|---|---|
| `0x310` | sensorRF to control head | Driver-text request and flow control for a response |
| `0x311` | control head to sensorRF | Flow control for a request and driver-text response |

CAN data bytes not used by an ISO-TP frame shall be padded with zero. The
receiver shall support the full 135-byte request PDU. ISO-TP timing, sequence,
flow-control, and abort behavior shall conform to ISO 15765-2; an incomplete
transfer shall never be displayed.

The reassembled `driverTextTransfer` PDU is:

| PDU byte | Field | Encoding |
|---:|---|---|
| 0 | `messageVersion` | 1 |
| 1–2 | `messageId` | source-selected value used for acknowledgement and duplicate suppression |
| 3 | `messageFlags` | bit 0 urgent, bit 1 clear existing message first; bits 2–7 zero |
| 4–5 | `displayDuration` | seconds; 0 selects the control-head default; otherwise 1–3600 |
| 6 | `textLength` | 0–128 UTF-8 bytes |
| 7… | `text` | exactly `textLength` bytes of well-formed UTF-8, without a terminating NUL |

The PDU length shall equal `7 + textLength`. The control head shall reject
malformed UTF-8, embedded control characters other than space, or reserved
flags. A sender shortening text to 128 bytes shall truncate only at a UTF-8
code-point boundary. Display rendering and line wrapping are control-head
responsibilities.

After reassembly and validation, the control head shall return a six-byte
`driverTextResponse` PDU on `0x311`:

| PDU byte | Field | Encoding |
|---:|---|---|
| 0 | `messageVersion` | 1 |
| 1–2 | `messageId` | echoes request |
| 3 | `resultCode` | 0 accepted for display, 1 displayed, 2 rejected, 3 busy, 4 unsupported |
| 4 | `detailCode` | implementation diagnostic; zero when not applicable |
| 5 | `reserved` | zero |

The control head shall retain the most recent accepted `messageId` for at
least 30 seconds. A duplicate transfer shall return the latest response but
shall not restart the display duration or enqueue another copy. ISO-TP success
only proves transport delivery; the application response proves validation and
display disposition.

## Timing and freshness

Periodic transmitters should distribute frame phases to avoid bursts. A
consumer shall consider position and motion stale when `solutionAge` exceeds
its application limit. Initial control-head guidance is 500 ms; this is not a
substitute for checking validity flags.

Classic CAN provides link-layer CRC and retransmission. This draft therefore
does not add an application CRC to each eight-byte frame. Consumers shall use
range, cadence, counter, and timeout checks for plausibility.

## Compatibility rule

Nodes with an unknown `canSchemaId` may record raw frames but shall not assume
field compatibility. Additive behavior that changes any transmitted field,
enumeration, cadence contract, or identifier allocation requires a new schema.
