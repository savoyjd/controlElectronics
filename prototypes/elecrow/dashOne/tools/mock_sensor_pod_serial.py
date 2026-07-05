#!/usr/bin/env python3
"""Send mock sensor-pod and RF-pod frames to the dash over a serial port.

Frame formats consumed by the firmware:
  SPOD,cvtF,auxF,mph,rpm,rollDeg,pitchDeg
  RFOD,gpsSpeedMph,lat,lon,gpsFix,lapCurrentMs,lapLastMs,lapBestMs,currentTime

Use '-' for a missing value.
"""

import argparse
import math
import sys
import time

try:
    import serial
except ImportError:
    serial = None


def field(value, missing=False, precision=2):
    if missing:
        return "-"
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return value
    return f"{value:.{precision}f}"


def missing_window(elapsed_s, args):
    return args.missing_period > 0.0 and elapsed_s % args.missing_period < args.missing_duration


def make_spod_frame(elapsed_s, args):
    if args.fixed:
        cvt_f = args.cvt
        aux_f = args.aux
        speed_mph = args.speed
        rpm = args.rpm
        roll_deg = args.roll
        pitch_deg = args.pitch
    else:
        cvt_f = 165.0 + 35.0 * math.sin(elapsed_s * 0.21)
        aux_f = 120.0 + 20.0 * math.sin(elapsed_s * 0.17 + 1.3)
        speed_mph = 30.0 + 25.0 * math.sin(elapsed_s * 0.41)
        rpm = speed_mph * 95.0
        roll_deg = 22.0 * math.sin(elapsed_s * 0.73)
        pitch_deg = 14.0 * math.sin(elapsed_s * 0.57 + 0.6)

    missing = missing_window(elapsed_s, args)

    return "SPOD,{},{},{},{},{},{}\n".format(
        field(cvt_f, missing),
        field(aux_f, missing),
        field(speed_mph, missing),
        field(rpm, missing),
        field(roll_deg, missing),
        field(pitch_deg, missing),
    )


def make_rfod_frame(elapsed_s, args):
    if args.rf_fixed:
        gps_speed_mph = args.gps_speed
        lat = args.lat
        lon = args.lon
        gps_fix = args.gps_fix
        lap_current_ms = args.lap_current_ms
        lap_last_ms = args.lap_last_ms
        lap_best_ms = args.lap_best_ms
    else:
        gps_speed_mph = 29.0 + 24.0 * math.sin(elapsed_s * 0.39 + 0.2)
        lat = args.lat + 0.0004 * math.sin(elapsed_s * 0.035)
        lon = args.lon + 0.0004 * math.cos(elapsed_s * 0.035)
        gps_fix = True
        lap_period_ms = max(1, args.lap_period_ms)
        lap_current_ms = int((elapsed_s * 1000.0) % lap_period_ms)
        lap_last_ms = args.lap_last_ms + int(650.0 * math.sin(elapsed_s * 0.09))
        lap_best_ms = args.lap_best_ms

    missing = missing_window(elapsed_s, args)
    current_time = args.rf_time if args.rf_time is not None else time.strftime("%H:%M:%S")

    return "RFOD,{},{},{},{},{},{},{},{}\n".format(
        field(gps_speed_mph, missing),
        field(lat, missing, precision=6),
        field(lon, missing, precision=6),
        field(gps_fix, missing),
        field(int(lap_current_ms), missing),
        field(int(lap_last_ms), missing),
        field(int(lap_best_ms), missing),
        field(current_time, missing),
    )


