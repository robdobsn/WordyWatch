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

// Debug control - uncomment to enable specific debugging
#define DEBUG_TIME_DISPLAY

namespace
{
    static constexpr const char* MODULE_PREFIX = "WordyWatchDisplay";
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
