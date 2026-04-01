# M5PaperS3 MQTT Slide Dashboard

## Purpose

This sketch runs on **M5PaperS3** and subscribes to:

- `env4`
- `home/env/lux/raw`
- `home/env/lux/meta`
- `home/env/lux/status`

It renders a simple 4-slide dashboard and uses the **microSD card** as storage for:

- `/logs/env4_log.csv`
- `/logs/lux_log.csv`
- `/state/latest.json`

## Slides

1. Current values
2. Signal arrows
3. Simple graphs
4. Sensor / MQTT status

## Buttons

- **BtnA**: previous slide
- **BtnB**: force redraw
- **BtnC**: next slide

Slides also auto-rotate.

## Notes

- This version is intentionally **English-only**.
- It avoids custom Japanese font work for now.
- It uses SD for persistent latest-state caching and CSV logging.
- The graph window is a simple rolling in-memory history loaded during runtime. The SD card is used for persistence/logging, not full history replay.

## Required libraries

- `M5Unified`
- `PubSubClient`
- `ArduinoJson`

## Important

On M5Unified / M5GFX projects, the M5 docs note that `#include <SD.h>` should come **before** `#include <M5Unified.h>` in code that uses SD-backed graphics or storage. PaperS3 Arduino development uses the `M5PaperS3` board with recent `M5Unified` and `M5GFX` versions, and the board includes a microSD interface. citeturn230194search0turn230194search12turn230194search14turn230194search15
