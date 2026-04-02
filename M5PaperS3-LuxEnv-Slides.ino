#include <SD.h>            // IMPORTANT: keep before M5Unified on M5GFX/M5Unified projects
#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"
#include "icons.h"
#include "ja_assets.h"
#include "ui_text.h"

// ============================================================
// M5PaperS3 MQTT Slide Dashboard
// - Subscribes:
//     env4
//     home/env/lux/raw
//     home/env/lux/meta
//     home/env/lux/status
// - Uses SD card for:
//     /logs/env4_log.csv
//     /logs/lux_log.csv
//     /state/latest.json
// - English-only UI for first implementation
// ============================================================

// ---------------------- MQTT topics --------------------------
static constexpr const char* TOPIC_ENV4        = "env4";
static constexpr const char* TOPIC_LUX_RAW     = "home/env/lux/raw";
static constexpr const char* TOPIC_LUX_META    = "home/env/lux/meta";
static constexpr const char* TOPIC_LUX_STATUS  = "home/env/lux/status";
static constexpr const char* MQTT_CLIENT_ID    = "m5papers3_lux_env_dashboard";

// ---------------------- timing -------------------------------
static constexpr uint32_t MQTT_RETRY_MS        = 5000;
static constexpr uint32_t SLIDE_INTERVAL_MS    = 15000;
static constexpr uint32_t EPD_REFRESH_MS       = SLIDE_INTERVAL_MS;
static constexpr uint32_t WIFI_RETRY_MS        = 5000;
static constexpr uint32_t NTP_RETRY_MS         = 60000;
static constexpr uint32_t STATE_SAVE_MS        = 30000;

// ---------------------- history ------------------------------
static constexpr size_t HISTORY_CAP = 300;  // Covers ~150 min at 30 s lux updates.
static constexpr uint32_t SHORT_WINDOW_MIN = 15;
static constexpr uint32_t LONG_WINDOW_MIN = 120;
static constexpr uint32_t CSV_STALE_LIMIT_MIN = 180;
static constexpr uint32_t NIGHT_LUX_WINDOW_MIN = 10;
static constexpr float NIGHT_LUX_THRESHOLD = 5.0f;
static constexpr uint8_t MAIN_SLIDE_COUNT = 4;
static constexpr uint8_t STATUS_SCREEN_INDEX = 4;

struct Env4Data {
  uint32_t ts = 0;
  float temperature = NAN;
  float humidity = NAN;
  float pressure = NAN;
  uint32_t seq = 0;
  uint32_t uptime_s = 0;
  bool time_valid = false;
  bool valid = false;
};

struct LuxRawData {
  float lux = NAN;
  uint32_t unix_time = 0;
  bool time_valid = false;
  bool valid = false;
};

struct LuxMetaData {
  float lux = NAN;
  float avg = NAN;
  float delta = NAN;
  float delta_prev = NAN;
  float rate_pct = NAN;
  char trend[20] = "unknown";
  uint32_t samples = 0;
  uint32_t interval_ms = 0;
  uint32_t seq = 0;
  uint32_t unix_time = 0;
  bool time_valid = false;
  bool valid = false;
};

struct LuxStatusData {
  char status[16] = "unknown";
  char reason[32] = "none";
  char wifi[16] = "unknown";
  char ip[32] = "0.0.0.0";
  bool sensor_ready = false;
  uint32_t sensor_error_count = 0;
  uint32_t wifi_reconnect_count = 0;
  uint32_t mqtt_reconnect_count = 0;
  uint32_t uptime_s = 0;
  uint32_t seq = 0;
  uint32_t unix_time = 0;
  bool time_valid = false;
  bool valid = false;
};

template <typename T, size_t N>
struct RingBuffer {
  T data[N];
  size_t head = 0;
  size_t count = 0;

  void clear() {
    head = 0;
    count = 0;
  }

  void push(const T& value) {
    data[head] = value;
    head = (head + 1) % N;
    if (count < N) ++count;
  }

  bool empty() const { return count == 0; }

  const T& at(size_t i) const {
    // i: 0..count-1 oldest -> newest
    size_t idx = (head + N - count + i) % N;
    return data[idx];
  }
};

struct PointEnv {
  uint32_t ts;
  float temperature;
  float humidity;
  float pressure;
};

struct PointLux {
  uint32_t ts;
  float lux;
  float avg;
  float rate_pct;
};

Env4Data g_env4;
LuxRawData g_luxRaw;
LuxMetaData g_luxMeta;
LuxStatusData g_luxStatus;

RingBuffer<PointEnv, HISTORY_CAP> g_envHist;
RingBuffer<PointLux, HISTORY_CAP> g_luxHist;

float g_pressureGraphVals[HISTORY_CAP];
float g_humidityGraphVals[HISTORY_CAP];
float g_luxGraphVals[HISTORY_CAP];
uint32_t g_envGraphTs[HISTORY_CAP];
uint32_t g_luxGraphTs[HISTORY_CAP];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

uint8_t g_currentSlide = 0;
uint8_t g_lastMainSlide = 0;
uint32_t g_liveDisplayTs = 0;
uint32_t g_restoredEnvLatestTs = 0;
uint32_t g_restoredLuxLatestTs = 0;
bool g_restoreFreshnessPending = false;
uint32_t g_lastSlideMs = 0;
uint32_t g_lastRefreshMs = 0;
uint32_t g_lastWifiAttemptMs = 0;
uint32_t g_lastMqttAttemptMs = 0;
uint32_t g_lastNtpAttemptMs = 0;
uint32_t g_lastStateSaveMs = 0;

bool g_needRedraw = true;
bool g_sdReady = false;
bool g_timeValid = false;

// ---------------------- utilities ----------------------------
String formatUnixTime(uint32_t ts) {
  if (ts == 0) return "----/--/-- --:--";
  time_t t = static_cast<time_t>(ts + 9 * 3600UL);
  struct tm tmLocal;
  gmtime_r(&t, &tmLocal);

  char buf[32];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d",
           tmLocal.tm_year + 1900, tmLocal.tm_mon + 1, tmLocal.tm_mday,
           tmLocal.tm_hour, tmLocal.tm_min);
  return String(buf);
}

String formatFloat1(float v, const char* fallback = "--") {
  if (isnan(v)) return String(fallback);
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f", v);
  return String(buf);
}

String formatFloat2(float v, const char* fallback = "--") {
  if (isnan(v)) return String(fallback);
  char buf[24];
  snprintf(buf, sizeof(buf), "%.2f", v);
  return String(buf);
}

String arrowForDelta(float v) {
  if (isnan(v)) return "?";
  if (v > 0.05f) return "UP";
  if (v < -0.05f) return "DOWN";
  return "FLAT";
}

float safeLatestTs() {
  if (g_luxRaw.valid && g_luxRaw.unix_time > 0) return g_luxRaw.unix_time;
  if (g_env4.valid && g_env4.ts > 0) return g_env4.ts;
  return 0;
}

void clearRestoredHistoryIfStale(uint32_t liveTs) {
  if (!g_restoreFreshnessPending || liveTs <= 1700000000UL) return;

  uint32_t latestCsvTs = g_restoredEnvLatestTs;
  if (g_restoredLuxLatestTs > latestCsvTs) latestCsvTs = g_restoredLuxLatestTs;
  if (latestCsvTs == 0) {
    g_restoreFreshnessPending = false;
    return;
  }

  uint32_t diffSec = (liveTs > latestCsvTs) ? (liveTs - latestCsvTs) : (latestCsvTs - liveTs);
  if (diffSec > CSV_STALE_LIMIT_MIN * 60UL) {
    g_envHist.clear();
    g_luxHist.clear();
    g_restoredEnvLatestTs = 0;
    g_restoredLuxLatestTs = 0;
    g_needRedraw = true;
  }
  g_restoreFreshnessPending = false;
}

void noteLiveDisplayTime(uint32_t ts, bool timeValid) {
  if (!timeValid || ts <= 1700000000UL) return;
  g_liveDisplayTs = ts;
  clearRestoredHistoryIfStale(ts);
}

bool hasValidDisplayTime() {
  return g_liveDisplayTs > 1700000000UL;
}

String formatFooterTime() {
  if (!hasValidDisplayTime()) return "--:--";
  return formatUnixTime(g_liveDisplayTs);
}

String formatBatteryStatus() {
  int batt = M5.Power.getBatteryLevel();
  if (batt < 0 || batt > 100) {
    return ui_text::kBatteryUnknown;
  }
  String label = String(ui_text::kBatteryPrefix) + String(batt) + "%";
  if (M5.Power.isCharging()) {
    label += "+";
  }
  return label;
}

void ensureLogDirs() {
  if (!g_sdReady) return;
  if (!SD.exists("/logs")) SD.mkdir("/logs");
  if (!SD.exists("/state")) SD.mkdir("/state");
}

