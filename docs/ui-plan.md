# UI Plan

## Slide 1～4 の役割

### Slide 1

- 役割: 現在値と最近の変化要約
- 表示:
  - 現在値
  - 最近の変化
  - Rain signs の要約

### Slide 2

- 役割: 雨の手がかりの解釈
- 表示:
  - Pressure / Humidity / Light の signal rows
  - Rain signs の数
  - 今の並びと雨っぽい並びの比較
  - `RAIN COMING?` の問い

### Slide 3

- 役割: 短期傾向
- 時間窓: 15 分
- 表示:
  - Pressure / Humidity / Lux の短期グラフ
  - `What is changing now?` の問い

### Slide 4

- 役割: 長期傾向
- 時間窓: 120 分
- 表示:
  - Pressure / Humidity / Lux の長期グラフ
  - `Is the trend continuing?` の問い

### Status

- 通常ループ外の補助画面
- footer の `STATUS / BACK` で出入り
- 通常の教材スライドより優先度は低い

## 表示優先順位

1. 現在値が読めること
2. 直近変化の方向が分かること
3. 短期と長期の違いが見えること
4. 雨の手がかりを比較できること
5. Status は補助的に見られればよいこと

## 制約

- 日本語対応は英語版の安定後に段階的に進める
- まずは文言切替、その次にフォント適用、その後にレイアウト調整
- 夜間は `Light` を Rain signs から外す
  - 条件:
    - 日没後または日の出前
    - 低 lux が一定時間継続
- `LUX RATE` は UI 主役から外し、表示を簡素化
- `Status` の操作性改善は優先度低め
