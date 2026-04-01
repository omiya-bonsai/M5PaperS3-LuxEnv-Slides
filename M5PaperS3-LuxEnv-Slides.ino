#include <SD.h>            // IMPORTANT: keep before M5Unified on M5GFX/M5Unified projects
#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"

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
static constexpr uint32_t EPD_REFRESH_MS       = 30000;
static constexpr uint32_t WIFI_RETRY_MS        = 5000;
static constexpr uint32_t NTP_RETRY_MS         = 60000;
static constexpr uint32_t STATE_SAVE_MS        = 30000;

// ---------------------- history ------------------------------
static constexpr size_t HISTORY_CAP = 72;  // 72 x 5 min = 6h if upstream is 5 min
                                      // also works for shorter intervals as a rolling window

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

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

uint8_t g_currentSlide = 0;
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
  time_t t = static_cast<time_t>(ts);
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);

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
  doc["saved_at"] = getLocalTime(nullptr, 1) ? (uint32_t)time(nullptr) : 0;

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
  g_needRedraw = true;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

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
}

// ---------------------- drawing helpers ----------------------
void drawHeader(const char* title) {
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);

  M5.Display.fillRect(0, 0, M5.Display.width(), 44, TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(title, 16, 12, &fonts::Font4);

  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawLine(0, 46, M5.Display.width(), 46, TFT_BLACK);
}

void drawFooter() {
  String ts = formatUnixTime((uint32_t)safeLatestTs());
  String net = (WiFi.status() == WL_CONNECTED) ? "WIFI OK" : "WIFI NG";
  String mq = mqttClient.connected() ? "MQTT OK" : "MQTT NG";

  M5.Display.drawLine(0, M5.Display.height() - 34, M5.Display.width(), M5.Display.height() - 34, TFT_BLACK);
  M5.Display.drawString(ts, 16, M5.Display.height() - 28, &fonts::Font2);
  M5.Display.drawRightString(net + "  " + mq, M5.Display.width() - 16, M5.Display.height() - 28, &fonts::Font2);
}

void drawKeyValue(const char* label, const String& value, int x, int y, const lgfx::IFont* valueFont = &fonts::Font6) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(label, x, y, &fonts::Font2);
  M5.Display.drawString(value, x, y + 18, valueFont);
}

void drawTrendBadge(const char* trend, int x, int y) {
  int w = 150;
  int h = 36;
  M5.Display.drawRect(x, y, w, h, TFT_BLACK);
  M5.Display.drawCentreString(trend, x + w / 2, y + 8, &fonts::Font2);
}