void appendLine(const char* path, const String& line) {
  if (!g_sdReady) return;
  File f = SD.open(path, FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
}

void saveLatestState() {
  if (!g_sdReady) return;
  ensureLogDirs();

  StaticJsonDocument<1024> doc;
  struct tm tmLocal;
  doc["saved_at"] = getLocalTime(&tmLocal, 1) ? (uint32_t)time(nullptr) : 0;

  JsonObject env4 = doc.createNestedObject("env4");
  env4["ts"] = g_env4.ts;
  env4["temperature"] = isnan(g_env4.temperature) ? 0 : g_env4.temperature;
  env4["humidity"] = isnan(g_env4.humidity) ? 0 : g_env4.humidity;
  env4["pressure"] = isnan(g_env4.pressure) ? 0 : g_env4.pressure;
  env4["seq"] = g_env4.seq;
  env4["uptime_s"] = g_env4.uptime_s;
  env4["time_valid"] = g_env4.time_valid;
  env4["valid"] = g_env4.valid;

  JsonObject raw = doc.createNestedObject("lux_raw");
  raw["lux"] = isnan(g_luxRaw.lux) ? 0 : g_luxRaw.lux;
  raw["unix_time"] = g_luxRaw.unix_time;
  raw["time_valid"] = g_luxRaw.time_valid;
  raw["valid"] = g_luxRaw.valid;

  JsonObject meta = doc.createNestedObject("lux_meta");
  meta["lux"] = isnan(g_luxMeta.lux) ? 0 : g_luxMeta.lux;
  meta["avg"] = isnan(g_luxMeta.avg) ? 0 : g_luxMeta.avg;
  meta["delta"] = isnan(g_luxMeta.delta) ? 0 : g_luxMeta.delta;
  meta["delta_prev"] = isnan(g_luxMeta.delta_prev) ? 0 : g_luxMeta.delta_prev;
  meta["rate_pct"] = isnan(g_luxMeta.rate_pct) ? 0 : g_luxMeta.rate_pct;
  meta["trend"] = g_luxMeta.trend;
  meta["samples"] = g_luxMeta.samples;
  meta["interval_ms"] = g_luxMeta.interval_ms;
  meta["seq"] = g_luxMeta.seq;
  meta["unix_time"] = g_luxMeta.unix_time;
  meta["time_valid"] = g_luxMeta.time_valid;
  meta["valid"] = g_luxMeta.valid;

  JsonObject status = doc.createNestedObject("lux_status");
  status["status"] = g_luxStatus.status;
  status["reason"] = g_luxStatus.reason;
  status["wifi"] = g_luxStatus.wifi;
  status["ip"] = g_luxStatus.ip;
  status["sensor_ready"] = g_luxStatus.sensor_ready;
  status["sensor_error_count"] = g_luxStatus.sensor_error_count;
  status["wifi_reconnect_count"] = g_luxStatus.wifi_reconnect_count;
  status["mqtt_reconnect_count"] = g_luxStatus.mqtt_reconnect_count;
  status["uptime_s"] = g_luxStatus.uptime_s;
  status["seq"] = g_luxStatus.seq;
  status["unix_time"] = g_luxStatus.unix_time;
  status["time_valid"] = g_luxStatus.time_valid;
  status["valid"] = g_luxStatus.valid;

  File f = SD.open("/state/latest.json", FILE_WRITE);
  if (!f) return;
  serializeJsonPretty(doc, f);
  f.close();
}

void loadLatestState() {
  if (!g_sdReady) return;
  if (!SD.exists("/state/latest.json")) return;

  File f = SD.open("/state/latest.json", FILE_READ);
  if (!f) return;

  StaticJsonDocument<1536> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  JsonObject env4 = doc["env4"];
  if (!env4.isNull()) {
    g_env4.ts = env4["ts"] | 0;
    g_env4.temperature = env4["temperature"] | NAN;
    g_env4.humidity = env4["humidity"] | NAN;
    g_env4.pressure = env4["pressure"] | NAN;
    g_env4.seq = env4["seq"] | 0;
    g_env4.uptime_s = env4["uptime_s"] | 0;
    g_env4.time_valid = env4["time_valid"] | false;
    g_env4.valid = env4["valid"] | false;
  }

  JsonObject raw = doc["lux_raw"];
  if (!raw.isNull()) {
    g_luxRaw.lux = raw["lux"] | NAN;
    g_luxRaw.unix_time = raw["unix_time"] | 0;
    g_luxRaw.time_valid = raw["time_valid"] | false;
    g_luxRaw.valid = raw["valid"] | false;
  }

  JsonObject meta = doc["lux_meta"];
  if (!meta.isNull()) {
    g_luxMeta.lux = meta["lux"] | NAN;
    g_luxMeta.avg = meta["avg"] | NAN;
    g_luxMeta.delta = meta["delta"] | NAN;
    g_luxMeta.delta_prev = meta["delta_prev"] | NAN;
    g_luxMeta.rate_pct = meta["rate_pct"] | NAN;
    strlcpy(g_luxMeta.trend, meta["trend"] | "unknown", sizeof(g_luxMeta.trend));
    g_luxMeta.samples = meta["samples"] | 0;
    g_luxMeta.interval_ms = meta["interval_ms"] | 0;
    g_luxMeta.seq = meta["seq"] | 0;
    g_luxMeta.unix_time = meta["unix_time"] | 0;
    g_luxMeta.time_valid = meta["time_valid"] | false;
    g_luxMeta.valid = meta["valid"] | false;
  }

  JsonObject status = doc["lux_status"];
  if (!status.isNull()) {
    strlcpy(g_luxStatus.status, status["status"] | "unknown", sizeof(g_luxStatus.status));
    strlcpy(g_luxStatus.reason, status["reason"] | "none", sizeof(g_luxStatus.reason));
    strlcpy(g_luxStatus.wifi, status["wifi"] | "unknown", sizeof(g_luxStatus.wifi));
    strlcpy(g_luxStatus.ip, status["ip"] | "0.0.0.0", sizeof(g_luxStatus.ip));
    g_luxStatus.sensor_ready = status["sensor_ready"] | false;
    g_luxStatus.sensor_error_count = status["sensor_error_count"] | 0;
    g_luxStatus.wifi_reconnect_count = status["wifi_reconnect_count"] | 0;
    g_luxStatus.mqtt_reconnect_count = status["mqtt_reconnect_count"] | 0;
    g_luxStatus.uptime_s = status["uptime_s"] | 0;
    g_luxStatus.seq = status["seq"] | 0;
    g_luxStatus.unix_time = status["unix_time"] | 0;
    g_luxStatus.time_valid = status["time_valid"] | false;
    g_luxStatus.valid = status["valid"] | false;
  }
}

void restoreEnvHistoryFromCsv() {
  if (!g_sdReady || !SD.exists("/logs/env4_log.csv")) return;

  File f = SD.open("/logs/env4_log.csv", FILE_READ);
  if (!f) return;

  g_envHist.clear();
  g_restoredEnvLatestTs = 0;
  bool isFirstLine = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (isFirstLine) {
      isFirstLine = false;
      if (line.startsWith("ts,")) continue;
    }

    uint32_t ts = 0;
    float temperature = NAN;
    float humidity = NAN;
    float pressure = NAN;
    uint32_t seq = 0;
    uint32_t uptime_s = 0;
    int time_valid = 0;

    int parsed = sscanf(line.c_str(), "%u,%f,%f,%f,%u,%u,%d",
                        &ts, &temperature, &humidity, &pressure, &seq, &uptime_s, &time_valid);
    if (parsed < 4 || ts == 0 || isnan(pressure)) continue;

    g_envHist.push({ts, temperature, humidity, pressure});
    g_restoredEnvLatestTs = ts;
  }
  f.close();
}

void restoreLuxHistoryFromCsv() {
  if (!g_sdReady || !SD.exists("/logs/lux_log.csv")) return;

  File f = SD.open("/logs/lux_log.csv", FILE_READ);
  if (!f) return;

  g_luxHist.clear();
  g_restoredLuxLatestTs = 0;
  bool isFirstLine = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (isFirstLine) {
      isFirstLine = false;
      if (line.startsWith("unix_time,")) continue;
    }

    uint32_t unix_time = 0;
    float lux = NAN;
    float avg = NAN;
    float delta = NAN;
    float delta_prev = NAN;
    float rate_pct = NAN;
    char trend[20] = {0};
    uint32_t samples = 0;
    uint32_t interval_ms = 0;
    uint32_t seq = 0;
    int time_valid = 0;

    int parsed = sscanf(line.c_str(), "%u,%f,%f,%f,%f,%f,%19[^,],%u,%u,%u,%d",
                        &unix_time, &lux, &avg, &delta, &delta_prev, &rate_pct,
                        trend, &samples, &interval_ms, &seq, &time_valid);
    if (parsed < 6 || unix_time == 0 || isnan(lux)) continue;

    g_luxHist.push({unix_time, lux, avg, rate_pct});
    g_restoredLuxLatestTs = unix_time;
  }
  f.close();
}

void restoreHistoryFromCsv() {
  restoreEnvHistoryFromCsv();
  restoreLuxHistoryFromCsv();
  g_restoreFreshnessPending = (g_restoredEnvLatestTs > 0 || g_restoredLuxLatestTs > 0);
}

