# M5PaperS3 Lux / Env Slides

## Overview

This sketch runs on **M5PaperS3** and renders a portrait 4-slide teaching dashboard.

It is designed for a learning use case:

- read current outdoor and window-side conditions
- notice recent changes over time
- compare pressure, humidity, and light together
- think about whether rain may be getting closer

The goal is **not** automated forecasting. The goal is to help the reader infer natural changes from sensor data.

## MQTT Topics

The sketch subscribes to these topics:

- `env4`
- `home/env/lux/raw`
- `home/env/lux/meta`
- `home/env/lux/status`

Meaning:

- `env4`: outdoor temperature / humidity / pressure
- `home/env/lux/raw`: current lux
- `home/env/lux/meta`: lux average / delta / rate / trend
- `home/env/lux/status`: sensor and network health

## Storage

The microSD card is used for lightweight persistence and logging.

- `/logs/env4_log.csv`
- `/logs/lux_log.csv`
- `/state/latest.json`

Current behavior:

- append CSV logs while running
- cache latest received values
- restore latest cached state on boot

Not implemented:

- full history replay from CSV
- full graph reconstruction from SD on startup

## Slides

### Slide 1: Current

Shows the current values in a 2x2 grid:

- temperature
- humidity
- pressure
- lux

The lower block summarizes recent changes across:

- pressure
- humidity
- light

It also shows:

- lux rate versus recent average
- number of rain-related signs currently observed

### Slide 2: Signals

Shows the direction of recent change for:

- pressure
- humidity
- light

Each row combines:

- label
- state word
- small icon / arrow cue
- centered gauge
- short interpretation

The bottom band summarizes the current rain-sign count.

### Slide 3: Graphs

Shows simple rolling graphs for:

- pressure
- humidity
- lux

It also shows:

- graph window start / end time
- current values summary strip

### Slide 4: Status

Shows two diagnostic cards:

- `HEALTH`
- `NETWORK`

It is intended to stay quiet during normal operation and become obvious only when something is wrong.

## UI Notes

- Portrait layout: `setRotation(0)`
- English UI only
- Fonts are limited to:
  - `fonts::Font2`
  - `fonts::Font4`
  - `fonts::Font6`
- A small built-in monochrome icon set is provided in [icons.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/icons.h)
- No SD-backed emoji or external icon font is required

## Buttons

- `BtnA`: previous slide
- `BtnB`: force redraw
- `BtnC`: next slide

Slides also auto-rotate.

## Required Files

- [M5PaperS3-LuxEnv-Slides.ino](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/M5PaperS3-LuxEnv-Slides.ino)
- [config.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/config.h)
- [icons.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/icons.h)

You can copy [config.example.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/config.example.h) to `config.h` and fill in your settings.

## Required Libraries

- `M5Unified`
- `M5GFX`
- `PubSubClient`
- `ArduinoJson`

## Build Notes

- Board: `M5PaperS3`
- `#include <SD.h>` must stay before `#include <M5Unified.h>`
- The sketch is built around recent `M5Unified` / `M5GFX` versions and avoids unsupported fonts such as `Font5`

## Current Focus

The sensor sender side is already stable enough. The main focus is display quality:

- readability
- visual hierarchy
- slide-to-slide teaching flow
- helping the reader compare multiple clues together
