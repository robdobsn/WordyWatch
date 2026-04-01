/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BreakoutGame
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <cstddef>
#include <stdint.h>
#include "wordclock_patterns.h"

class BreakoutGame
{
public:
    static constexpr int PADDLE_LEN = 5;

    BreakoutGame();

    void reset();
    void setPaddleTop(int top);
    void update();

    bool isWon() const { return _won; }
    bool isLost() const { return _lost; }

    int getPaddleTop() const { return _paddleTop; }
    int getPaddleLen() const { return PADDLE_LEN; }
    int getBallX() const { return _ballX; }
    int getBallY() const { return _ballY; }

    const std::array<std::array<bool, LED_GRID_HEIGHT>, 2>& getBricks() const
    {
        return _bricks;
    }

private:
    std::array<std::array<bool, LED_GRID_HEIGHT>, 2> _bricks{};
    int _paddleTop = 0;
    int _ballX = 0;
    int _ballY = 0;
    int _dx = 1;
    int _dy = 1;
    bool _won = false;
    bool _lost = false;
};