// ---------------------- connectivity -------------------------
void syncNtpIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (g_timeValid && time(nullptr) > 1700000000) return;

  uint32_t nowMs = millis();
  if (nowMs - g_lastNtpAttemptMs < NTP_RETRY_MS) return;
  g_lastNtpAttemptMs = nowMs;

  configTzTime(CONFIG_TZ_INFO, CONFIG_NTP_SERVER_1, CONFIG_NTP_SERVER_2);

  time_t now = time(nullptr);
  for (int i = 0; i < 20; ++i) {
    if (now > 1700000000) {
      g_timeValid = true;
      return;
    }
    delay(250);
    now = time(nullptr);
  }
}

void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  uint32_t nowMs = millis();
  if (nowMs - g_lastWifiAttemptMs < WIFI_RETRY_MS) return;
  g_lastWifiAttemptMs = nowMs;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool mqttConnect() {
  StaticJsonDocument<256> willDoc;
  willDoc["status"] = "offline";
  willDoc["reason"] = "last_will";
  willDoc["unix_time"] = 0;
  willDoc["time_valid"] = false;

  char willBuf[256];
  size_t n = serializeJson(willDoc, willBuf, sizeof(willBuf));

  return mqttClient.connect(
      MQTT_CLIENT_ID,
      nullptr,
      nullptr,
      TOPIC_LUX_STATUS,
      1,
      true,
      willBuf,
      n);
}

void ensureMqttConnected() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) return;

  uint32_t nowMs = millis();
  if (nowMs - g_lastMqttAttemptMs < MQTT_RETRY_MS) return;
  g_lastMqttAttemptMs = nowMs;

  if (!mqttConnect()) return;

  mqttClient.subscribe(TOPIC_ENV4);
  mqttClient.subscribe(TOPIC_LUX_RAW);
  mqttClient.subscribe(TOPIC_LUX_META);
  mqttClient.subscribe(TOPIC_LUX_STATUS);
}

// ---------------------- topic handlers -----------------------
void handleEnv4(const JsonDocument& doc) {
  g_env4.ts = doc["ts"] | 0;
  g_env4.temperature = doc["temperature"] | NAN;
  g_env4.humidity = doc["humidity"] | NAN;
  g_env4.pressure = doc["pressure"] | NAN;
  g_env4.seq = doc["seq"] | 0;
  g_env4.uptime_s = doc["uptime_s"] | 0;
  g_env4.time_valid = (doc["time_valid"] | 0) != 0;
  g_env4.valid = true;

  if (g_env4.ts > 0 && !isnan(g_env4.pressure)) {
    g_envHist.push({g_env4.ts, g_env4.temperature, g_env4.humidity, g_env4.pressure});
  }
  noteLiveDisplayTime(g_env4.ts, g_env4.time_valid);

  if (g_sdReady) {
    ensureLogDirs();
    if (!SD.exists("/logs/env4_log.csv")) {
      appendLine("/logs/env4_log.csv", "ts,temperature,humidity,pressure,seq,uptime_s,time_valid");
    }
    String line = String(g_env4.ts) + "," +
                  formatFloat2(g_env4.temperature, "") + "," +
                  formatFloat2(g_env4.humidity, "") + "," +
                  formatFloat2(g_env4.pressure, "") + "," +
                  String(g_env4.seq) + "," +
                  String(g_env4.uptime_s) + "," +
                  String(g_env4.time_valid ? 1 : 0);
    appendLine("/logs/env4_log.csv", line);
  }

  g_needRedraw = true;
}

void handleLuxRaw(const JsonDocument& doc) {
  g_luxRaw.lux = doc["lux"] | NAN;
  g_luxRaw.unix_time = doc["unix_time"] | 0;
  g_luxRaw.time_valid = doc["time_valid"] | false;
  g_luxRaw.valid = true;
  noteLiveDisplayTime(g_luxRaw.unix_time, g_luxRaw.time_valid);
  g_needRedraw = true;
}

void handleLuxMeta(const JsonDocument& doc) {
  g_luxMeta.lux = doc["lux"] | NAN;
  g_luxMeta.avg = doc["avg"] | NAN;
  g_luxMeta.delta = doc["delta"] | NAN;
  g_luxMeta.delta_prev = doc["delta_prev"] | NAN;
  g_luxMeta.rate_pct = doc["rate_pct"] | NAN;
  strlcpy(g_luxMeta.trend, doc["trend"] | "unknown", sizeof(g_luxMeta.trend));
  g_luxMeta.samples = doc["samples"] | 0;
  g_luxMeta.interval_ms = doc["interval_ms"] | 0;
  g_luxMeta.seq = doc["seq"] | 0;
  g_luxMeta.unix_time = doc["unix_time"] | 0;
  g_luxMeta.time_valid = doc["time_valid"] | false;
  g_luxMeta.valid = true;

  if (g_luxMeta.unix_time > 0 && !isnan(g_luxMeta.lux)) {
    g_luxHist.push({g_luxMeta.unix_time, g_luxMeta.lux, g_luxMeta.avg, g_luxMeta.rate_pct});
  }
  noteLiveDisplayTime(g_luxMeta.unix_time, g_luxMeta.time_valid);

  if (g_sdReady) {
    ensureLogDirs();
    if (!SD.exists("/logs/lux_log.csv")) {
      appendLine("/logs/lux_log.csv", "unix_time,lux,avg,delta,delta_prev,rate_pct,trend,samples,interval_ms,seq,time_valid");
    }
    String line = String(g_luxMeta.unix_time) + "," +
                  formatFloat2(g_luxMeta.lux, "") + "," +
                  formatFloat2(g_luxMeta.avg, "") + "," +
                  formatFloat2(g_luxMeta.delta, "") + "," +
                  formatFloat2(g_luxMeta.delta_prev, "") + "," +
                  formatFloat2(g_luxMeta.rate_pct, "") + "," +
                  String(g_luxMeta.trend) + "," +
                  String(g_luxMeta.samples) + "," +
                  String(g_luxMeta.interval_ms) + "," +
                  String(g_luxMeta.seq) + "," +
                  String(g_luxMeta.time_valid ? 1 : 0);
    appendLine("/logs/lux_log.csv", line);
  }

  g_needRedraw = true;
}

void handleLuxStatus(const JsonDocument& doc) {
  strlcpy(g_luxStatus.status, doc["status"] | "unknown", sizeof(g_luxStatus.status));
  strlcpy(g_luxStatus.reason, doc["reason"] | "none", sizeof(g_luxStatus.reason));
  strlcpy(g_luxStatus.wifi, doc["wifi"] | "unknown", sizeof(g_luxStatus.wifi));
  strlcpy(g_luxStatus.ip, doc["ip"] | "0.0.0.0", sizeof(g_luxStatus.ip));
  g_luxStatus.sensor_ready = doc["sensor_ready"] | false;
  g_luxStatus.sensor_error_count = doc["sensor_error_count"] | 0;
  g_luxStatus.wifi_reconnect_count = doc["wifi_reconnect_count"] | 0;
  g_luxStatus.mqtt_reconnect_count = doc["mqtt_reconnect_count"] | 0;
  g_luxStatus.uptime_s = doc["uptime_s"] | 0;
  g_luxStatus.seq = doc["seq"] | 0;
  g_luxStatus.unix_time = doc["unix_time"] | 0;
  g_luxStatus.time_valid = doc["time_valid"] | false;
  g_luxStatus.valid = true;
  noteLiveDisplayTime(g_luxStatus.unix_time, g_luxStatus.time_valid);
  g_needRedraw = true;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("[MQTT] topic=%s len=%u\n", topic, length);
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.printf("[MQTT] json error: %s\n", err.c_str());
    return;
  }

  if (strcmp(topic, TOPIC_ENV4) == 0) {
    handleEnv4(doc);
  } else if (strcmp(topic, TOPIC_LUX_RAW) == 0) {
    handleLuxRaw(doc);
  } else if (strcmp(topic, TOPIC_LUX_META) == 0) {
    handleLuxMeta(doc);
  } else if (strcmp(topic, TOPIC_LUX_STATUS) == 0) {
    handleLuxStatus(doc);
  }

  saveLatestState();
  Serial.println("[MQTT] handled");
}

// ---------------------- drawing helpers ----------------------
static constexpr int UI_MARGIN_X = 24;
static constexpr int UI_HEADER_H = 52;
static constexpr int UI_FOOTER_H = 34;
static constexpr int UI_CONTENT_TOP = 78;
static constexpr int FOOTER_BUTTON_W = 96;
static constexpr int FOOTER_BUTTON_H = 22;
static constexpr int FOOTER_BUTTON_HIT_W = 164;
static constexpr int FOOTER_BUTTON_HIT_H = 38;
static constexpr int STATUS_SWIPE_START_Y = 760;
static constexpr int STATUS_SWIPE_MIN_DISTANCE = 120;
static constexpr int STATUS_SWIPE_MAX_SIDE_SHIFT = 80;

