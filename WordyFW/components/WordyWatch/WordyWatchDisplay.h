/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WordyWatchDisplay.h
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <stdint.h>
#include "RaftSysMod.h"
#include "wordclock_patterns.h"

class RTC;

class WordyWatchDisplay
{
public:
    explicit WordyWatchDisplay(RaftSysMod& sysMod);

    void showTime(RTC& rtc);
    void clear();

private:
    const led_pattern_t* getPatternForTime(int hour, int minute) const;
    void sendMaskToPanel(const uint32_t* words, size_t wordCount);

    RaftSysMod& _sysMod;
};
