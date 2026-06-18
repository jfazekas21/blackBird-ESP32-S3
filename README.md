# blackBird ESP32-S3

Blank ESP-IDF project targeting the ESP32-S3.

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) installed and the environment exported (`export.ps1` / `export.sh`).

## Build & flash

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

## Layout

```
.
├── CMakeLists.txt        # Top-level project definition
├── sdkconfig.defaults    # Default config (target = esp32s3)
└── main/
    ├── CMakeLists.txt     # Component registration
    └── main.c             # app_main() entry point
```