inline bool isJapaneseUi() {
  return CONFIG_UI_LANG == UI_LANG_JA;
}

inline const lgfx::IFont* uiSmallFont() {
  return isJapaneseUi() ? ja_assets::kLabelFont : &fonts::Font2;
}

inline const lgfx::IFont* uiBodyFont() {
  return isJapaneseUi() ? ja_assets::kBodyFont : &fonts::Font4;
}

inline const lgfx::IFont* uiTitleFont() {
  return isJapaneseUi() ? ja_assets::kTitleFont : &fonts::Font4;
}

M5Canvas gUiTextCanvas(&M5.Display);

int uiTextWidth(const char* text, const lgfx::IFont* font) {
  if (!text) return 0;
  if (!isJapaneseUi()) {
    return M5.Display.textWidth(text, font);
  }
  M5.Display.setFont(font);
  return M5.Display.textWidth(text);
}

int uiTextWidth(const String& text, const lgfx::IFont* font) {
  return uiTextWidth(text.c_str(), font);
}

void drawUiTextLeft(const char* text, int x, int y, const lgfx::IFont* font,
                    uint16_t fg = TFT_BLACK, uint16_t bg = TFT_WHITE) {
  M5.Display.setTextColor(fg, bg);
  if (!isJapaneseUi()) {
    M5.Display.drawString(text, x, y, font);
    return;
  }
  int width = uiTextWidth(text, font);
  int height = M5.Display.fontHeight(font);
  if (width <= 0) return;

  gUiTextCanvas.deleteSprite();
  gUiTextCanvas.setColorDepth(1);
  gUiTextCanvas.setFont(font);
  gUiTextCanvas.setTextWrap(false);
  gUiTextCanvas.createSprite(width + 8, height + 8);
  gUiTextCanvas.fillSprite(bg);
  gUiTextCanvas.setTextColor(fg, bg);
  gUiTextCanvas.setCursor(0, 0);
  gUiTextCanvas.print(text);
  gUiTextCanvas.pushSprite(&M5.Display, x, y);
}

void drawUiTextLeft(const String& text, int x, int y, const lgfx::IFont* font,
                    uint16_t fg = TFT_BLACK, uint16_t bg = TFT_WHITE) {
  drawUiTextLeft(text.c_str(), x, y, font, fg, bg);
}

void drawUiTextRight(const char* text, int rightX, int y, const lgfx::IFont* font,
                     uint16_t fg = TFT_BLACK, uint16_t bg = TFT_WHITE) {
  int width = uiTextWidth(text, font);
  drawUiTextLeft(text, rightX - width, y, font, fg, bg);
}

void drawUiTextRight(const String& text, int rightX, int y, const lgfx::IFont* font,
                     uint16_t fg = TFT_BLACK, uint16_t bg = TFT_WHITE) {
  drawUiTextRight(text.c_str(), rightX, y, font, fg, bg);
}

void drawUiTextCenter(const char* text, int centerX, int y, const lgfx::IFont* font,
                      uint16_t fg = TFT_BLACK, uint16_t bg = TFT_WHITE) {
  int width = uiTextWidth(text, font);
  drawUiTextLeft(text, centerX - width / 2, y, font, fg, bg);
}

void drawUiTextCenter(const String& text, int centerX, int y, const lgfx::IFont* font,
                      uint16_t fg = TFT_BLACK, uint16_t bg = TFT_WHITE) {
  drawUiTextCenter(text.c_str(), centerX, y, font, fg, bg);
}

void drawHeader(const char* title) {
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);

  M5.Display.fillRect(0, 0, M5.Display.width(), UI_HEADER_H - 8, TFT_BLACK);
  drawUiTextLeft(title, 16, 14, uiTitleFont(), TFT_WHITE, TFT_BLACK);
  drawUiTextRight(formatBatteryStatus(), M5.Display.width() - 16, 16, uiSmallFont(), TFT_WHITE, TFT_BLACK);

  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawLine(0, UI_HEADER_H, M5.Display.width(), UI_HEADER_H, TFT_BLACK);
}

void drawFooter() {
  String ts = formatFooterTime();
  String net = (WiFi.status() == WL_CONNECTED) ? ui_text::kWifiOk : ui_text::kWifiNg;
  String mq = mqttClient.connected() ? ui_text::kMqttOk : ui_text::kMqttNg;
  const int btnX = (M5.Display.width() - FOOTER_BUTTON_W) / 2;
  const int btnY = M5.Display.height() - UI_FOOTER_H + 6;
  const char* btnLabel = (g_currentSlide == STATUS_SCREEN_INDEX) ? ui_text::kBackButton : ui_text::kStatusButton;

  M5.Display.drawLine(0, M5.Display.height() - UI_FOOTER_H, M5.Display.width(), M5.Display.height() - UI_FOOTER_H, TFT_BLACK);
  M5.Display.drawString(ts, UI_MARGIN_X, M5.Display.height() - 28, &fonts::Font2);
  M5.Display.drawRect(btnX, btnY, FOOTER_BUTTON_W, FOOTER_BUTTON_H, TFT_BLACK);
  drawUiTextCenter(btnLabel, btnX + FOOTER_BUTTON_W / 2, btnY + 4, uiSmallFont());
  M5.Display.drawRightString(net + "  " + mq, M5.Display.width() - UI_MARGIN_X, M5.Display.height() - 28, &fonts::Font2);
}

bool footerButtonContains(int x, int y) {
  const int hitX = (M5.Display.width() - FOOTER_BUTTON_HIT_W) / 2;
  const int hitY = M5.Display.height() - UI_FOOTER_H;
  return x >= hitX && x < (hitX + FOOTER_BUTTON_HIT_W)
      && y >= hitY && y < (hitY + FOOTER_BUTTON_HIT_H);
}

bool isStatusSwipe(const m5::touch_detail_t& touch) {
  if (g_currentSlide == STATUS_SCREEN_INDEX) return false;
  if (!(touch.wasFlicked() || touch.wasDragged() || touch.wasReleased())) return false;
  if (touch.base_y < STATUS_SWIPE_START_Y) return false;
  if (touch.distanceY() > -STATUS_SWIPE_MIN_DISTANCE) return false;
  if (abs(touch.distanceX()) > STATUS_SWIPE_MAX_SIDE_SHIFT) return false;
  return true;
}

void drawKeyValue(const char* label, const String& value, int x, int y, const lgfx::IFont* valueFont = &fonts::Font4) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(label, x, y, uiSmallFont());
  M5.Display.drawString(value, x, y + 18, valueFont);
}

void drawMetricBlock(const char* label, const String& value, const String& unit, int x, int y, int unitDx = 110) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(label, x, y, uiSmallFont());
  M5.Display.drawString(value, x, y + 16, &fonts::Font6);
  if (unit.length() > 0) {
    M5.Display.drawString(unit, x + unitDx, y + 34, &fonts::Font2);
  }
}

void drawTextBlock(const char* label, const String& value, int x, int y, const lgfx::IFont* valueFont = &fonts::Font4) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(label, x, y, uiSmallFont());
  M5.Display.drawString(value, x, y + 16, valueFont);
}

void drawTextPair(const char* label, const String& value, int x, int y, int valueX, const lgfx::IFont* valueFont = &fonts::Font4) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(label, x, y, uiSmallFont());
  M5.Display.drawString(value, valueX, y, valueFont);
}

void drawTextRowAligned(const char* label, const String& value, int labelX, int valueRightX, int y,
                        const lgfx::IFont* valueFont = &fonts::Font4) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(label, labelX, y, uiSmallFont());
  if (isJapaneseUi()) {
    drawUiTextRight(value, valueRightX, y, valueFont);
  } else {
    M5.Display.drawRightString(value, valueRightX, y, valueFont);
  }
}

void drawSummaryRow(const char* label, const String& value, int x, int y) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(label, x, y, uiSmallFont());
  drawUiTextRight(value, M5.Display.width() - UI_MARGIN_X - 8, y, uiBodyFont());
}

void drawMonoIcon(int x, int y, const MonoIcon& icon, int scale = 1) {
  const int bytesPerRow = (icon.width + 7) / 8;
  for (int row = 0; row < icon.height; ++row) {
    for (int col = 0; col < icon.width; ++col) {
      uint8_t bits = pgm_read_byte(icon.data + row * bytesPerRow + (col / 8));
      if (bits & (0x80 >> (col % 8))) {
        M5.Display.fillRect(x + col * scale, y + row * scale, scale, scale, TFT_BLACK);
      }
    }
  }
}

void drawLabeledIcon(const MonoIcon& icon, const char* label, int x, int y, int scale = 1) {
  drawMonoIcon(x, y, icon, scale);
  drawUiTextLeft(label, x + icon.width * scale + 8, y + 2, uiSmallFont());
}

void drawMetricWithIcon(const MonoIcon& icon, const char* label, const String& value, const String& unit,
                        int x, int y, int unitX) {
  drawMonoIcon(x, y + 2, icon, 1);
  drawUiTextLeft(label, x + 24, y + 2, uiSmallFont());
  M5.Display.drawString(value, x, y + 28, &fonts::Font6);
  if (unit.length() > 0) {
    M5.Display.drawString(unit, unitX, y + 50, &fonts::Font2);
  }
}

