# MQTT トピック仕様

## MQTT topic 一覧

### `env4`

屋外環境センサ本体の生データ。

主な項目:

- `ts`
- `temperature`
- `humidity`
- `pressure`
- `seq`
- `uptime_s`
- `time_valid`

### `home/env/env4/status`

屋外環境センサ送信側の状態監視トピック。

主な項目:

- `status`
- `reason`
- `wifi`
- `ip`
- `sensor_ready`
- `sensor_error_count`
- `wifi_reconnect_count`
- `mqtt_reconnect_count`
- `uptime_s`
- `seq`
- `unix_time`
- `time_valid`

### `home/env/lux/raw`

現在照度の軽量トピック。

主な項目:

- `lux`
- `unix_time`
- `time_valid`

### `home/env/lux/meta`

照度の移動平均、差分、変化率、trend を含む意味付け済みトピック。

主な項目:

- `lux`
- `avg`
- `delta`
- `delta_prev`
- `rate_pct`
- `trend`
- `samples`
- `interval_ms`
- `seq`
- `unix_time`
- `time_valid`

### `home/env/lux/status`

照度センサ送信側の状態監視トピック。

主な項目:

- `status`
- `reason`
- `wifi`
- `ip`
- `sensor_ready`
- `sensor_error_count`
- `wifi_reconnect_count`
- `mqtt_reconnect_count`
- `uptime_s`
- `seq`
- `unix_time`
- `time_valid`

## payload 例

### `env4`

```json
{"id":"env4","ts":1775016349,"temperature":16.34,"humidity":75.73,"pressure":1011.95,"seq":5741,"uptime_s":172329,"time_valid":1}
```

### `home/env/env4/status`

```json
{"status":"ok","reason":"periodic","wifi":"connected","ip":"192.168.0.232","sensor_ready":true,"sensor_error_count":0,"wifi_reconnect_count":0,"mqtt_reconnect_count":0,"uptime_s":172329,"seq":12,"unix_time":1775016349,"time_valid":true}
```

### `home/env/lux/raw`

```json
{"lux":345.0,"unix_time":1775014836,"time_valid":true}
```

### `home/env/lux/meta`

```json
{"lux":345.0,"avg":345.0,"delta":0.0,"delta_prev":-1.0,"rate_pct":0.00,"trend":"stable","samples":1,"interval_ms":30000,"seq":1,"unix_time":1775014836,"time_valid":true}
```

### `home/env/lux/status`

```json
{"status":"ok","reason":"boot","wifi":"connected","ip":"192.168.0.2","sensor_ready":true,"sensor_error_count":0,"wifi_reconnect_count":1,"mqtt_reconnect_count":1,"uptime_s":6,"seq":0,"unix_time":1775014836,"time_valid":true}
```

## 保持方針

### 送信側

- `raw` は現在値の軽量配信用
- `meta` は表示側で再計算しなくてよいよう、意味付け済み値を送る
- `status` は retained publish を使い、起動・再接続・異常時の状態を broker 側へ残す

### 表示側

- `env4` と `lux_meta` はグラフ履歴用リングバッファへ格納
- `lux_raw` は現在照度表示用
- `env4_status` と `lux_status` はフッターから入る `送信機状態` 画面用

### SDカード

- 最新状態は `/state/latest.json` へ保存
- 履歴は `/logs/env4_log.csv` と `/logs/lux_log.csv` へ追記
- 起動時に CSV を読み直して履歴復元
- ただし、ライブ受信時刻と 180 分以上ずれる古い履歴は破棄
