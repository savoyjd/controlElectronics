#!/usr/bin/env python3
"""Generate dashboard needle PNG assets."""

import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None


NEEDLE_WIDTH_PX = 5
NEEDLE_LENGTH_PX = 125
PALE_OUTLINE_FRACTION = 0.80

BACKGROUND_COLOR = (0, 0, 0)
RED_OUTLINE_COLOR = (214, 24, 24)

NEEDLES = {
    "orange": {
        "fill": (242, 123, 31),
        "outline": (255, 185, 112),
    },
    "blue": {
        "fill": (40, 138, 230),
        "outline": (132, 201, 255),
    },
}


def draw_needle(fill_color, pale_outline_color):
    image = Image.new("RGB", (NEEDLE_WIDTH_PX, NEEDLE_LENGTH_PX), BACKGROUND_COLOR)
    pixels = image.load()
    red_outline_end_y = NEEDLE_LENGTH_PX - round(NEEDLE_LENGTH_PX * PALE_OUTLINE_FRACTION)

    for y in range(NEEDLE_LENGTH_PX):
        outline_color = RED_OUTLINE_COLOR if y < red_outline_end_y else pale_outline_color
        pixels[0, y] = outline_color
        pixels[NEEDLE_WIDTH_PX - 1, y] = outline_color

        for x in range(1, NEEDLE_WIDTH_PX - 1):
            pixels[x, y] = fill_color

    return image


def parse_args():
    parser = argparse.ArgumentParser(description="Generate orange and blue dashboard needle PNGs.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "generated_needles",
        help="Directory where PNGs will be written.",
    )
    return parser.parse_args()


def main():
    if Image is None:
        print("Pillow is required. Install it with: python -m pip install pillow")
        return 2

    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    for name, colors in NEEDLES.items():
        image = draw_needle(colors["fill"], colors["outline"])
        output_path = args.output_dir / f"{name}_needle_{NEEDLE_WIDTH_PX}x{NEEDLE_LENGTH_PX}.png"
        image.save(output_path)
        print(output_path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
