#pragma once

#include "config.h"

namespace ui_text_en {

// Header / footer
static constexpr const char* kBatteryPrefix = "BAT ";
static constexpr const char* kBatteryUnknown = "BAT --%";
static constexpr const char* kWifiOk = "WIFI OK";
static constexpr const char* kWifiNg = "WIFI NG";
static constexpr const char* kMqttOk = "MQTT OK";
static constexpr const char* kMqttNg = "MQTT NG";
static constexpr const char* kStatusButton = "STATUS";
static constexpr const char* kBackButton = "BACK";
static constexpr const char* kBooting = "BOOTING...";

// Slide titles
static constexpr const char* kSlide1Title = "SLIDE 1  CURRENT";
static constexpr const char* kSlide2Title = "SLIDE 2  SIGNALS";
static constexpr const char* kSlide3Title = "SLIDE 3  SHORT-TERM (15 min)";
static constexpr const char* kSlide4Title = "SLIDE 4  LONG-TERM (120 min)";
static constexpr const char* kStatusTitle = "STATUS";

// Shared labels
static constexpr const char* kTemp = "TEMP";
static constexpr const char* kHum = "HUM";
static constexpr const char* kPres = "PRES";
static constexpr const char* kLux = "LUX";
static constexpr const char* kPatternPressure = "P";
static constexpr const char* kPatternHumidity = "H";
static constexpr const char* kPatternLight = "L";
static constexpr const char* kPressure = "PRESSURE";
static constexpr const char* kHumidity = "HUMIDITY";
static constexpr const char* kLight = "LIGHT";
static constexpr const char* kCurrentValues = "CURRENT VALUES";
static constexpr const char* kRecentChanges = "RECENT CHANGES";
static constexpr const char* kInterpret = "INTERPRET";
static constexpr const char* kHealth = "HEALTH";
static constexpr const char* kNetwork = "NETWORK";
static constexpr const char* kSensor = "SENSOR";
static constexpr const char* kStatus = "STATUS";
static constexpr const char* kReason = "REASON";
static constexpr const char* kWifi = "WIFI";
static constexpr const char* kIp = "IP";
static constexpr const char* kErrCnt = "ERR CNT";
static constexpr const char* kMqttRetry = "MQTT RETRY";
static constexpr const char* kUpdated = "UPDATED";

// State / meaning words
static constexpr const char* kNoData = "NO DATA";
static constexpr const char* kNight = "NIGHT";
static constexpr const char* kAscend = "ASCEND";
static constexpr const char* kDescend = "DESCEND";
static constexpr const char* kSteady = "STEADY";
static constexpr const char* kRisingFast = "RISING FAST";
static constexpr const char* kRising = "RISING";
static constexpr const char* kFallingFast = "FALLING FAST";
static constexpr const char* kFalling = "FALLING";
static constexpr const char* kStable = "STABLE";
static constexpr const char* kRainSign = "rain sign";
static constexpr const char* kFairSign = "fair sign";
static constexpr const char* kDrySign = "dry sign";
static constexpr const char* kCloudSign = "cloud sign";
static constexpr const char* kBrightSign = "bright sign";
static constexpr const char* kNightSkip = "night skip";
static constexpr const char* kWatch = "watch";

// Teaching text
static constexpr const char* kRainSignsFmt = "Rain signs: %d / %d";
static constexpr const char* kNow = "Now:";
static constexpr const char* kRainPattern = "Rain pattern:";
static constexpr const char* kDayRule = "Pressure down  Humidity up  Light down in daytime";
static constexpr const char* kNightRule = "At night, use pressure and humidity.";
static constexpr const char* kCheckOrder = "Check order: Pressure -> Humidity -> Light";
static constexpr const char* kWhatChangedFirst = "What changed first?";
static constexpr const char* kRainComing = "RAIN COMING?";
static constexpr const char* kNowPromptFmt = "NOW  %s";
static constexpr const char* kWindowFmt = "WINDOW %s - %s  (%lu min / target %lu min)";
static constexpr const char* kShortPrompt = "What is changing now?";
static constexpr const char* kLongPrompt = "Is the trend continuing?";

// Warning
static constexpr const char* kWarningSensorMqtt = "WARNING: SENSOR / MQTT ISSUE";

}  // namespace ui_text_en

