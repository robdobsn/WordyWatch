/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BallSimGame
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BallSimGame.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "esp_random.h"

void BallSimGame::configure(int count, float viscosity)
{
    _ballCount = std::clamp(count, 1, MAX_BALLS);
    _viscosity = std::clamp(viscosity, 0.0f, 1.0f);
}

void BallSimGame::reset()
{
    _occupied.fill(0);

    for (int idx = 0; idx < _ballCount; idx++)
    {
        int tries = 0;
        int col = 0;
        int row = 0;
        do
        {
            uint32_t r = esp_random();
            col = static_cast<int>(r % LED_GRID_WIDTH);
            row = static_cast<int>((r / LED_GRID_WIDTH) % LED_GRID_HEIGHT);
            tries++;
        } while (_occupied[row * LED_GRID_WIDTH + col] != 0 && tries < 1000);

        _occupied[row * LED_GRID_WIDTH + col] = 1;
        _balls[idx].x = static_cast<int16_t>(col * POS_SCALE);
        _balls[idx].y = static_cast<int16_t>(row * POS_SCALE);
        _balls[idx].vx = 0;
        _balls[idx].vy = 0;
    }
}

void BallSimGame::update(int16_t accelX, int16_t accelY, int16_t accelZ)
{
    int16_t az = static_cast<int16_t>(std::abs(accelZ));
    az = static_cast<int16_t>(az / 2048);
    az = (az >= 3) ? 1 : static_cast<int16_t>(4 - az);
    int16_t randRange = static_cast<int16_t>(az * 2 + 1);

    int16_t dampingScale = static_cast<int16_t>(256 - (static_cast<int>(std::lround(_viscosity * 128.0f))));
    if (dampingScale < 0)
        dampingScale = 0;
    if (dampingScale > 256)
        dampingScale = 256;

    for (int idx = 0; idx < _ballCount; idx++)
    {
        Ball& ball = _balls[idx];
        int16_t jitterX = static_cast<int16_t>(esp_random() % randRange);
        int16_t jitterY = static_cast<int16_t>(esp_random() % randRange);

        ball.vx = static_cast<int16_t>(((ball.vx + accelX + jitterX) * dampingScale) / 256);
        ball.vy = static_cast<int16_t>(((ball.vy + accelY + jitterY) * dampingScale) / 256);

        int32_t v2 = static_cast<int32_t>(ball.vx) * ball.vx + static_cast<int32_t>(ball.vy) * ball.vy;
        if (v2 > (POS_SCALE * POS_SCALE))
        {
            float v = std::sqrt(static_cast<float>(v2));
            ball.vx = static_cast<int16_t>((POS_SCALE * ball.vx) / v);
            ball.vy = static_cast<int16_t>((POS_SCALE * ball.vy) / v);
        }
    }

    _occupied.fill(0);
    for (int idx = 0; idx < _ballCount; idx++)
    {
        Ball& ball = _balls[idx];
        int32_t newX = static_cast<int32_t>(ball.x) + ball.vx;
        int32_t newY = static_cast<int32_t>(ball.y) + ball.vy;

        if (newX > MAX_X)
        {
            newX = MAX_X;
            ball.vx = static_cast<int16_t>(ball.vx / -2);
        }
        else if (newX < 0)
        {
            newX = 0;
            ball.vx = static_cast<int16_t>(ball.vx / -2);
        }

        if (newY > MAX_Y)
        {
            newY = MAX_Y;
            ball.vy = static_cast<int16_t>(ball.vy / -2);
        }
        else if (newY < 0)
        {
            newY = 0;
            ball.vy = static_cast<int16_t>(ball.vy / -2);
        }

        int oldCol = ball.x / POS_SCALE;
        int oldRow = ball.y / POS_SCALE;
        int newCol = static_cast<int>(newX / POS_SCALE);
        int newRow = static_cast<int>(newY / POS_SCALE);
        int oldIdx = oldRow * LED_GRID_WIDTH + oldCol;
        int newIdx = newRow * LED_GRID_WIDTH + newCol;

        if ((oldIdx != newIdx) && _occupied[newIdx])
        {
            int delta = std::abs(newIdx - oldIdx);
            if (delta == 1)
            {
                newX = ball.x;
                ball.vx = static_cast<int16_t>(ball.vx / -2);
                newIdx = oldIdx;
            }
            else if (delta == LED_GRID_WIDTH)
            {
                newY = ball.y;
                ball.vy = static_cast<int16_t>(ball.vy / -2);
                newIdx = oldIdx;
            }
            else
            {
                if (std::abs(ball.vx) >= std::abs(ball.vy))
                {
                    int tryIdx = oldRow * LED_GRID_WIDTH + newCol;
                    if (!_occupied[tryIdx])
                    {
                        newY = ball.y;
                        ball.vy = static_cast<int16_t>(ball.vy / -2);
                        newIdx = tryIdx;
                    }
                    else
                    {
                        tryIdx = newRow * LED_GRID_WIDTH + oldCol;
                        if (!_occupied[tryIdx])
                        {
                            newX = ball.x;
                            ball.vx = static_cast<int16_t>(ball.vx / -2);
                            newIdx = tryIdx;
                        }
                        else
                        {
                            newX = ball.x;
                            newY = ball.y;
                            ball.vx = static_cast<int16_t>(ball.vx / -2);
                            ball.vy = static_cast<int16_t>(ball.vy / -2);
                            newIdx = oldIdx;
                        }
                    }
                }
                else
                {
                    int tryIdx = newRow * LED_GRID_WIDTH + oldCol;
                    if (!_occupied[tryIdx])
                    {
                        newX = ball.x;
                        ball.vx = static_cast<int16_t>(ball.vx / -2);
                        newIdx = tryIdx;
                    }
                    else
                    {
                        tryIdx = oldRow * LED_GRID_WIDTH + newCol;
                        if (!_occupied[tryIdx])
                        {
                            newY = ball.y;
                            ball.vy = static_cast<int16_t>(ball.vy / -2);
                            newIdx = tryIdx;
                        }
                        else
                        {
                            newX = ball.x;
                            newY = ball.y;
                            ball.vx = static_cast<int16_t>(ball.vx / -2);
                            ball.vy = static_cast<int16_t>(ball.vy / -2);
                            newIdx = oldIdx;
                        }
                    }
                }
            }
        }

        ball.x = static_cast<int16_t>(newX);
        ball.y = static_cast<int16_t>(newY);
        if (newIdx >= 0 && newIdx < static_cast<int>(_occupied.size()))
            _occupied[newIdx] = 1;
    }
}
