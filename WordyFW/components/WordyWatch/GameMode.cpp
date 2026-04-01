/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GameMode
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GameMode.h"

#include <cmath>
#include <cstddef>

#include "Accelerometer.h"
#include "WordyWatchDisplay.h"
#include "wordclock_patterns.h"

void GameMode::start(uint32_t nowMs)
{
    _active = true;
    _bootPressedPrev = false;
    _bootPressStartMs = 0;
    _startMs = nowMs;
    _gameOverStartMs = 0;
    _lastUpdateMs = 0;
    _needsRender = true;
    _breakout.reset();
    _paddleTop = _breakout.getPaddleTop();
    _paddleTopF = static_cast<float>(_paddleTop);
}

GameMode::UpdateResult GameMode::update(Accelerometer& accel, bool bootPressed, uint32_t nowMs)
{
    UpdateResult result;
    if (!_active)
        return result;

    if (bootPressed)
    {
        if (!_bootPressedPrev)
            _bootPressStartMs = nowMs;
        else if (nowMs - _bootPressStartMs >= LONG_PRESS_MS)
        {
            _active = false;
            result.exitRequested = true;
            return result;
        }
    }
    else
    {
        _bootPressStartMs = 0;
    }
    _bootPressedPrev = bootPressed;

    if (nowMs - _startMs >= GAME_TIMEOUT_MS)
    {
        _active = false;
        result.exitRequested = true;
        return result;
    }

    if (nowMs - _lastUpdateMs >= TICK_MS)
    {
        _lastUpdateMs = nowMs;
        updatePaddle(accel);
        _breakout.setPaddleTop(_paddleTop);
        _breakout.update();
        _needsRender = true;
    }

    if (_breakout.isWon() || _breakout.isLost())
    {
        if (_gameOverStartMs == 0)
            _gameOverStartMs = nowMs;
        if (nowMs - _gameOverStartMs >= GAME_OVER_EXIT_MS)
        {
            _active = false;
            result.exitRequested = true;
            return result;
        }
    }

    return result;
}

void GameMode::render(WordyWatchDisplay& display)
{
    if (!_active)
        return;

    if (!_needsRender)
        return;

    display.showBreakoutFrame(_paddleTop,
                              _breakout.getPaddleLen(),
                              _breakout.getBallX(),
                              _breakout.getBallY(),
                              _breakout.getBricks());
    _needsRender = false;
}

void GameMode::updatePaddle(Accelerometer& accel)
{
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    if (!accel.readAccelRaw(ax, ay, az))
        return;

    float ayf = static_cast<float>(ay);
    float azf = static_cast<float>(az);
    if (std::fabs(azf) < 500.0f)
        return;

    float angle = std::atan2f(ayf, azf);
    const float maxAngle = 0.349f;
    float normalized = -angle / maxAngle;
    if (normalized < -1.0f)
        normalized = -1.0f;
    if (normalized > 1.0f)
        normalized = 1.0f;

    int range = LED_GRID_HEIGHT - BreakoutGame::PADDLE_LEN;
    if (range < 0)
        range = 0;
    float t = (normalized + 1.0f) * 0.5f;
    float targetTopF = t * range;
    if (targetTopF < 0.0f)
        targetTopF = 0.0f;
    if (targetTopF > static_cast<float>(range))
        targetTopF = static_cast<float>(range);

    const float alpha = 0.25f;
    _paddleTopF = _paddleTopF + alpha * (targetTopF - _paddleTopF);
    _paddleTop = static_cast<int>(std::lround(_paddleTopF));
}