namespace ui_text_ja {

// Header / footer
static constexpr const char* kBatteryPrefix = "電池 ";
static constexpr const char* kBatteryUnknown = "電池 --%";
static constexpr const char* kWifiOk = "WIFI OK";
static constexpr const char* kWifiNg = "WIFI NG";
static constexpr const char* kMqttOk = "MQTT OK";
static constexpr const char* kMqttNg = "MQTT NG";
static constexpr const char* kStatusButton = "状態";
static constexpr const char* kBackButton = "戻る";
static constexpr const char* kBooting = "起動中...";

// Slide titles
static constexpr const char* kSlide1Title = "スライド1  現在値";
static constexpr const char* kSlide2Title = "スライド2  変化のサイン";
static constexpr const char* kSlide3Title = "スライド3  短期傾向 (15分)";
static constexpr const char* kSlide4Title = "スライド4  長期傾向 (120分)";
static constexpr const char* kStatusTitle = "状態";

// Shared labels
static constexpr const char* kTemp = "気温";
static constexpr const char* kHum = "湿度";
static constexpr const char* kPres = "気圧";
static constexpr const char* kLux = "明るさ";
static constexpr const char* kPatternPressure = "気圧";
static constexpr const char* kPatternHumidity = "湿度";
static constexpr const char* kPatternLight = "明るさ";
static constexpr const char* kPressure = "気圧";
static constexpr const char* kHumidity = "湿度";
static constexpr const char* kLight = "明るさ";
static constexpr const char* kCurrentValues = "現在値";
static constexpr const char* kRecentChanges = "最近の変化";
static constexpr const char* kInterpret = "考えるヒント";
static constexpr const char* kHealth = "本体";
static constexpr const char* kNetwork = "通信";
static constexpr const char* kSensor = "センサー";
static constexpr const char* kStatus = "状態";
static constexpr const char* kReason = "理由";
static constexpr const char* kWifi = "Wi-Fi";
static constexpr const char* kIp = "IP";
static constexpr const char* kErrCnt = "エラー";
static constexpr const char* kMqttRetry = "MQTT再試行";
static constexpr const char* kUpdated = "更新";

// State / meaning words
static constexpr const char* kNoData = "データなし";
static constexpr const char* kNight = "夜";
static constexpr const char* kAscend = "上向き";
static constexpr const char* kDescend = "下向き";
static constexpr const char* kSteady = "横ばい";
static constexpr const char* kRisingFast = "大きく上昇";
static constexpr const char* kRising = "上昇";
static constexpr const char* kFallingFast = "大きく下降";
static constexpr const char* kFalling = "下降";
static constexpr const char* kStable = "安定";
static constexpr const char* kRainSign = "雨の手がかり";
static constexpr const char* kFairSign = "晴れの手がかり";
static constexpr const char* kDrySign = "乾いた空気";
static constexpr const char* kCloudSign = "雲の手がかり";
static constexpr const char* kBrightSign = "明るい空";
static constexpr const char* kNightSkip = "夜は使わない";
static constexpr const char* kWatch = "ようすを見る";

// Teaching text
static constexpr const char* kRainSignsFmt = "雨の手がかり: %d / %d";
static constexpr const char* kNow = "今:";
static constexpr const char* kRainPattern = "雨が近い並び:";
static constexpr const char* kDayRule = "気圧↓ 湿度↑ 明るさ↓（昼）";
static constexpr const char* kNightRule = "夜は気圧と湿度を見る。";
static constexpr const char* kCheckOrder = "見る順番: 気圧→湿度→明るさ";
static constexpr const char* kWhatChangedFirst = "気圧、湿度、明るさのうち、どれが先に変わった？";
static constexpr const char* kRainComing = "もうすぐ雨が降るかな？";
static constexpr const char* kNowPromptFmt = "%s";
static constexpr const char* kWindowFmt = "期間 %s - %s  (%lu分 / 目標%lu分)";
static constexpr const char* kShortPrompt = "今、何が変わっている？";
static constexpr const char* kLongPrompt = "今までの流れは続いている？";

// Warning
static constexpr const char* kWarningSensorMqtt = "警告: センサー / MQTT異常";

}  // namespace ui_text_ja

#define UI_TEXT_SELECT(name) ((CONFIG_UI_LANG == UI_LANG_JA) ? ui_text_ja::name : ui_text_en::name)

