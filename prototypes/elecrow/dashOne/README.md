# ELECROW SquareLine PlatformIO Template

This project is derived from the working ELECROW 5 inch RGB display example.

The hardware/display setup is kept in:

- `src/main.cpp`
- `include/LovyanGFX_Driver.h`
- `include/pins_config.h`
- `boards/ESP32-S3-WROOM-1-N16R8.json`
- `platformio.ini`

The current `src/ui/ui.c` and `src/ui/ui.h` are only placeholders so the project builds before a SquareLine Studio export is added.

When exporting from SquareLine Studio for LVGL 8.x, copy/replace the generated UI source and header files into `src/ui/` so the generated UI stays isolated from the hardware/display setup.
