# ビルドメモ

## ボード設定

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
- 日本語 UI では [ja_assets.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ja_assets.h) の日本語フォント設定を使う
- 文言切替は [ui_text.h](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/ui_text.h) に集約している
- `config.h` の `CONFIG_UI_LANG` で英語 / 日本語を切り替える
- 日本語 UI では `LUX` の表現を `明るさ` に統一している
- アイコンは現在 `icons.h` の `32x32` モノクロビットマップを使う
- アイコン素材の出典とライセンスは [docs/third-party-assets.md](/Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides/docs/third-party-assets.md) に記録している
- 同一スライド更新時は、ヘッダ / フッタ枠を再利用し、スライド本文を中心に再描画する

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

日本語表示は実機で成立している。  
現在の方針:

- 数値・単位・時刻は既存の英数字フォントを維持
- ラベル・見出し・補助文は日本語描画ヘルパーを使う
- 今後の主課題はフォント不具合ではなく、日本語レイアウトの最終調整

## 補足

コンパイル例:

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_papers3 /Users/tomato/Documents/Arduino/M5PaperS3-LuxEnv-Slides
```

最近の変更:

- `icons.h` は `16x16` から `32x32` へ更新済み
- `drawMonoIcon()` は横方向の連続黒画素をまとめて描く
- `renderSlide()` はフル描画と本文中心再描画を切り替える構造へ更新済み
