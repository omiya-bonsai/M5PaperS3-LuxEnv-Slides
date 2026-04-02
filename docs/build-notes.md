# Build Notes

## Board設定

- Board: `M5PaperS3`
- FQBN:

```text
m5stack:esp32:m5stack_papers3
```

- Display: 縦置き
- SD 初期化:

```cpp
SD.begin(GPIO_NUM_47, SPI, 40000000)
```

## 使用ライブラリ

- `M5Unified`
- `M5GFX`
- `SD`
- `WiFi`
- `PubSubClient`
- `ArduinoJson`

補足:

- `#include <SD.h>` は `#include <M5Unified.h>` より前に置く

## 既知のビルドエラー

### `Font5` is not a member of `fonts`

過去に発生した既知問題。

原因:

- 使用環境の `M5Unified 0.2.13`
- 使用環境の `M5GFX 0.2.19`
- `fonts::Font5` が存在しない

対応方針:

- `Font2`
- `Font4`
- `Font6`

の 3 種類へ統一する。

### 日本語表示について

コンパイル自体は通っても、実機では日本語が豆腐文字になるケースがある。  
これはビルドエラーではなく、フォント適用と描画経路の問題として扱う。

## 補足

コンパイル例:

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_papers3 /Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides
```
