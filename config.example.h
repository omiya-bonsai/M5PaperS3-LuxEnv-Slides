#pragma once

// Wi-Fi
static constexpr const char* WIFI_SSID = "YOUR_WIFI_SSID";
static constexpr const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// MQTT
static constexpr const char* MQTT_BROKER = "192.168.3.82";
static constexpr uint16_t    MQTT_PORT   = 1883;

// NTP / timezone
static constexpr const char* CONFIG_NTP_SERVER_1 = "ntp.nict.jp";
static constexpr const char* CONFIG_NTP_SERVER_2 = "pool.ntp.org";
static constexpr const char* CONFIG_TZ_INFO      = "JST-9";
static constexpr double CONFIG_SITE_LATITUDE     = 35.8617;
static constexpr double CONFIG_SITE_LONGITUDE    = 139.6455;