void drawSummaryIconRow(const MonoIcon& icon, const char* label, const String& value, int x, int y) {
  drawMonoIcon(x, y - 2, icon, 1);
  drawUiTextLeft(label, x + 24, y, uiSmallFont());
  drawUiTextRight(value, M5.Display.width() - UI_MARGIN_X - 8, y, uiBodyFont());
}

void drawSummaryMetric(const char* label, const String& value, const String& unit,
                       int x, int y, int valueW, int unitGap = 10) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(label, x, y, uiSmallFont());
  M5.Display.drawString(value, x, y + 18, &fonts::Font6);
  if (unit.length() > 0) {
    M5.Display.drawString(unit, x + valueW + unitGap, y + 40, &fonts::Font2);
  }
}

String formatClockOnly(uint32_t ts) {
  if (ts == 0) return "--:--";
  time_t t = static_cast<time_t>(ts + 9 * 3600UL);
  struct tm tmLocal;
  gmtime_r(&t, &tmLocal);

  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
  return String(buf);
}

double normalizeDegrees(double deg) {
  while (deg < 0.0) deg += 360.0;
  while (deg >= 360.0) deg -= 360.0;
  return deg;
}

double degToRad(double deg) {
  return deg * PI / 180.0;
}

double radToDeg(double rad) {
  return rad * 180.0 / PI;
}

int computeSunEventMinuteJst(uint32_t ts, bool sunrise) {
  if (ts == 0) return sunrise ? 360 : 1080;

  time_t localTs = static_cast<time_t>(ts + 9 * 3600UL);
  struct tm tmLocal;
  gmtime_r(&localTs, &tmLocal);
  int dayOfYear = tmLocal.tm_yday + 1;

  double lngHour = CONFIG_SITE_LONGITUDE / 15.0;
  double t = dayOfYear + ((sunrise ? 6.0 : 18.0) - lngHour) / 24.0;
  double M = (0.9856 * t) - 3.289;
  double L = normalizeDegrees(M + (1.916 * sin(degToRad(M))) + (0.020 * sin(2 * degToRad(M))) + 282.634);

  double RA = radToDeg(atan(0.91764 * tan(degToRad(L))));
  RA = normalizeDegrees(RA);
  double Lquadrant = floor(L / 90.0) * 90.0;
  double RAquadrant = floor(RA / 90.0) * 90.0;
  RA = (RA + (Lquadrant - RAquadrant)) / 15.0;

  double sinDec = 0.39782 * sin(degToRad(L));
  double cosDec = cos(asin(sinDec));
  double cosH = (cos(degToRad(90.833)) - (sinDec * sin(degToRad(CONFIG_SITE_LATITUDE))))
              / (cosDec * cos(degToRad(CONFIG_SITE_LATITUDE)));

  if (cosH > 1.0) return sunrise ? 360 : 1080;
  if (cosH < -1.0) return sunrise ? 0 : 1439;

  double H = sunrise ? (360.0 - radToDeg(acos(cosH))) : radToDeg(acos(cosH));
  H /= 15.0;

  double T = H + RA - (0.06571 * t) - 6.622;
  double UT = T - lngHour;
  while (UT < 0.0) UT += 24.0;
  while (UT >= 24.0) UT -= 24.0;

  double localHour = UT + 9.0;
  while (localHour < 0.0) localHour += 24.0;
  while (localHour >= 24.0) localHour -= 24.0;
  return (int)round(localHour * 60.0);
}

bool isNightCandidate(uint32_t ts) {
  if (ts == 0) return false;
  time_t localTs = static_cast<time_t>(ts + 9 * 3600UL);
  struct tm tmLocal;
  gmtime_r(&localTs, &tmLocal);
  int nowMin = tmLocal.tm_hour * 60 + tmLocal.tm_min;
  int sunriseMin = computeSunEventMinuteJst(ts, true);
  int sunsetMin = computeSunEventMinuteJst(ts, false);
  return nowMin < sunriseMin || nowMin >= sunsetMin;
}

bool isLuxDarkSustained(uint32_t windowMin, float threshold) {
  if (g_luxHist.count == 0) return false;
  uint32_t newestTs = g_luxHist.at(g_luxHist.count - 1).ts;
  uint32_t cutoffTs = (newestTs > windowMin * 60) ? (newestTs - windowMin * 60) : 0;
  size_t samples = 0;
  for (size_t i = 0; i < g_luxHist.count; ++i) {
    const auto& point = g_luxHist.at(i);
    if (point.ts < cutoffTs) continue;
    ++samples;
    if (point.lux > threshold) return false;
  }
  return samples >= 3;
}

bool isLightRainFactorActive() {
  uint32_t liveTs = g_liveDisplayTs > 0 ? g_liveDisplayTs : (uint32_t)safeLatestTs();
  if (!isNightCandidate(liveTs)) return true;
  return !isLuxDarkSustained(NIGHT_LUX_WINDOW_MIN, NIGHT_LUX_THRESHOLD);
}

size_t collectEnvWindow(float* pressureVals, float* humidityVals, uint32_t* tsVals, uint32_t windowMin) {
  if (g_envHist.count == 0) return 0;

  uint32_t newestTs = g_envHist.at(g_envHist.count - 1).ts;
  uint32_t cutoffTs = (newestTs > windowMin * 60) ? (newestTs - windowMin * 60) : 0;
  size_t out = 0;
  for (size_t i = 0; i < g_envHist.count; ++i) {
    const auto& point = g_envHist.at(i);
    if (point.ts < cutoffTs) continue;
    pressureVals[out] = point.pressure;
    humidityVals[out] = point.humidity;
    tsVals[out] = point.ts;
    ++out;
  }
  return out;
}

size_t collectLuxWindow(float* luxVals, uint32_t* tsVals, uint32_t windowMin) {
  if (g_luxHist.count == 0) return 0;

  uint32_t newestTs = g_luxHist.at(g_luxHist.count - 1).ts;
  uint32_t cutoffTs = (newestTs > windowMin * 60) ? (newestTs - windowMin * 60) : 0;
  size_t out = 0;
  for (size_t i = 0; i < g_luxHist.count; ++i) {
    const auto& point = g_luxHist.at(i);
    if (point.ts < cutoffTs) continue;
    luxVals[out] = point.lux;
    tsVals[out] = point.ts;
    ++out;
  }
  return out;
}

void drawCard(int x, int y, int w, int h, const char* title) {
  M5.Display.drawRect(x, y, w, h, TFT_BLACK);
  drawUiTextLeft(title, x + 10, y + 10, uiSmallFont());
}

const char* trendLabel(const char* trend) {
  if (strcmp(trend, "rising_fast") == 0) return ui_text::kRisingFast;
  if (strcmp(trend, "rising") == 0) return ui_text::kRising;
  if (strcmp(trend, "falling_fast") == 0) return ui_text::kFallingFast;
  if (strcmp(trend, "falling") == 0) return ui_text::kFalling;
  return ui_text::kStable;
}

const char* signalGlyph(const String& signal) {
  if (signal == "NIGHT") return ui_text::kNight;
  if (signal == "UP") return ui_text::kAscend;
  if (signal == "DOWN") return ui_text::kDescend;
  return ui_text::kSteady;
}

const MonoIcon& signalIcon(const String& signal) {
  if (signal == "NIGHT") return ICON_CLOCK;
  if (signal == "UP") return ICON_ARROW_UP;
  if (signal == "DOWN") return ICON_ARROW_DOWN;
  return ICON_ARROW_STEADY;
}

bool isRainSign(const char* label, const String& signal) {
  if (strcmp(label, ui_text::kPressure) == 0) return signal == "DOWN";
  if (strcmp(label, ui_text::kHumidity) == 0) return signal == "UP";
  if (strcmp(label, ui_text::kLight) == 0) return signal == "DOWN";
  return false;
}

const char* signalMeaning(const char* label, const String& signal) {
  if (strcmp(label, ui_text::kPressure) == 0) {
    if (signal == "DOWN") return ui_text::kRainSign;
    if (signal == "UP") return ui_text::kFairSign;
    return ui_text::kWatch;
  }
  if (strcmp(label, ui_text::kHumidity) == 0) {
    if (signal == "UP") return ui_text::kRainSign;
    if (signal == "DOWN") return ui_text::kDrySign;
    return ui_text::kWatch;
  }
  if (signal == "NIGHT") return ui_text::kNightSkip;
  if (signal == "DOWN") return ui_text::kCloudSign;
  if (signal == "UP") return ui_text::kBrightSign;
  return ui_text::kWatch;
}

void drawSignalToken(int x, int y, const char* label, const String& signal) {
  M5.Display.drawString(label, x, y, &fonts::Font2);
  int iconX = x + 16;
  if (signal == "NIGHT") {
    drawUiTextLeft(ui_text::kNight, iconX, y, uiSmallFont());
    return;
  }
  drawMonoIcon(iconX, y - 2, signalIcon(signal), 1);
}