void drawSimpleLineGraphFloat(int x, int y, int w, int h, const float* vals, size_t n, const char* title, const char* unit) {
  M5.Display.drawRect(x, y, w, h, TFT_BLACK);
  M5.Display.drawString(title, x + 8, y + 6, &fonts::Font2);

  if (n < 2) {
    M5.Display.drawCentreString("NO DATA", x + w / 2, y + h / 2 - 8, &fonts::Font2);
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

  M5.Display.drawRightString(bufMax, x + w - 8, y + 6, &fonts::Font2);
  M5.Display.drawRightString(bufMin, x + w - 8, y + h - 22, &fonts::Font2);

  int gx = x + 10;
  int gy = y + 28;
  int gw = w - 20;
  int gh = h - 44;

  int prevX = -1;
  int prevY = -1;
  for (size_t i = 0; i < n; ++i) {
    float v = vals[i];
    if (isnan(v)) continue;
    int px = gx + (int)((float)i / (float)(n - 1) * gw);
    int py = gy + gh - (int)(((v - vMin) / (vMax - vMin)) * gh);
    if (prevX >= 0) M5.Display.drawLine(prevX, prevY, px, py, TFT_BLACK);
    M5.Display.fillCircle(px, py, 2, TFT_BLACK);
    prevX = px;
    prevY = py;
  }
}

void drawSlideSummary() {
  drawHeader("SLIDE 1  CURRENT");
  drawKeyValue("TEMP", formatFloat1(g_env4.temperature) + " C", 24, 68);
  drawKeyValue("HUM", formatFloat1(g_env4.humidity) + " %", 24, 178);
  drawKeyValue("PRES", formatFloat1(g_env4.pressure) + " hPa", 24, 288, &fonts::Font4);
  drawKeyValue("LUX", formatFloat1(g_luxRaw.lux), 24, 400);

  drawKeyValue("LUX AVG", formatFloat1(g_luxMeta.avg), 290, 68);
  drawKeyValue("LUX DELTA", formatFloat1(g_luxMeta.delta), 290, 178);
  drawKeyValue("LUX RATE", formatFloat2(g_luxMeta.rate_pct) + " %", 290, 288, &fonts::Font4);

  M5.Display.drawString("TREND", 290, 400, &fonts::Font2);
  drawTrendBadge(g_luxMeta.trend, 290, 426);

  drawFooter();
}

void drawSlideSignals() {
  drawHeader("SLIDE 2  SIGNALS");

  String pArrow = arrowForDelta(g_envHist.count >= 2 ? g_envHist.at(g_envHist.count - 1).pressure - g_envHist.at(0).pressure : NAN);
  String hArrow = arrowForDelta(g_envHist.count >= 2 ? g_envHist.at(g_envHist.count - 1).humidity - g_envHist.at(0).humidity : NAN);
  String lArrow = arrowForDelta(g_luxMeta.delta);

  M5.Display.drawString("PRESSURE", 40, 90, &fonts::Font4);
  M5.Display.drawString(pArrow, 360, 90, &fonts::Font6);

  M5.Display.drawString("HUMIDITY", 40, 210, &fonts::Font4);
  M5.Display.drawString(hArrow, 360, 210, &fonts::Font6);

  M5.Display.drawString("LIGHT", 40, 330, &fonts::Font4);
  M5.Display.drawString(lArrow, 360, 330, &fonts::Font6);

  M5.Display.drawString("QUESTION", 40, 470, &fonts::Font4);
  M5.Display.drawString("RAIN COMING?", 220, 470, &fonts::Font4);

  drawFooter();
}

void drawSlideGraphs() {
  drawHeader("SLIDE 3  GRAPHS");

  float pressureVals[HISTORY_CAP];
  float humidityVals[HISTORY_CAP];
  float luxVals[HISTORY_CAP];

  size_t envN = g_envHist.count;
  for (size_t i = 0; i < envN; ++i) {
    pressureVals[i] = g_envHist.at(i).pressure;
    humidityVals[i] = g_envHist.at(i).humidity;
  }

  size_t luxN = g_luxHist.count;
  for (size_t i = 0; i < luxN; ++i) {
    luxVals[i] = g_luxHist.at(i).lux;
  }

  drawSimpleLineGraphFloat(20, 60, 500, 230, pressureVals, envN, "PRESSURE", "hPa");
  drawSimpleLineGraphFloat(20, 310, 240, 220, humidityVals, envN, "HUMIDITY", "%");
  drawSimpleLineGraphFloat(280, 310, 240, 220, luxVals, luxN, "LUX", "");

  drawFooter();
}

void drawSlideStatus() {
  drawHeader("SLIDE 4  STATUS");

  drawKeyValue("SENSOR", String(g_luxStatus.sensor_ready ? "READY" : "FAIL"), 24, 70);
  drawKeyValue("STATUS", String(g_luxStatus.status), 24, 180);
  drawKeyValue("REASON", String(g_luxStatus.reason), 24, 290, &fonts::Font4);

  drawKeyValue("WIFI", String(g_luxStatus.wifi), 290, 70);
  drawKeyValue("IP", String(g_luxStatus.ip), 290, 180, &fonts::Font4);
  drawKeyValue("ERR CNT", String(g_luxStatus.sensor_error_count), 290, 290);
  drawKeyValue("MQTT RETRY", String(g_luxStatus.mqtt_reconnect_count), 290, 400);

  if (strcmp(g_luxStatus.status, "ok") != 0) {
    M5.Display.fillRect(20, 520, 500, 70, TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawCentreString("WARNING: SENSOR / MQTT ISSUE", M5.Display.width() / 2, 544, &fonts::Font2);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  }

  drawFooter();
}

void renderSlide() {
  switch (g_currentSlide) {
    case 0: drawSlideSummary(); break;
    case 1: drawSlideSignals(); break;
    case 2: drawSlideGraphs(); break;
    case 3: drawSlideStatus(); break;
    default: drawSlideSummary(); break;
  }
}

// ---------------------- setup / loop -------------------------
void setup() {
  auto cfg = M5.config();
  cfg.external_spk = false;
  M5.begin(cfg);

  // Landscape gives a wider graph area on PaperS3.
  M5.Display.setRotation(1);
  M5.Display.setEpdMode(epd_mode_t::epd_quality);

  Serial.begin(115200);
  delay(300);

  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString("BOOTING...", 30, 30, &fonts::Font4);

  g_sdReady = SD.begin(GPIO_NUM_47, SPI, 25000000);
  if (g_sdReady) {
    ensureLogDirs();
    loadLatestState();
  }

  WiFi.mode(WIFI_STA);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  connectWiFiIfNeeded();
  syncNtpIfNeeded();

  g_needRedraw = true;
  g_lastSlideMs = millis();
  g_lastRefreshMs = 0;
  g_lastStateSaveMs = millis();
}

void handleButtons() {
  if (M5.BtnA.wasClicked()) {
    g_currentSlide = (g_currentSlide + 3) % 4;
    g_needRedraw = true;
  }
  if (M5.BtnC.wasClicked()) {
    g_currentSlide = (g_currentSlide + 1) % 4;
    g_needRedraw = true;
  }
  if (M5.BtnB.wasClicked()) {
    g_needRedraw = true;  // manual refresh
  }
}

void loop() {
  M5.update();

  handleButtons();
  connectWiFiIfNeeded();
  syncNtpIfNeeded();
  ensureMqttConnected();

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  uint32_t nowMs = millis();
  if (nowMs - g_lastSlideMs >= SLIDE_INTERVAL_MS) {
    g_currentSlide = (g_currentSlide + 1) % 4;
    g_lastSlideMs = nowMs;
    g_needRedraw = true;
  }

  if (g_sdReady && nowMs - g_lastStateSaveMs >= STATE_SAVE_MS) {
    saveLatestState();
    g_lastStateSaveMs = nowMs;
  }

  if (g_needRedraw && (nowMs - g_lastRefreshMs >= EPD_REFRESH_MS || M5.BtnB.wasClicked())) {
    renderSlide();
    g_lastRefreshMs = nowMs;
    g_needRedraw = false;
  }

  delay(20);
}
