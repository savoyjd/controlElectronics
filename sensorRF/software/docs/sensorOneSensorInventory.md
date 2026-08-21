# sensorOne sensor inventory

Status: Schematic review, 2026-08-20

This note records the sensors and sensor interfaces found in the sensorOne
hardware so that common CAN messages can be designed deliberately. It is not
the sensorOne hardware specification. The source reviewed was the KiCad design
under `controlElectronics/sensorOne/hardware/sensorOnePcb`; that project was
read only during this review.

## Confirmed sensor functions

| Function | Channels | Implementation | Interface to MCU | Notes |
|---|---:|---|---|---|
| Thermocouple measurement | 2 | U3 and U4, MAX31856MUD+ | Shared SPI bus, separate chip selects | External differential inputs are `TC1+`/`TC1-` and `TC2+`/`TC2-`. Each converter also measures its cold-junction temperature and reports thermocouple faults. |
| Acceleration | 3 axes | U7, LIS3DHTR | SPI, chip select `SPI0_CS3` | Onboard accelerometer. INT1 and INT2 are not connected, so firmware must poll it unless the hardware changes. |
| Rotational speed sensing | 2 | External Littelfuse 55100-2M-02-A two-wire current-output Hall switches; one measures wheel speed and one measures shaft speed | MCU ADC inputs `PA17_A1_2` (channel 1) and `PA16_A1_1` (channel 2) through the input divider networks | Nets are named `HE_SENSE_1`, `HE_SENSE_2`, `HE_1_IN`, and `HE_2_IN`. The divider/load networks convert sensor loop current into ADC voltage and provide the ground return. The channel-to-function assignment and mechanical scaling are not yet defined. |
| Suspension displacement | 1 prototype channel | String displacement sensor; implementation not yet baselined | To be defined | CAN publication will report the latest value and the minimum and maximum of higher-rate samples accumulated over each observation window. |

The MAX31856 supports several thermocouple types, but the schematic does not
establish which type will be installed or configured. Thermocouple type is
therefore a software/configuration property, not a conclusion from this
review.

## Sensor connector

J2 is an eight-position sensor connector in the KiCad design. Its PCB pad
mapping is:

| Pin | Net | Purpose inferred from net name |
|---:|---|---|
| 1 | `TC1+` | Thermocouple channel 1 positive |
| 2 | `TC1-` | Thermocouple channel 1 negative |
| 3 | `+5V` | Hall/analog sensor supply |
| 4 | `HE_SENSE_1` | Hall/analog channel 1 signal |
| 5 | `+5V` | Hall/analog sensor supply |
| 6 | `HE_SENSE_2` | Hall/analog channel 2 signal |
| 7 | `TC2+` | Thermocouple channel 2 positive |
| 8 | `TC2-` | Thermocouple channel 2 negative |

No separate Hall-sensor ground conductor is required on J2. Each
55100-2M-02-A is a two-wire current-output device connected between its `+5V`
pin and `HE_SENSE_n`. The PCB input divider/load network completes the return
path to ground and converts loop current into a voltage for the MCU ADC.

The manufacturer's specified current bands are 5.0 to 6.9 mA with the Hall
switch off and 12.0 to 17.0 mA with it on. Its operating supply range is 3 to
24 VDC, switching rate is specified up to 12 kHz, and the `02` option denotes
a 300 mm cable. Firmware should classify the two non-overlapping current bands
rather than treating the ADC voltage as a continuous position measurement.

## CAN data that these functions will need

The following are candidate signals, not yet assigned CAN identifiers or
encodings:

- For each thermocouple channel: thermocouple temperature, cold-junction
  temperature, configured thermocouple type, validity, open-circuit and
  over/undervoltage fault status, and a sample counter or age.
- For the accelerometer: X, Y, and Z acceleration, validity, configured range,
  and a sample counter or timestamp. The ICD must define the transmitted axis
  frame; raw package axes alone are not sufficient for vehicle integration.
- For the wheel-speed and shaft-speed channels: detected state, validity,
  transition count, measured edge frequency or period, and input
  out-of-range/fault state. Raw ADC value or inferred loop current may also be
  carried diagnostically. Calibrated wheel speed and shaft speed require the
  channel assignment and mechanical scale factors described below.

## Items to resolve before changing the CAN schema

1. Assign `HE_SENSE_1` and `HE_SENSE_2` to wheel speed and shaft speed.
2. Establish pulses per revolution for both channels. For vehicle linear speed,
   also establish loaded tire circumference; for any speed on the opposite side
   of a transmission, establish the applicable gear ratio.
3. Decide whether the CAN interface publishes calibrated speed only or also
   publishes edge frequency/period and counts so consumers can operate before
   or independently of mechanical calibration. Calculate ADC thresholds from
   the as-built divider values and verify them over the sensor's 5.0--6.9 mA OFF
   and 12.0--17.0 mA ON current bands.
4. Select the thermocouple type and required temperature range for each
   channel.
5. Define the sensorOne PCB mounting orientation and vehicle/body coordinate
   convention for acceleration.
6. Select acquisition and CAN publication rates, acceptable latency, and
   behavior when a sample is repeated or invalid.
7. Confirm whether the manufacturing BOM is synchronized with the reviewed
   schematic and PCB revision.

## Reference

- [Littelfuse 55100 Hall Effect Sensor datasheet](https://www.littelfuse.com/assetdocs/littelfuse_hall_effect_sensors_55100_datasheet.pdf?assetguid=6d69d457-770e-46ba-9998-012c5e0aedd7),
  revision GD, 2023-05-16.