void drawPatternSummaryRow(int x, int y, const char* heading,
                           const String& pSignal, const String& hSignal, const String& lSignal,
  bool lightActive, bool isClueRow) {
  drawUiTextLeft(heading, x, y, uiSmallFont());
  int baseX = x + 86;
  String cluePressure = isClueRow ? String("DOWN") : pSignal;
  String clueHumidity = isClueRow ? String("UP") : hSignal;
  String clueLight = isClueRow ? String("DOWN") : lSignal;
  drawSignalToken(baseX, y, "P", cluePressure);
  drawSignalToken(baseX + 66, y, "H", clueHumidity);
  if (!isClueRow || lightActive) {
    drawSignalToken(baseX + 132, y, "L", lightActive ? clueLight : String("NIGHT"));
  }
}

void drawChangeSummaryRow(const MonoIcon& icon, const char* label, const String& signal, int y) {
  drawMonoIcon(UI_MARGIN_X + 22, y + 2, icon, 1);
  drawUiTextLeft(label, UI_MARGIN_X + 46, y, uiSmallFont());
  drawUiTextLeft(signalGlyph(signal), UI_MARGIN_X + 220, y, uiBodyFont());
  drawMonoIcon(UI_MARGIN_X + 370, y - 2, signalIcon(signal), 1);
  drawUiTextRight(signalMeaning(label, signal), M5.Display.width() - UI_MARGIN_X - 20, y, uiSmallFont());
}

float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

void drawCenteredGauge(int x, int y, int w, int h, float normalized) {
  const int mid = x + w / 2;
  const int innerY = y + 2;
  const int innerH = h - 4;
  M5.Display.drawRect(x, y, w, h, TFT_BLACK);
  M5.Display.drawLine(mid, y + 1, mid, y + h - 2, TFT_BLACK);

  float clamped = clamp01(fabs(normalized));
  int halfW = (w - 6) / 2;
  int fillW = (int)(halfW * clamped);
  if (fillW <= 0) return;

  if (normalized > 0.0f) {
    M5.Display.fillRect(mid + 1, innerY, fillW, innerH, TFT_BLACK);
  } else if (normalized < 0.0f) {
    M5.Display.fillRect(mid - fillW, innerY, fillW, innerH, TFT_BLACK);
  } else {
    M5.Display.fillRect(mid - 6, innerY, 12, innerH, TFT_BLACK);
  }
}

void drawFillGauge(int x, int y, int w, int h, float normalized) {
  M5.Display.drawRect(x, y, w, h, TFT_BLACK);
  int fillW = (int)((w - 4) * clamp01(normalized));
  if (fillW > 0) {
    M5.Display.fillRect(x + 2, y + 2, fillW, h - 4, TFT_BLACK);
  }
}

float normalizedRate(float ratePct) {
  if (isnan(ratePct)) return 0.0f;
  float n = ratePct / 10.0f;
  if (n > 1.0f) n = 1.0f;
  if (n < -1.0f) n = -1.0f;
  return n;
}

float normalizedPressureTrend() {
  if (g_envHist.count < 2) return 0.0f;
  float delta = g_envHist.at(g_envHist.count - 1).pressure - g_envHist.at(0).pressure;
  float n = delta / 1.5f;
  if (n > 1.0f) n = 1.0f;
  if (n < -1.0f) n = -1.0f;
  return n;
}

float normalizedHumidityTrend() {
  if (g_envHist.count < 2) return 0.0f;
  float delta = g_envHist.at(g_envHist.count - 1).humidity - g_envHist.at(0).humidity;
  float n = delta / 8.0f;
  if (n > 1.0f) n = 1.0f;
  if (n < -1.0f) n = -1.0f;
  return n;
}

float normalizedLuxTrend() {
  if (!g_luxMeta.valid) return 0.0f;
  return normalizedRate(g_luxMeta.rate_pct);
}

void drawSignalRow(const MonoIcon& icon, const char* label, const String& signal, float gaugeValue, int y) {
  const int labelX = UI_MARGIN_X + 10;
  const int stateX = UI_MARGIN_X + 10;
  const int gaugeX = UI_MARGIN_X + 10;
  const int gaugeW = M5.Display.width() - UI_MARGIN_X * 2 - 20;
  drawLabeledIcon(icon, label, labelX, y + 4);
  drawUiTextLeft(signalGlyph(signal), stateX, y + 38, uiBodyFont());
  drawMonoIcon(M5.Display.width() - UI_MARGIN_X - 30, y + 34, signalIcon(signal), 1);
  drawUiTextRight(signalMeaning(label, signal), M5.Display.width() - UI_MARGIN_X - 48, y + 40, uiSmallFont());
  drawCenteredGauge(gaugeX, y + 98, gaugeW, 22, gaugeValue);
  M5.Display.drawLine(UI_MARGIN_X, y + 148, M5.Display.width() - UI_MARGIN_X, y + 148, TFT_BLACK);
}

size_t sampledIndex(size_t outIdx, size_t sourceCount, size_t targetCount) {
  if (sourceCount == 0 || targetCount == 0) return 0;
  if (targetCount >= sourceCount || targetCount == 1) return (targetCount == 1) ? (sourceCount - 1) : outIdx;
  return (outIdx * (sourceCount - 1) + (targetCount - 1) / 2) / (targetCount - 1);
}

void drawSimpleLineGraphFloat(int x, int y, int w, int h, const MonoIcon& icon,
                              const float* vals, size_t n, const char* title, const char* unit,
                              const String& startLabel, const String& midLabel, const String& endLabel,
                              size_t maxRenderPoints, bool drawAllMarkers) {
  M5.Display.drawRect(x, y, w, h, TFT_BLACK);
  drawMonoIcon(x + 6, y + 4, icon, 1);
  drawUiTextLeft(title, x + 30, y + 4, uiSmallFont());

  if (n < 2) {
    drawUiTextCenter(ui_text::kNoData, x + w / 2, y + h / 2 - 8, uiSmallFont());
    return;
  }

  float vMin = vals[0];
  float vMax = vals[0];
  for (size_t i = 1; i < n; ++i) {
    if (isnan(vals[i])) continue;
    if (vals[i] < vMin) vMin = vals[i];
    if (vals[i] > vMax) vMax = vals[i];
  }
  if (fabs(vMax - vMin) < 0.001f) {
    vMin -= 1.0f;
    vMax += 1.0f;
  }

  char bufMin[32], bufMax[32];
  snprintf(bufMax, sizeof(bufMax), "%.1f %s", vMax, unit);
  snprintf(bufMin, sizeof(bufMin), "%.1f %s", vMin, unit);

  M5.Display.drawRightString(bufMax, x + w - 6, y + 4, &fonts::Font2);
  M5.Display.drawRightString(bufMin, x + w - 6, y + h - 36, &fonts::Font2);
  M5.Display.drawString(startLabel, x + 6, y + h - 18, &fonts::Font2);
  M5.Display.drawCentreString(midLabel, x + w / 2, y + h - 18, &fonts::Font2);
  M5.Display.drawRightString(endLabel, x + w - 6, y + h - 18, &fonts::Font2);

  int gx = x + 10;
  int gy = y + 24;
  int gw = w - 20;
  int gh = h - 36;
  size_t renderN = n;
  if (maxRenderPoints >= 2 && renderN > maxRenderPoints) {
    renderN = maxRenderPoints;
  }

  int prevX = -1;
  int prevY = -1;
  int lastPx = -1;
  int lastPy = -1;
  for (size_t i = 0; i < renderN; ++i) {
    size_t sourceIdx = sampledIndex(i, n, renderN);
    float v = vals[sourceIdx];
    if (isnan(v)) continue;
    int px = gx + (int)((float)i / (float)(renderN - 1) * gw);
    int py = gy + gh - (int)(((v - vMin) / (vMax - vMin)) * gh);
    if (prevX >= 0) M5.Display.drawLine(prevX, prevY, px, py, TFT_BLACK);
    if (drawAllMarkers) {
      M5.Display.fillCircle(px, py, 2, TFT_BLACK);
    }
    prevX = px;
    prevY = py;
    lastPx = px;
    lastPy = py;
  }
  if (!drawAllMarkers && lastPx >= 0) {
    M5.Display.fillCircle(lastPx, lastPy, 2, TFT_BLACK);
  }
}

