# SquareLine UI Export

Export the next SquareLine Studio UI into this directory.

Use these project settings:

- LVGL version: 8.3.x
- Resolution: 800 x 480
- Color depth: 16 bit
- Color swap: off

After export, update `src/main.cpp`:

- Add `#include "ui.h"`
- Replace `initPlaceholderUi();` with `ui_init();`
