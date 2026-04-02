# M5PaperS3 Lux / Env Slides

## Overview

This sketch runs on **M5PaperS3** and shows a portrait teaching dashboard for reading environmental change.

The learning goal is:

- read current outdoor and window-side conditions
- notice recent changes over time
- compare pressure, humidity, and light together
- think about whether rain may be getting closer

This is **not** an automated forecast app. It is a teaching UI for observing clues and making simple inferences.

## Current UI State

- English UI exists as a stable baseline
- Japanese UI is now readable on device
- current work is final Japanese layout polishing
- slide roles are fixed and no longer changing significantly

## MQTT Topics

The sketch subscribes to:

- `env4`
- `home/env/lux/raw`
- `home/env/lux/meta`
- `home/env/lux/status`

Meaning:

- `env4`: outdoor temperature / humidity / pressure
- `home/env/lux/raw`: current lux
- `home/env/lux/meta`: lux average / delta / rate / trend
- `home/env/lux/status`: sensor and network health

See also: [docs/spec-topics.md](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/docs/spec-topics.md)

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

### Slide 1: Current / 現在値

Shows:

- current values in a 2x2 grid
- recent changes for pressure / humidity / light
- rain-sign count
- current pattern vs rain-pattern reference

### Slide 2: Signals / 変化のサイン

Shows:

- direction of recent change for pressure / humidity / light
- a centered gauge for each signal
- an interpretation card
- rain-sign count
- current pattern summary
- a reference rain pattern
- a short guiding question

### Slide 3: Short-Term / 短期傾向

Shows:

- short-term rolling graphs for pressure / humidity / lux
- current values summary strip
- question: `What is changing now?` / `今、何が変わっている？`

Target window:

- `15 min`

### Slide 4: Long-Term / 長期傾向

Shows:

- long-term rolling graphs for pressure / humidity / lux
- current values summary strip
- question: `Is the trend continuing?` / `流れは続いている？`

Target window:

- `120 min`

### Status Screen / 状態

Status is a separate auxiliary screen, not part of the normal 4-slide loop.

It shows two diagnostic cards:

- `HEALTH`
- `NETWORK`

## UI Notes

- Portrait layout: `setRotation(0)`
- UI language can be switched in `config.h`
  - English
  - Japanese
- UI text is centralized in [ui_text.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ui_text.h)
- Japanese-capable font aliases are provided in [ja_assets.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ja_assets.h)
- A small built-in monochrome icon set is provided in [icons.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/icons.h)
- Numeric text and Japanese text use separate drawing paths
  - numeric values keep the Latin fonts
  - labels and helper text use Japanese-capable drawing helpers
- Header right side shows battery level as `BAT xx%`
- Footer left side shows JST time converted from received MQTT unix time
- If no valid live time has been received yet, the footer shows `--:--`

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

Sunrise / sunset are estimated locally from:

- `CONFIG_SITE_LATITUDE`
- `CONFIG_SITE_LONGITUDE`

Night mode affects interpretation only.

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
- unsupported fonts such as `Font5` are avoided
- build target:
  - `m5stack:esp32:m5stack_papers3`

See also: [docs/build-notes.md](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/docs/build-notes.md)

## Current Focus

The main focus is no longer basic functionality. The main focus is:

- Japanese UI readability
- visual hierarchy
- slide-to-slide teaching flow
- helping the reader compare multiple clues together