void drawSlideSummary() {
  drawHeader(ui_text::kSlide1Title);
  const int cardW = M5.Display.width() - UI_MARGIN_X * 2;
  const int currentY = 92;
  const int currentH = 278;
  const int changeY = 404;
  const int changeH = 488;
  const int innerX = UI_MARGIN_X + 20;
  const int innerW = cardW - 40;
  const int colGap = 34;
  const int colW = (innerW - colGap) / 2;
  const int rightColX = innerX + colW + colGap;
  const int leftUnitX = innerX + 108;
  const int rightUnitX = rightColX + 164;

  String pArrow = arrowForDelta(g_envHist.count >= 2 ? g_envHist.at(g_envHist.count - 1).pressure - g_envHist.at(0).pressure : NAN);
  String hArrow = arrowForDelta(g_envHist.count >= 2 ? g_envHist.at(g_envHist.count - 1).humidity - g_envHist.at(0).humidity : NAN);
  String lArrow = arrowForDelta(g_luxMeta.delta);
  bool lightActive = isLightRainFactorActive();
  String lightDisplay = lightActive ? lArrow : String("NIGHT");
  int rainDenom = lightActive ? 3 : 2;
  int rainSigns = (isRainSign(ui_text::kPressure, pArrow) ? 1 : 0) +
                  (isRainSign(ui_text::kHumidity, hArrow) ? 1 : 0) +
                  ((lightActive && isRainSign(ui_text::kLight, lArrow)) ? 1 : 0);
  String luxValue = formatFloat1(g_luxRaw.lux);
  if (!lightActive && g_luxRaw.valid) {
    luxValue += "  ";
    luxValue += ui_text::kNight;
  }

  drawCard(UI_MARGIN_X, currentY, cardW, currentH, ui_text::kCurrentValues);
  drawMetricWithIcon(ICON_TEMP, ui_text::kTemp, formatFloat1(g_env4.temperature), "C", innerX, 126, leftUnitX);
  drawMetricWithIcon(ICON_LIGHT, ui_text::kLux, luxValue, "", rightColX, 126, 0);
  drawMetricWithIcon(ICON_HUMIDITY, ui_text::kHum, formatFloat1(g_env4.humidity), "%", innerX, 248, leftUnitX);
  drawMetricWithIcon(ICON_PRESSURE, ui_text::kPres, formatFloat1(g_env4.pressure), "hPa", rightColX, 248, rightUnitX);

  drawCard(UI_MARGIN_X, changeY, cardW, changeH, ui_text::kRecentChanges);
  drawChangeSummaryRow(ICON_PRESSURE, ui_text::kPressure, pArrow, 450);
  drawChangeSummaryRow(ICON_HUMIDITY, ui_text::kHumidity, hArrow, 504);
  drawChangeSummaryRow(ICON_LIGHT, ui_text::kLight, lightDisplay, 558);
  M5.Display.drawLine(innerX, 610, UI_MARGIN_X + cardW - 20, 610, TFT_BLACK);
  char rainSignsBuf[32];
  snprintf(rainSignsBuf, sizeof(rainSignsBuf), ui_text::kRainSignsFmt, rainSigns, rainDenom);
  drawUiTextLeft(rainSignsBuf, innerX, 634, uiBodyFont());
  drawPatternSummaryRow(innerX, 668, ui_text::kNow, pArrow, hArrow, lArrow, lightActive, false);
  drawPatternSummaryRow(innerX, 704, ui_text::kRainPattern, pArrow, hArrow, lArrow, lightActive, true);
  drawUiTextLeft(lightActive ? ui_text::kDayRule : ui_text::kNightRule,
                 innerX + 76, 748, uiSmallFont());

  drawFooter();
}

void drawSlideSignals() {
  drawHeader(ui_text::kSlide2Title);

  String pArrow = arrowForDelta(g_envHist.count >= 2 ? g_envHist.at(g_envHist.count - 1).pressure - g_envHist.at(0).pressure : NAN);
  String hArrow = arrowForDelta(g_envHist.count >= 2 ? g_envHist.at(g_envHist.count - 1).humidity - g_envHist.at(0).humidity : NAN);
  String lArrow = arrowForDelta(g_luxMeta.delta);
  bool lightActive = isLightRainFactorActive();
  String lightDisplay = lightActive ? lArrow : String("NIGHT");
  int rainDenom = lightActive ? 3 : 2;
  int rainSigns = (isRainSign(ui_text::kPressure, pArrow) ? 1 : 0) +
                  (isRainSign(ui_text::kHumidity, hArrow) ? 1 : 0) +
                  ((lightActive && isRainSign(ui_text::kLight, lArrow)) ? 1 : 0);

  drawSignalRow(ICON_PRESSURE, ui_text::kPressure, pArrow, normalizedPressureTrend(), 88);
  drawSignalRow(ICON_HUMIDITY, ui_text::kHumidity, hArrow, normalizedHumidityTrend(), 242);
  drawSignalRow(ICON_LIGHT, ui_text::kLight, lightDisplay, lightActive ? normalizedLuxTrend() : 0.0f, 396);

  drawCard(UI_MARGIN_X, 544, M5.Display.width() - UI_MARGIN_X * 2, 278, ui_text::kInterpret);
  drawUiTextLeft(ui_text::kCheckOrder, UI_MARGIN_X + 18, 574, uiSmallFont());
  char rainSignsBuf[32];
  snprintf(rainSignsBuf, sizeof(rainSignsBuf), ui_text::kRainSignsFmt, rainSigns, rainDenom);
  drawUiTextLeft(rainSignsBuf, UI_MARGIN_X + 18, 608, uiBodyFont());
  drawPatternSummaryRow(UI_MARGIN_X + 18, 644, ui_text::kNow, pArrow, hArrow, lArrow, lightActive, false);
  drawPatternSummaryRow(UI_MARGIN_X + 18, 680, ui_text::kRainPattern, pArrow, hArrow, lArrow, lightActive, true);
  drawUiTextLeft(lightActive ? ui_text::kDayRule : ui_text::kNightRule,
                 UI_MARGIN_X + 76, 724, uiSmallFont());
  drawUiTextLeft(ui_text::kWhatChangedFirst, UI_MARGIN_X + 18, 774, uiSmallFont());
  drawUiTextRight(ui_text::kRainComing, M5.Display.width() - UI_MARGIN_X - 34, 774, uiSmallFont());

  drawFooter();
}

void drawTrendGraphsSlide(const char* title, uint32_t targetWindowMin, const char* prompt) {
  drawHeader(title);

  size_t envN = collectEnvWindow(g_pressureGraphVals, g_humidityGraphVals, g_envGraphTs, targetWindowMin);
  size_t luxN = collectLuxWindow(g_luxGraphVals, g_luxGraphTs, targetWindowMin);

  uint32_t oldestTs = 0;
  uint32_t newestTs = 0;
  if (envN > 0) {
    oldestTs = g_envGraphTs[0];
    newestTs = g_envGraphTs[envN - 1];
  }
  if (luxN > 0) {
    if (oldestTs == 0 || g_luxGraphTs[0] < oldestTs) oldestTs = g_luxGraphTs[0];
    if (g_luxGraphTs[luxN - 1] > newestTs) newestTs = g_luxGraphTs[luxN - 1];
  }
  uint32_t spanMin = (oldestTs > 0 && newestTs > oldestTs) ? ((newestTs - oldestTs) / 60) : 0;
  bool compressed = targetWindowMin >= LONG_WINDOW_MIN;
  size_t maxRenderPoints = compressed ? 48 : HISTORY_CAP;
  bool drawAllMarkers = !compressed;

  String startLabel = formatClockOnly(oldestTs);
  String midLabel = formatClockOnly((oldestTs > 0 && newestTs >= oldestTs) ? (oldestTs + (newestTs - oldestTs) / 2) : 0);
  String endLabel = formatClockOnly(newestTs);
  drawSimpleLineGraphFloat(UI_MARGIN_X, 92, M5.Display.width() - UI_MARGIN_X * 2, 188,
                           ICON_PRESSURE, g_pressureGraphVals, envN, ui_text::kPressure, "hPa",
                           startLabel, midLabel, endLabel, maxRenderPoints, drawAllMarkers);
  drawSimpleLineGraphFloat(UI_MARGIN_X, 302, M5.Display.width() - UI_MARGIN_X * 2, 188,
                           ICON_HUMIDITY, g_humidityGraphVals, envN, ui_text::kHumidity, "%",
                           startLabel, midLabel, endLabel, maxRenderPoints, drawAllMarkers);
  drawSimpleLineGraphFloat(UI_MARGIN_X, 512, M5.Display.width() - UI_MARGIN_X * 2, 188,
                           ICON_LIGHT, g_luxGraphVals, luxN, ui_text::kLux, "",
                           startLabel, midLabel, endLabel, maxRenderPoints, drawAllMarkers);

  M5.Display.drawLine(UI_MARGIN_X, 726, M5.Display.width() - UI_MARGIN_X, 726, TFT_BLACK);
  char nowBuf[64];
  snprintf(nowBuf, sizeof(nowBuf), ui_text::kNowPromptFmt, prompt);
  drawUiTextLeft(nowBuf, UI_MARGIN_X + 4, 740, uiSmallFont());
  char windowBuf[96];
  snprintf(windowBuf, sizeof(windowBuf), ui_text::kWindowFmt,
           formatClockOnly(oldestTs).c_str(), formatClockOnly(newestTs).c_str(),
           (unsigned long)spanMin, (unsigned long)targetWindowMin);
  drawUiTextRight(windowBuf, M5.Display.width() - UI_MARGIN_X, 740, uiSmallFont());
  drawSummaryIconRow(ICON_PRESSURE, ui_text::kPres, formatFloat1(g_env4.pressure) + " hPa", UI_MARGIN_X + 16, 772);
  drawSummaryIconRow(ICON_HUMIDITY, ui_text::kHum, formatFloat1(g_env4.humidity) + " %", UI_MARGIN_X + 16, 804);
  drawSummaryIconRow(ICON_LIGHT, ui_text::kLux, formatFloat1(g_luxRaw.lux), UI_MARGIN_X + 16, 836);

  drawFooter();
}