def parse_args():
    parser = argparse.ArgumentParser(description="Stream mock pod data to dashOne.")
    parser.add_argument("--port", default="COM11", help="Serial port connected to the panel.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument("--rate", type=float, default=None, help="Legacy alias for --spod-rate.")
    parser.add_argument("--spod-rate", type=float, default=100.0, help="Sensor-pod frames per second; 0 disables SPOD.")
    parser.add_argument("--rfod-rate", type=float, default=10.0, help="RF-pod frames per second; 0 disables RFOD.")
    parser.add_argument("--duration", type=float, default=0.0, help="Seconds to run; 0 runs until Ctrl+C.")
    parser.add_argument("--echo", action="store_true", help="Print each frame as it is sent.")
    parser.add_argument("--settle", type=float, default=1.0, help="Seconds to wait after opening the port before sending.")

    parser.add_argument("--fixed", action="store_true", help="Send fixed SPOD values instead of animated values.")
    parser.add_argument("--cvt", type=float, default=180.0, help="Fixed CVT temperature in deg F.")
    parser.add_argument("--aux", type=float, default=125.0, help="Fixed aux temperature in deg F.")
    parser.add_argument("--speed", type=float, default=32.0, help="Fixed wheel speed in MPH.")
    parser.add_argument("--rpm", type=float, default=3000.0, help="Fixed wheel RPM.")
    parser.add_argument("--roll", type=float, default=8.0, help="Fixed roll angle in degrees.")
    parser.add_argument("--pitch", type=float, default=-4.0, help="Fixed pitch angle in degrees.")

    parser.add_argument("--rf-fixed", action="store_true", help="Send fixed RFOD values instead of animated values.")
    parser.add_argument("--gps-speed", type=float, default=31.0, help="Fixed GPS speed in MPH.")
    parser.add_argument("--lat", type=float, default=42.2931, help="Base/fixed GPS latitude.")
    parser.add_argument("--lon", type=float, default=-83.7158, help="Base/fixed GPS longitude.")
    parser.add_argument("--gps-fix", action=argparse.BooleanOptionalAction, default=True, help="Whether fixed RFOD data has GPS fix.")
    parser.add_argument("--lap-current-ms", type=int, default=45230, help="Fixed current lap time in ms.")
    parser.add_argument("--lap-last-ms", type=int, default=73120, help="Fixed last lap time in ms.")
    parser.add_argument("--lap-best-ms", type=int, default=70540, help="Fixed best lap time in ms.")
    parser.add_argument("--lap-period-ms", type=int, default=75000, help="Animated current lap period in ms.")
    parser.add_argument("--rf-time", default=None, help="Fixed RFOD current time as HH:MM:SS; defaults to the computer clock.")

    parser.add_argument(
        "--missing-period",
        type=float,
        default=0.0,
        help="Seconds between missing-data windows; 0 disables missing-data injection.",
    )
    parser.add_argument(
        "--missing-duration",
        type=float,
        default=0.5,
        help="Seconds to send '-' fields during each missing-data window.",
    )
    return parser.parse_args()


def validate_time_text(value):
    return len(value) == 8 and value[2] == ":" and value[5] == ":" and all(
        ch.isdigit() for index, ch in enumerate(value) if index not in (2, 5)
    )


def validate_rate(name, value):
    if value < 0.0:
        print(f"{name} must be greater than or equal to zero", file=sys.stderr)
        return False
    return True


def main():
    args = parse_args()
    if serial is None:
        print("pyserial is required. Install it with: python -m pip install pyserial", file=sys.stderr)
        return 2

    if args.rate is not None:
        args.spod_rate = args.rate

    if not validate_rate("--spod-rate", args.spod_rate) or not validate_rate("--rfod-rate", args.rfod_rate):
        return 2

    if args.rf_time is not None and not validate_time_text(args.rf_time):
        print("--rf-time must use HH:MM:SS", file=sys.stderr)
        return 2

    if args.spod_rate == 0.0 and args.rfod_rate == 0.0:
        print("At least one of --spod-rate or --rfod-rate must be greater than zero", file=sys.stderr)
        return 2

    with serial.Serial(args.port, args.baud, timeout=0.0, write_timeout=1.0, dsrdtr=False, rtscts=False) as port:
        port.dtr = False
        port.rts = False
        port.reset_input_buffer()
        port.reset_output_buffer()
        print(
            f"Streaming mock data on {args.port} at {args.baud} baud; "
            f"SPOD {args.spod_rate:g} Hz, RFOD {args.rfod_rate:g} Hz"
        )
        print(f"DTR={port.dtr} RTS={port.rts}; waiting {args.settle:g}s before first frame")
        time.sleep(args.settle)

        start_s = time.monotonic()
        next_spod_s = start_s if args.spod_rate > 0.0 else None
        next_rfod_s = start_s if args.rfod_rate > 0.0 else None
        spod_period_s = 1.0 / args.spod_rate if args.spod_rate > 0.0 else None
        rfod_period_s = 1.0 / args.rfod_rate if args.rfod_rate > 0.0 else None

        try:
            while True:
                now_s = time.monotonic()
                elapsed_s = now_s - start_s
                if args.duration > 0.0 and elapsed_s >= args.duration:
                    break

                sent = False
                if next_spod_s is not None and now_s >= next_spod_s:
                    frame = make_spod_frame(elapsed_s, args)
                    port.write(frame.encode("ascii"))
                    if args.echo:
                        print(frame.rstrip())
                    next_spod_s += spod_period_s
                    sent = True

                if next_rfod_s is not None and now_s >= next_rfod_s:
                    frame = make_rfod_frame(elapsed_s, args)
                    port.write(frame.encode("ascii"))
                    if args.echo:
                        print(frame.rstrip())
                    next_rfod_s += rfod_period_s
                    sent = True

                if not sent:
                    wakeups = [t for t in (next_spod_s, next_rfod_s) if t is not None]
                    sleep_s = max(0.0, min(wakeups) - now_s) if wakeups else 0.002
                    time.sleep(min(0.002, sleep_s))
        except KeyboardInterrupt:
            print("\nStopped")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())