/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WordyWatchDisplay.cpp
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "WordyWatchDisplay.h"

#include "RaftUtils.h"
#include "RaftDevice.h"
#include "RTC.h"
#include "wordclock_patterns.h"
#include <cmath>

// Debug control - uncomment to enable specific debugging
#define DEBUG_TIME_DISPLAY

namespace
{
    static constexpr const char* MODULE_PREFIX = "WordyWatchDisplay";
    static constexpr uint16_t MINUTE_INDICATOR_X = 0;
    static constexpr uint16_t MINUTE_INDICATOR_Y_START = 3;
    static constexpr uint16_t MINUTE_INDICATOR_COUNT = 5;

    void setLedInMask(uint32_t* maskWords, uint16_t x, uint16_t y)
    {
        if (x >= LED_GRID_WIDTH || y >= LED_GRID_HEIGHT)
            return;
        const uint16_t ledIndex = y * LED_GRID_WIDTH + x;
        const uint16_t wordIndex = ledIndex / 32;
        const uint8_t bitIndex = ledIndex % 32;
        maskWords[wordIndex] |= (1U << bitIndex);
    }
}

WordyWatchDisplay::WordyWatchDisplay(RaftSysMod& sysMod)
    : _sysMod(sysMod)
{
}

void WordyWatchDisplay::showTime(RTC& rtc)
{
    // Get time from RTC (falling back to system time if RTC read fails)
    struct tm timeinfo = {};
    if (rtc.getTime(&timeinfo))
    {
#ifdef DEBUG_TIME_DISPLAY
        LOG_I(MODULE_PREFIX, "showTime showing time %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
#endif
        const led_pattern_t* pattern = getPatternForTime(timeinfo.tm_hour, timeinfo.tm_min);
        if (!pattern)
        {
            LOG_W(MODULE_PREFIX, "showTime pattern not found for %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            return;
        }

        sendMaskToPanel(pattern->led_mask, LED_MASK_WORDS);
    }
#ifdef DEBUG_TIME_DISPLAY
    else
    {
        LOG_E(MODULE_PREFIX, "showTime failed to get time");
    }
#endif
}

void WordyWatchDisplay::showTimeWithMinuteIndicators(const struct tm& timeinfo)
{
    const led_pattern_t* pattern = getPatternForTime(timeinfo.tm_hour, timeinfo.tm_min);
    if (!pattern)
    {
        LOG_W(MODULE_PREFIX, "showTimeWithMinuteIndicators pattern not found for %02d:%02d",
              timeinfo.tm_hour, timeinfo.tm_min);
        return;
    }

    uint32_t mask[LED_MASK_WORDS] = {};
    for (size_t idx = 0; idx < LED_MASK_WORDS; idx++)
        mask[idx] = pattern->led_mask[idx];

    const uint8_t minuteOffset = static_cast<uint8_t>(timeinfo.tm_min % LED_MINUTE_GRANULARITY);
    const uint8_t solidCount = minuteOffset;
    const uint8_t flashIndex = minuteOffset;
    const bool flashOn = (timeinfo.tm_sec % 2) == 0;

    for (uint8_t idx = 0; idx < solidCount && idx < MINUTE_INDICATOR_COUNT; idx++)
    {
        setLedInMask(mask, MINUTE_INDICATOR_X, MINUTE_INDICATOR_Y_START + idx);
    }

    if (flashOn && flashIndex < MINUTE_INDICATOR_COUNT)
    {
        setLedInMask(mask, MINUTE_INDICATOR_X, MINUTE_INDICATOR_Y_START + flashIndex);
    }

    sendMaskToPanel(mask, LED_MASK_WORDS);
}

void WordyWatchDisplay::showBatteryGauge(uint8_t ledCount)
{
    if (ledCount > LED_GRID_WIDTH)
        ledCount = LED_GRID_WIDTH;

    uint32_t mask[LED_MASK_WORDS] = {};
    const uint16_t baseIndex = (LED_GRID_HEIGHT - 1) * LED_GRID_WIDTH;

    for (uint16_t col = 0; col < ledCount; col++)
    {
        const uint16_t ledIndex = baseIndex + col;
        const uint16_t wordIndex = ledIndex / 32;
        const uint8_t bitIndex = ledIndex % 32;
        mask[wordIndex] |= (1U << bitIndex);
    }

    sendMaskToPanel(mask, LED_MASK_WORDS);
}

void WordyWatchDisplay::showBatteryGaugeWithMinuteIndicators(uint8_t ledCount)
{
    if (ledCount > LED_GRID_WIDTH)
        ledCount = LED_GRID_WIDTH;

    uint32_t mask[LED_MASK_WORDS] = {};
    const uint16_t baseIndex = (LED_GRID_HEIGHT - 1) * LED_GRID_WIDTH;

    for (uint16_t col = 0; col < ledCount; col++)
    {
        const uint16_t ledIndex = baseIndex + col;
        const uint16_t wordIndex = ledIndex / 32;
        const uint8_t bitIndex = ledIndex % 32;
        mask[wordIndex] |= (1U << bitIndex);
    }

    for (uint8_t idx = 0; idx < MINUTE_INDICATOR_COUNT; idx++)
    {
        setLedInMask(mask, MINUTE_INDICATOR_X, MINUTE_INDICATOR_Y_START + idx);
    }

    sendMaskToPanel(mask, LED_MASK_WORDS);
}

void WordyWatchDisplay::showBreakoutFrame(int paddleTop, int paddleLen, int ballX, int ballY,
    const std::array<std::array<bool, LED_GRID_HEIGHT>, 2>& bricks)
{
    uint32_t mask[LED_MASK_WORDS] = {};

    for (int idx = 0; idx < paddleLen; idx++)
    {
        int y = paddleTop + idx;
        if (y >= 0 && y < LED_GRID_HEIGHT)
            setLedInMask(mask, 0, static_cast<uint16_t>(y));
    }

    for (int col = 0; col < 2; col++)
    {
        int x = LED_GRID_WIDTH - 2 + col;
        if (x < 0 || x >= LED_GRID_WIDTH)
            continue;
        for (int y = 0; y < LED_GRID_HEIGHT; y++)
        {
            if (bricks[col][y])
                setLedInMask(mask, static_cast<uint16_t>(x), static_cast<uint16_t>(y));
        }
    }

    if (ballX >= 0 && ballX < LED_GRID_WIDTH && ballY >= 0 && ballY < LED_GRID_HEIGHT)
        setLedInMask(mask, static_cast<uint16_t>(ballX), static_cast<uint16_t>(ballY));

    sendMaskToPanel(mask, LED_MASK_WORDS);
}

void WordyWatchDisplay::showBallSimFrame(const std::array<BallSimGame::Ball, BallSimGame::MAX_BALLS>& balls,
    int ballCount)
{
    uint32_t mask[LED_MASK_WORDS] = {};

    int clampedCount = ballCount;
    if (clampedCount < 0)
        clampedCount = 0;
    if (clampedCount > BallSimGame::MAX_BALLS)
        clampedCount = BallSimGame::MAX_BALLS;

    for (int idx = 0; idx < clampedCount; idx++)
    {
        int x = balls[idx].x / BallSimGame::POS_SCALE;
        int y = balls[idx].y / BallSimGame::POS_SCALE;
        if (x >= 0 && x < LED_GRID_WIDTH && y >= 0 && y < LED_GRID_HEIGHT)
            setLedInMask(mask, static_cast<uint16_t>(x), static_cast<uint16_t>(y));
    }

    sendMaskToPanel(mask, LED_MASK_WORDS);
}

void WordyWatchDisplay::clear()
{
#ifdef DEBUG_TIME_DISPLAY
    LOG_I(MODULE_PREFIX, "clear stopping LED panel");
#endif

    RaftDevice* pDevice = _sysMod.getDeviceByName("LEDPanel");
    if (pDevice)
        pDevice->sendCmdJSON("{\"cmd\":\"stop\"}");
}

const led_pattern_t* WordyWatchDisplay::getPatternForTime(int hour, int minute) const
{
    if (hour < 0 || minute < 0)
        return nullptr;

    hour = hour % 24;
    minute = (minute / LED_MINUTE_GRANULARITY) * LED_MINUTE_GRANULARITY;
    if (minute >= 60)
        minute = 0;

    for (const auto& pattern : led_patterns)
    {
        if (pattern.hour == hour && pattern.minute == minute)
        {
            return &pattern;
        }
    }

    return nullptr;
}

void WordyWatchDisplay::sendMaskToPanel(const uint32_t* words, size_t wordCount)
{
    RaftDevice* pDevice = _sysMod.getDeviceByName("LEDPanel");
    if (!pDevice)
        return;

    pDevice->sendCmdJSON("{\"cmd\":\"start\"}");

    String maskStr;
    for (size_t idx = 0; idx < wordCount; idx++)
    {
        char wordBuf[12] = {};
        snprintf(wordBuf, sizeof(wordBuf), "0x%08lX", static_cast<unsigned long>(words[idx]));
        maskStr += wordBuf;
        if (idx + 1 < wordCount)
            maskStr += ",";
    }

    String jsonCmd = "{\"cmd\":\"blitMask\",\"width\":" + String(LED_GRID_WIDTH) +
        ",\"height\":" + String(LED_GRID_HEIGHT) +
        ",\"clear\":1,\"mask\":\"" + maskStr + "\"}";

    pDevice->sendCmdJSON(jsonCmd.c_str());
}