void drawSlideGraphsShort() {
  drawTrendGraphsSlide(ui_text::kSlide3Title, SHORT_WINDOW_MIN, ui_text::kShortPrompt);
}

void drawSlideGraphsLong() {
  drawTrendGraphsSlide(ui_text::kSlide4Title, LONG_WINDOW_MIN, ui_text::kLongPrompt);
}

void drawSlideStatus() {
  drawHeader(ui_text::kStatusTitle);
  const int cardW = M5.Display.width() - UI_MARGIN_X * 2;
  const int healthX = UI_MARGIN_X + 18;
  const int healthRight = M5.Display.width() - UI_MARGIN_X - 22;
  const int networkX = UI_MARGIN_X + 18;
  const int networkRight = M5.Display.width() - UI_MARGIN_X - 22;

  drawCard(UI_MARGIN_X, 92, cardW, 252, ui_text::kHealth);
  drawMonoIcon(M5.Display.width() - UI_MARGIN_X - 38, 102, ICON_SENSOR, 1);
  drawTextRowAligned(ui_text::kSensor, String(g_luxStatus.sensor_ready ? "READY" : "FAIL"),
                     healthX, healthRight - 28, 170, &fonts::Font4);
  drawTextRowAligned(ui_text::kStatus, String(g_luxStatus.status),
                     healthX, healthRight - 28, 224, &fonts::Font4);
  drawTextRowAligned(ui_text::kReason, String(g_luxStatus.reason),
                     healthX, healthRight - 28, 278, &fonts::Font4);

  drawCard(UI_MARGIN_X, 370, cardW, 396, ui_text::kNetwork);
  drawMonoIcon(M5.Display.width() - UI_MARGIN_X - 38, 380, ICON_WIFI, 1);
  drawTextRowAligned(ui_text::kWifi, String(g_luxStatus.wifi),
                     networkX, networkRight - 28, 444, &fonts::Font4);
  drawTextRowAligned(ui_text::kIp, String(g_luxStatus.ip),
                     networkX, networkRight - 28, 496, &fonts::Font4);
  drawTextRowAligned(ui_text::kErrCnt, String(g_luxStatus.sensor_error_count),
                     networkX, networkRight - 28, 548, &fonts::Font4);
  drawMonoIcon(networkX, 618, ICON_MQTT, 1);
  drawTextRowAligned(ui_text::kMqttRetry, String(g_luxStatus.mqtt_reconnect_count),
                     networkX, networkRight - 28, 600, &fonts::Font4);
  M5.Display.drawLine(UI_MARGIN_X + 18, 670, M5.Display.width() - UI_MARGIN_X - 18, 670, TFT_BLACK);
  drawMonoIcon(networkX, 680, ICON_CLOCK, 1);
  drawTextRowAligned(ui_text::kUpdated, formatUnixTime(g_luxStatus.unix_time),
                     networkX, networkRight - 28, 698, isJapaneseUi() ? uiSmallFont() : &fonts::Font2);

  if (strcmp(g_luxStatus.status, "ok") != 0) {
    const int warnX = 20;
    const int warnY = M5.Display.height() - 108;
    const int warnW = M5.Display.width() - 40;
    const int warnH = 38;
    M5.Display.fillRect(warnX, warnY, warnW, warnH, TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    drawMonoIcon(warnX + 10, warnY + 10, ICON_WARNING, 1);
    drawUiTextCenter(ui_text::kWarningSensorMqtt, M5.Display.width() / 2, warnY + 10, uiSmallFont(), TFT_WHITE, TFT_BLACK);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  }

  drawFooter();
}

void renderSlide() {
  switch (g_currentSlide) {
    case 0: drawSlideSummary(); break;
    case 1: drawSlideSignals(); break;
    case 2: drawSlideGraphsShort(); break;
    case 3: drawSlideGraphsLong(); break;
    case STATUS_SCREEN_INDEX: drawSlideStatus(); break;
    default: drawSlideSummary(); break;
  }
}

// ---------------------- setup / loop -------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[SETUP] begin");

  auto cfg = M5.config();
  cfg.external_spk = false;
  Serial.println("[SETUP] calling M5.begin()");
  M5.begin(cfg);
  Serial.println("[SETUP] M5.begin() done");

  // Portrait layout fits the teaching flow better.
  M5.Display.setRotation(0);
  M5.update();
  Serial.println("[SETUP] display rotation done");

  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  Serial.println("[SETUP] EPD mode set");

  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  drawUiTextLeft(ui_text::kBooting, 30, 30, uiBodyFont());
  Serial.println("[SETUP] boot screen drawn");

  g_sdReady = SD.begin(GPIO_NUM_47, SPI, 40000000);  // 40 MHz for stable SD access on PaperS3.
  Serial.printf("[SETUP] SD.begin() -> %s\n", g_sdReady ? "ok" : "ng");
  if (g_sdReady) {
    ensureLogDirs();
    loadLatestState();
    restoreHistoryFromCsv();
    Serial.println("[SETUP] state restored");
  }

  WiFi.mode(WIFI_STA);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.println("[SETUP] network configured");

  connectWiFiIfNeeded();
  syncNtpIfNeeded();
  Serial.println("[SETUP] initial connect attempts done");

  g_needRedraw = true;
  g_lastSlideMs = millis();
  g_lastRefreshMs = millis() - EPD_REFRESH_MS;
  g_lastStateSaveMs = millis();
  Serial.println("[SETUP] ready");
}

void handleButtons() {
  if (M5.BtnA.wasClicked()) {
    if (g_currentSlide == STATUS_SCREEN_INDEX) {
      g_currentSlide = g_lastMainSlide;
    } else {
      g_currentSlide = (g_currentSlide + MAIN_SLIDE_COUNT - 1) % MAIN_SLIDE_COUNT;
      g_lastMainSlide = g_currentSlide;
    }
    g_lastSlideMs = millis();
    g_needRedraw = true;
  }
  if (M5.BtnC.wasClicked()) {
    if (g_currentSlide == STATUS_SCREEN_INDEX) {
      g_currentSlide = g_lastMainSlide;
    } else {
      g_currentSlide = (g_currentSlide + 1) % MAIN_SLIDE_COUNT;
      g_lastMainSlide = g_currentSlide;
    }
    g_lastSlideMs = millis();
    g_needRedraw = true;
  }
  if (M5.BtnB.wasClicked()) {
    g_needRedraw = true;  // manual refresh
  }

  if (M5.Touch.getCount()) {
    const auto& touch = M5.Touch.getDetail();
    if (touch.wasClicked() && footerButtonContains(touch.x, touch.y)) {
      if (g_currentSlide == STATUS_SCREEN_INDEX) {
        g_currentSlide = g_lastMainSlide;
      } else {
        g_lastMainSlide = g_currentSlide;
        g_currentSlide = STATUS_SCREEN_INDEX;
      }
      g_lastSlideMs = millis();
      g_needRedraw = true;
    } else if (isStatusSwipe(touch)) {
      g_lastMainSlide = g_currentSlide;
      g_currentSlide = STATUS_SCREEN_INDEX;
      g_lastSlideMs = millis();
      g_needRedraw = true;
    }
  }
}

void loop() {
  M5.update();
  bool manualRefresh = M5.BtnB.wasClicked();

  handleButtons();
  connectWiFiIfNeeded();
  syncNtpIfNeeded();
  ensureMqttConnected();

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  uint32_t nowMs = millis();
  if (nowMs - g_lastSlideMs >= SLIDE_INTERVAL_MS) {
    if (g_currentSlide != STATUS_SCREEN_INDEX) {
      g_currentSlide = (g_currentSlide + 1) % MAIN_SLIDE_COUNT;
      g_lastMainSlide = g_currentSlide;
    }
    g_lastSlideMs = nowMs;
    g_needRedraw = true;
  }

  if (g_sdReady && nowMs - g_lastStateSaveMs >= STATE_SAVE_MS) {
    Serial.println("[LOOP] periodic state save");
    saveLatestState();
    g_lastStateSaveMs = nowMs;
  }

  if (g_needRedraw && (nowMs - g_lastRefreshMs >= EPD_REFRESH_MS || manualRefresh)) {
    Serial.printf("[LOOP] render slide=%u now=%lu last=%lu\n", g_currentSlide, (unsigned long)nowMs, (unsigned long)g_lastRefreshMs);
    renderSlide();
    Serial.println("[LOOP] render done");
    g_lastRefreshMs = nowMs;
    g_needRedraw = false;
  }

  delay(20);
}
