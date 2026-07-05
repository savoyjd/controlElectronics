#!/usr/bin/env python3
"""Send mock sensor-pod frames to the dash over a serial port.

Frame format consumed by the firmware:
  SPOD,cvtF,auxF,mph,rpm,rollDeg,pitchDeg

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


def field(value, missing=False):
    if missing:
        return "-"
    return f"{value:.2f}"


def make_frame(elapsed_s, args):
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

    missing = args.missing_period > 0.0 and elapsed_s % args.missing_period < args.missing_duration

    return "SPOD,{},{},{},{},{},{}\n".format(
        field(cvt_f, missing),
        field(aux_f, missing),
        field(speed_mph, missing),
        field(rpm, missing),
        field(roll_deg, missing),
        field(pitch_deg, missing),
    )


def parse_args():
    parser = argparse.ArgumentParser(description="Stream mock sensor-pod data to dashOne.")
    parser.add_argument("--port", default="COM11", help="Serial port connected to the panel.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument("--rate", type=float, default=100.0, help="Frames per second to send.")
    parser.add_argument("--duration", type=float, default=0.0, help="Seconds to run; 0 runs until Ctrl+C.")
    parser.add_argument("--echo", action="store_true", help="Print each frame as it is sent.")

    parser.add_argument("--fixed", action="store_true", help="Send fixed values instead of animated values.")
    parser.add_argument("--cvt", type=float, default=180.0, help="Fixed CVT temperature in deg F.")
    parser.add_argument("--aux", type=float, default=125.0, help="Fixed aux temperature in deg F.")
    parser.add_argument("--speed", type=float, default=32.0, help="Fixed wheel speed in MPH.")
    parser.add_argument("--rpm", type=float, default=3000.0, help="Fixed wheel RPM.")
    parser.add_argument("--roll", type=float, default=8.0, help="Fixed roll angle in degrees.")
    parser.add_argument("--pitch", type=float, default=-4.0, help="Fixed pitch angle in degrees.")

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


def main():
    args = parse_args()
    if serial is None:
        print("pyserial is required. Install it with: python -m pip install pyserial", file=sys.stderr)
        return 2

    if args.rate <= 0.0:
        print("--rate must be greater than zero", file=sys.stderr)
        return 2

    period_s = 1.0 / args.rate
    start_s = time.monotonic()
    next_send_s = start_s

    with serial.Serial(args.port, args.baud, timeout=0.0, write_timeout=1.0) as port:
        print(f"Streaming mock sensor data on {args.port} at {args.baud} baud, {args.rate:g} Hz")
        try:
            while True:
                now_s = time.monotonic()
                elapsed_s = now_s - start_s
                if args.duration > 0.0 and elapsed_s >= args.duration:
                    break

                if now_s >= next_send_s:
                    frame = make_frame(elapsed_s, args)
                    port.write(frame.encode("ascii"))
                    if args.echo:
                        print(frame.rstrip())
                    next_send_s += period_s
                else:
                    time.sleep(min(0.002, next_send_s - now_s))
        except KeyboardInterrupt:
            print("\nStopped")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
