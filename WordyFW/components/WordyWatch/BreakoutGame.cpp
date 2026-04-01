/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BreakoutGame
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BreakoutGame.h"

#include <algorithm>

BreakoutGame::BreakoutGame()
{
    reset();
}

void BreakoutGame::reset()
{
    for (auto& col : _bricks)
        col.fill(true);

    _paddleTop = (LED_GRID_HEIGHT - PADDLE_LEN) / 2;
    _ballX = 1;
    _ballY = _paddleTop + (PADDLE_LEN / 2);
    _dx = 1;
    _dy = 1;
    _won = false;
    _lost = false;
}

void BreakoutGame::setPaddleTop(int top)
{
    int maxTop = LED_GRID_HEIGHT - PADDLE_LEN;
    if (maxTop < 0)
        maxTop = 0;
    _paddleTop = std::clamp(top, 0, maxTop);
}

void BreakoutGame::update()
{
    if (_won || _lost)
        return;

    int nextX = _ballX + _dx;
    int nextY = _ballY + _dy;

    if (nextY < 0 || nextY >= LED_GRID_HEIGHT)
    {
        _dy = -_dy;
        nextY = _ballY + _dy;
    }

    if (nextX < 0)
    {
        _lost = true;
        return;
    }

    if (nextX == 0)
    {
        if (nextY >= _paddleTop && nextY < (_paddleTop + PADDLE_LEN))
        {
            _dx = 1;
            nextX = _ballX + _dx;
        }
        else
        {
            _lost = true;
            return;
        }
    }

    if (nextX >= LED_GRID_WIDTH)
    {
        _won = true;
        return;
    }

    if (nextX >= (LED_GRID_WIDTH - 2))
    {
        int brickCol = nextX - (LED_GRID_WIDTH - 2);
        if (brickCol >= 0 && brickCol < 2)
        {
            if (_bricks[brickCol][nextY])
            {
                _bricks[brickCol][nextY] = false;
                _dx = -_dx;
                nextX = _ballX + _dx;
            }
        }
    }

    _ballX = nextX;
    _ballY = nextY;
}
