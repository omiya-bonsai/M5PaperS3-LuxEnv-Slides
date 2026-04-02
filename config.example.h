#pragma once

// Copy this file to `config.h` and replace the placeholder values below.

// Wi-Fi settings used by the M5PaperS3 itself.
// Use an SSID/password pair for a 2.4 GHz network that the device can join.
static constexpr const char* WIFI_SSID = "YOUR_WIFI_SSID";
static constexpr const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// MQTT broker settings.
// `MQTT_BROKER` can be a local IP address or a hostname reachable from the device.
static constexpr const char* MQTT_BROKER = "broker.local";
static constexpr uint16_t    MQTT_PORT   = 1883;

// NTP / timezone / site location.
// The timezone string is used for time display and NTP conversion.
// Latitude/longitude are used for sunrise/sunset estimation so the UI can
// decide when light should be treated as a daytime rain clue.
static constexpr const char* CONFIG_NTP_SERVER_1 = "ntp.nict.jp";
static constexpr const char* CONFIG_NTP_SERVER_2 = "pool.ntp.org";
static constexpr const char* CONFIG_TZ_INFO      = "JST-9";
static constexpr double CONFIG_SITE_LATITUDE     = 35.8617;
static constexpr double CONFIG_SITE_LONGITUDE    = 139.6455;

// UI language selection.
// Use `UI_LANG_EN` for English or `UI_LANG_JA` for Japanese.
static constexpr uint8_t UI_LANG_EN = 0;
static constexpr uint8_t UI_LANG_JA = 1;
static constexpr uint8_t CONFIG_UI_LANG = UI_LANG_EN;
