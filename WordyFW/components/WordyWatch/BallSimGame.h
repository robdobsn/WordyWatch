/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BallSimGame
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <cstddef>
#include <stdint.h>
#include "wordclock_patterns.h"

class BallSimGame
{
public:
    static constexpr int MAX_BALLS = LED_GRID_WIDTH * LED_GRID_HEIGHT;
    static constexpr int16_t POS_SCALE = 256;

    struct Ball
    {
        int16_t x = 0;
        int16_t y = 0;
        int16_t vx = 0;
        int16_t vy = 0;
    };

    void configure(int count, float viscosity);
    void reset();
    void update(int16_t accelX, int16_t accelY, int16_t accelZ);

    int getBallCount() const { return _ballCount; }
    const std::array<Ball, MAX_BALLS>& getBalls() const { return _balls; }

private:
    static constexpr int16_t MAX_X = LED_GRID_WIDTH * POS_SCALE - 1;
    static constexpr int16_t MAX_Y = LED_GRID_HEIGHT * POS_SCALE - 1;

    std::array<Ball, MAX_BALLS> _balls{};
    std::array<uint8_t, LED_GRID_WIDTH * LED_GRID_HEIGHT> _occupied{};
    int _ballCount = 12;
    float _viscosity = 0.5f;
};
