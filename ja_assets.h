#pragma once

#include <M5Unified.h>

// Japanese-capable font aliases prepared for future localization work.
// These use built-in M5GFX Japanese Gothic fonts, so no SD assets are required.
namespace ja_assets {

static constexpr const lgfx::IFont* kLabelFont = &fonts::lgfxJapanGothic_12;
static constexpr const lgfx::IFont* kBodyFont = &fonts::lgfxJapanGothic_16;
static constexpr const lgfx::IFont* kTitleFont = &fonts::lgfxJapanGothic_20;
static constexpr const lgfx::IFont* kHeadlineFont = &fonts::lgfxJapanGothic_24;

// Current UI vocabulary expected during the Japanese localization phase.
static constexpr const char* kPreparedTerms[] = {
    "現在値",
    "最近の変化",
    "気圧",
    "湿度",
    "照度",
    "雨の手がかり",
    "短期傾向",
    "長期傾向",
    "状態",
    "雨が近い？",
    "何が変わった？",
    "流れは続いている？",
};

static constexpr size_t kPreparedTermsCount =
    sizeof(kPreparedTerms) / sizeof(kPreparedTerms[0]);

}  // namespace ja_assets
