# M5PaperS3 Lux / Env Slides

## Overview

This sketch runs on **M5PaperS3** and renders a portrait teaching dashboard for reading environmental change.

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

The microSD card is used for persistence and graph continuity.

- `/logs/env4_log.csv`
- `/logs/lux_log.csv`
- `/state/latest.json`

Current behavior:

- append CSV logs while running
- cache latest received values
- restore latest cached state on boot
- restore recent graph history from CSV on boot
- discard restored graph history if it is too old compared with the first live MQTT time

SD access is initialized with explicit SPI settings:

- PaperS3 SD CS: `GPIO_NUM_47`
- SPI clock: `40 MHz`

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

- number of rain-related signs currently observed
- a simple `Now` pattern line
- a simple `Rain pattern` reference line
- a short explanation line for daytime or night mode

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
- short meaning hint

The bottom interpretation band shows:

- rain-sign count
- current pattern summary
- rain-pattern reference
- a short daytime / night explanation
- a guiding question

### Slide 3: Short-Term

Shows short-term rolling graphs for:

- pressure
- humidity
- lux

It also shows:

- graph window start / end time
- graph midpoint time
- current values summary strip
- question: `What is changing now?`

Default target window:

- `15 min`

### Slide 4: Long-Term

Shows long-term rolling graphs for:

- pressure
- humidity
- lux

It also shows:

- graph window start / middle / end time
- current values summary strip
- question: `Is the trend continuing?`

Default target window:

- `120 min`

### Status Screen

Status is a separate auxiliary screen, not part of the normal 4-slide loop.

It shows two diagnostic cards:

- `HEALTH`
- `NETWORK`

## UI Notes

- Portrait layout: `setRotation(0)`
- UI language can be switched in `config.h`
  - English
  - Japanese
- Fonts are limited to:
  - `fonts::Font2`
  - `fonts::Font4`
  - `fonts::Font6`
- Japanese-capable font aliases are provided in [ja_assets.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ja_assets.h)
- UI text is centralized in [ui_text.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ui_text.h)
- A small built-in monochrome icon set is provided in [icons.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/icons.h)
- No SD-backed emoji or external icon font is required
- Header right side shows battery level as `BAT xx%`
- Footer left side shows JST time converted from received MQTT unix time
- If no valid live time has been received yet, the footer shows `--:--`
- Japanese text uses a separate drawing path from numeric text
  - numeric values keep the existing Latin fonts
  - labels and helper text use Japanese-capable fonts

## Navigation

- `BtnA`: previous slide
- `BtnB`: force redraw
- `BtnC`: next slide

Slides auto-rotate across the 4 teaching slides.

The footer center button opens:

- `STATUS` from the teaching slides
- `BACK` from the status screen

There is also a swipe-up gesture from the footer area to open the status screen.

## Night Mode

`Light` is not always used as a rain clue.

During night mode:

- rain signs use `pressure + humidity`
- the denominator changes from `/3` to `/2`
- `Light` is shown as skipped for interpretation

Night mode is enabled only when both are true:

- local time is after sunset or before sunrise
- low lux continues for a sustained period

Sunrise / sunset are estimated locally from the configured site position:

- `CONFIG_SITE_LATITUDE`
- `CONFIG_SITE_LONGITUDE`

Night mode is used only for interpretation.

- `Light` still appears as a sensor value and graph
- but it is removed from `Rain signs` counting when night conditions are confirmed

## Required Files

- [M5PaperS3-LuxEnv-Slides.ino](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/M5PaperS3-LuxEnv-Slides.ino)
- [config.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/config.h)
- [icons.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/icons.h)
- [ja_assets.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ja_assets.h)
- [ui_text.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ui_text.h)

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
- Build target used in this project:
  - `m5stack:esp32:m5stack_papers3`

## Current Focus

The sender side is already stable enough. The main focus is display quality:

- readability
- visual hierarchy
- slide-to-slide teaching flow
- helping the reader compare multiple clues together
