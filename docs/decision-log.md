# Decision Log

## Purpose

This file records major design decisions for the M5PaperS3 Lux / Env Slides project.
The goal is to preserve why key choices were made, not just what the current code does.

## Decision History

### 1. Keep the project focused on teaching, not forecasting

- Decision:
  - Treat the UI as a teaching tool for observing environmental clues.
  - Do not turn it into a high-confidence automated rain prediction system.
- Reason:
  - The core learning goal is to help a middle-school student compare pressure, humidity, and light and think about what they mean together.

### 2. Use a portrait layout

- Decision:
  - Use portrait orientation on M5PaperS3.
- Reason:
  - The content is read top-to-bottom more naturally than left-to-right.
  - Portrait layout made it easier to assign one role to each slide and avoid wasted right-side space.

### 3. Separate the UI into 4 teaching slides plus an auxiliary status screen

- Decision:
  - Normal slide loop:
    1. Current
    2. Signals
    3. Short-term
    4. Long-term
  - Status is not part of the normal loop.
- Reason:
  - The first four slides are for teaching flow.
  - Status is operational information and should not interrupt the learning sequence.

### 4. Keep `Status` as a footer-accessed auxiliary screen

- Decision:
  - Open `Status` only from the footer button or swipe-up gesture.
- Reason:
  - Status information is useful, but lower priority than the learning slides.
  - This keeps the main loop focused on observation and inference.

### 5. Use short-term and long-term graph windows separately

- Decision:
  - Slide 3 shows short-term trends over 15 minutes.
  - Slide 4 shows long-term trends over 120 minutes.
- Reason:
  - Short-term trends help answer “What is changing now?”
  - Long-term trends help answer “Is the trend continuing?”

### 6. Restore graph history from CSV logs at boot

- Decision:
  - Use SD CSV logs to rebuild recent graph history after reboot or reflashing.
- Reason:
  - Without CSV restore, graphs would always start empty after flashing.
  - CSV restore gives continuity for short-term and long-term slides.

### 7. Discard restored CSV history when it is too old

- Decision:
  - Compare restored history with the first live MQTT time and discard stale graph history if the gap is too large.
- Reason:
  - Old graph data can be misleading after the device has been powered off for a long time.
  - It is better to show an empty graph than imply that stale history is current.

### 8. Use received Unix time for footer time display

- Decision:
  - Use live MQTT `unix_time` / `ts` for footer time, converted to JST.
  - Do not wait for M5PaperS3 local NTP stabilization before showing time.
- Reason:
  - Sender-side timestamps are already meaningful and arrive earlier than a stable local clock.
  - This avoids long `--:--` periods after boot.

### 9. Keep Wi-Fi because MQTT is required

- Decision:
  - Keep Wi-Fi enabled even if local NTP is not the main time source.
- Reason:
  - MQTT depends on Wi-Fi in the current architecture.
  - Power saving is lower priority because the device is expected to remain powered.

### 10. Use SD at explicit 40 MHz SPI

- Decision:
  - Initialize SD with explicit CS and SPI speed:
    - `GPIO_NUM_47`
    - `40000000`
- Reason:
  - SD stability is better when the SPI configuration is explicit.

### 11. Night mode should exclude light from rain-sign counting

- Decision:
  - Exclude `Light` from `Rain signs` at night.
  - Change denominator from `/3` to `/2` during confirmed night mode.
- Reason:
  - After sunset, low lux is normal and should not be interpreted as a rain clue.

### 12. Use time + sustained low lux for night mode

- Decision:
  - Night mode requires:
    - local time in the night band
    - sustained low lux for a period
- Reason:
  - Time alone is too rigid.
  - Lux alone is too noisy.
  - Combining both reduces false night-mode activation.

### 13. Estimate sunrise and sunset locally from configured coordinates

- Decision:
  - Compute sunrise and sunset in the sketch using configured site latitude and longitude.
- Reason:
  - This avoids adding another online dependency or API just for day/night interpretation.

### 14. Keep location in `config.h`

- Decision:
  - Move site latitude and longitude into configuration.
- Reason:
  - The project should not hardcode a specific site in the main sketch.

### 15. Use monochrome built-in icons instead of emoji fonts

- Decision:
  - Use `icons.h` with monochrome symbols rather than external emoji fonts.
- Reason:
  - Emoji/font support is unstable and heavy in this environment.
  - The built-in icons are lighter and better suited to E-Ink.

### 16. Simplify `LUX RATE` out of the main teaching UI

- Decision:
  - Remove `LUX RATE` from the main UI emphasis.
- Reason:
  - It added calculation-heavy information but did not help intuitive understanding as much as direction and pattern did.

### 17. Prefer “current pattern vs rain pattern” over raw symbolic shorthand

- Decision:
  - Show a compact current pattern and a reference rain pattern rather than only symbolic shorthand like `P v H ^ L v`.
- Reason:
  - Direct comparison is easier for students than decoding compact notation alone.

### 18. Keep English and Japanese in the same repository

- Decision:
  - Maintain both English and Japanese UI modes in the same repo.
- Reason:
  - The logic is shared.
  - Separate repos would duplicate maintenance and increase drift risk.

### 19. Localize in phases

- Decision:
  - Proceed in phases:
    1. text table extraction
    2. Japanese text set
    3. Japanese-capable drawing path
    4. Japanese layout tuning
- Reason:
  - This makes it easier to isolate issues and keeps the English baseline stable.

### 20. Use separate drawing paths for numeric values and Japanese text

- Decision:
  - Keep Latin numeric fonts for numbers, units, and timestamps.
  - Use Japanese-capable drawing for labels and helper text.
- Reason:
  - Numeric readability was already good.
  - Japanese text required a different rendering path.

## Current Direction

The current development focus is:

- finishing Japanese UI readability
- tightening Slide 1 and Slide 2 helper areas
- keeping Slide 3 and Slide 4 stable
- maintaining the project as a learning tool rather than expanding operational features
