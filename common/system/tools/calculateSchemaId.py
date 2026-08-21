#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def canonicalJson(document: object) -> bytes:
    return json.dumps(
        document,
        ensure_ascii=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")


def crc16CcittFalse(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Calculate a sensorRF 16-bit schema identifier."
    )
    parser.add_argument("manifest", type=Path)
    arguments = parser.parse_args()

    with arguments.manifest.open("r", encoding="utf-8-sig") as manifestFile:
        document = json.load(manifestFile)

    schemaId = crc16CcittFalse(canonicalJson(document))
    print(f"0x{schemaId:04X}")


if __name__ == "__main__":
    main()