namespace ui_text {

static constexpr const char* kBatteryPrefix = UI_TEXT_SELECT(kBatteryPrefix);
static constexpr const char* kBatteryUnknown = UI_TEXT_SELECT(kBatteryUnknown);
static constexpr const char* kWifiOk = UI_TEXT_SELECT(kWifiOk);
static constexpr const char* kWifiNg = UI_TEXT_SELECT(kWifiNg);
static constexpr const char* kMqttOk = UI_TEXT_SELECT(kMqttOk);
static constexpr const char* kMqttNg = UI_TEXT_SELECT(kMqttNg);
static constexpr const char* kStatusButton = UI_TEXT_SELECT(kStatusButton);
static constexpr const char* kBackButton = UI_TEXT_SELECT(kBackButton);
static constexpr const char* kBooting = UI_TEXT_SELECT(kBooting);

static constexpr const char* kSlide1Title = UI_TEXT_SELECT(kSlide1Title);
static constexpr const char* kSlide2Title = UI_TEXT_SELECT(kSlide2Title);
static constexpr const char* kSlide3Title = UI_TEXT_SELECT(kSlide3Title);
static constexpr const char* kSlide4Title = UI_TEXT_SELECT(kSlide4Title);
static constexpr const char* kStatusTitle = UI_TEXT_SELECT(kStatusTitle);

static constexpr const char* kTemp = UI_TEXT_SELECT(kTemp);
static constexpr const char* kHum = UI_TEXT_SELECT(kHum);
static constexpr const char* kPres = UI_TEXT_SELECT(kPres);
static constexpr const char* kLux = UI_TEXT_SELECT(kLux);
static constexpr const char* kPatternPressure = UI_TEXT_SELECT(kPatternPressure);
static constexpr const char* kPatternHumidity = UI_TEXT_SELECT(kPatternHumidity);
static constexpr const char* kPatternLight = UI_TEXT_SELECT(kPatternLight);
static constexpr const char* kPressure = UI_TEXT_SELECT(kPressure);
static constexpr const char* kHumidity = UI_TEXT_SELECT(kHumidity);
static constexpr const char* kLight = UI_TEXT_SELECT(kLight);
static constexpr const char* kCurrentValues = UI_TEXT_SELECT(kCurrentValues);
static constexpr const char* kRecentChanges = UI_TEXT_SELECT(kRecentChanges);
static constexpr const char* kInterpret = UI_TEXT_SELECT(kInterpret);
static constexpr const char* kHealth = UI_TEXT_SELECT(kHealth);
static constexpr const char* kNetwork = UI_TEXT_SELECT(kNetwork);
static constexpr const char* kSensor = UI_TEXT_SELECT(kSensor);
static constexpr const char* kStatus = UI_TEXT_SELECT(kStatus);
static constexpr const char* kReason = UI_TEXT_SELECT(kReason);
static constexpr const char* kWifi = UI_TEXT_SELECT(kWifi);
static constexpr const char* kIp = UI_TEXT_SELECT(kIp);
static constexpr const char* kErrCnt = UI_TEXT_SELECT(kErrCnt);
static constexpr const char* kMqttRetry = UI_TEXT_SELECT(kMqttRetry);
static constexpr const char* kUpdated = UI_TEXT_SELECT(kUpdated);

static constexpr const char* kNoData = UI_TEXT_SELECT(kNoData);
static constexpr const char* kNight = UI_TEXT_SELECT(kNight);
static constexpr const char* kAscend = UI_TEXT_SELECT(kAscend);
static constexpr const char* kDescend = UI_TEXT_SELECT(kDescend);
static constexpr const char* kSteady = UI_TEXT_SELECT(kSteady);
static constexpr const char* kRisingFast = UI_TEXT_SELECT(kRisingFast);
static constexpr const char* kRising = UI_TEXT_SELECT(kRising);
static constexpr const char* kFallingFast = UI_TEXT_SELECT(kFallingFast);
static constexpr const char* kFalling = UI_TEXT_SELECT(kFalling);
static constexpr const char* kStable = UI_TEXT_SELECT(kStable);
static constexpr const char* kRainSign = UI_TEXT_SELECT(kRainSign);
static constexpr const char* kFairSign = UI_TEXT_SELECT(kFairSign);
static constexpr const char* kDrySign = UI_TEXT_SELECT(kDrySign);
static constexpr const char* kCloudSign = UI_TEXT_SELECT(kCloudSign);
static constexpr const char* kBrightSign = UI_TEXT_SELECT(kBrightSign);
static constexpr const char* kNightSkip = UI_TEXT_SELECT(kNightSkip);
static constexpr const char* kWatch = UI_TEXT_SELECT(kWatch);

static constexpr const char* kRainSignsFmt = UI_TEXT_SELECT(kRainSignsFmt);
static constexpr const char* kNow = UI_TEXT_SELECT(kNow);
static constexpr const char* kRainPattern = UI_TEXT_SELECT(kRainPattern);
static constexpr const char* kDayRule = UI_TEXT_SELECT(kDayRule);
static constexpr const char* kNightRule = UI_TEXT_SELECT(kNightRule);
static constexpr const char* kCheckOrder = UI_TEXT_SELECT(kCheckOrder);
static constexpr const char* kWhatChangedFirst = UI_TEXT_SELECT(kWhatChangedFirst);
static constexpr const char* kRainComing = UI_TEXT_SELECT(kRainComing);
static constexpr const char* kNowPromptFmt = UI_TEXT_SELECT(kNowPromptFmt);
static constexpr const char* kWindowFmt = UI_TEXT_SELECT(kWindowFmt);
static constexpr const char* kShortPrompt = UI_TEXT_SELECT(kShortPrompt);
static constexpr const char* kLongPrompt = UI_TEXT_SELECT(kLongPrompt);

static constexpr const char* kWarningSensorMqtt = UI_TEXT_SELECT(kWarningSensorMqtt);

}  // namespace ui_text

#undef UI_TEXT_SELECT
