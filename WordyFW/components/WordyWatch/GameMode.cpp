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

void GameMode::configureBallSim(int count, float viscosity)
{
    _ballCount = count;
    _ballViscosity = viscosity;
    _ballSim.configure(_ballCount, _ballViscosity);
}

void GameMode::start(uint32_t nowMs)
{
    _active = true;
    _bootPressedPrev = false;
    _bootPressStartMs = 0;
    _startMs = nowMs;
    _gameOverStartMs = 0;
    _lastUpdateMs = 0;
    _needsRender = true;
    if (_currentGame == GameType::Breakout)
    {
        _breakout.reset();
        _paddleTop = _breakout.getPaddleTop();
        _paddleTopF = static_cast<float>(_paddleTop);
    }
    else
    {
        _ballSim.configure(_ballCount, _ballViscosity);
        _ballSim.reset();
    }
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
        if (_bootPressedPrev)
        {
            uint32_t pressDurationMs = nowMs - _bootPressStartMs;
            if (pressDurationMs < LONG_PRESS_MS)
            {
                cycleGame();
                _needsRender = true;
            }
        }
        _bootPressStartMs = 0;
    }
    _bootPressedPrev = bootPressed;

    if (nowMs - _startMs >= GAME_TIMEOUT_MS)
    {
        _active = false;
        result.exitRequested = true;
        return result;
    }

    uint32_t tickMs = (_currentGame == GameType::Breakout) ? TICK_MS_BREAKOUT : TICK_MS_BALLSIM;
    if (nowMs - _lastUpdateMs >= tickMs)
    {
        _lastUpdateMs = nowMs;
        if (_currentGame == GameType::Breakout)
        {
            updatePaddle(accel);
            _breakout.setPaddleTop(_paddleTop);
            _breakout.update();
        }
        else
        {
            updateBallSim(accel);
        }
        _needsRender = true;
    }

    if (_currentGame == GameType::Breakout && (_breakout.isWon() || _breakout.isLost()))
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

    if (_currentGame == GameType::Breakout)
    {
        display.showBreakoutFrame(_paddleTop,
                                  _breakout.getPaddleLen(),
                                  _breakout.getBallX(),
                                  _breakout.getBallY(),
                                  _breakout.getBricks());
    }
    else
    {
        display.showBallSimFrame(_ballSim.getBalls(), _ballSim.getBallCount());
    }
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

void GameMode::updateBallSim(Accelerometer& accel)
{
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    if (!accel.readAccelRaw(ax, ay, az))
        return;

    int16_t sandAx = static_cast<int16_t>(-ax / 256);
    int16_t sandAy = static_cast<int16_t>(-ay / 256);

    int16_t azAbs = static_cast<int16_t>(std::abs(az));
    int16_t azBias = static_cast<int16_t>(azAbs / 2048);
    azBias = (azBias >= 3) ? 1 : static_cast<int16_t>(4 - azBias);
    sandAx = static_cast<int16_t>(sandAx - azBias);
    sandAy = static_cast<int16_t>(sandAy - azBias);

    _ballSim.update(sandAx, sandAy, az);
}

void GameMode::cycleGame()
{
    if (_currentGame == GameType::Breakout)
        _currentGame = GameType::BallSim;
    else
        _currentGame = GameType::Breakout;

    _gameOverStartMs = 0;
    if (_currentGame == GameType::Breakout)
    {
        _breakout.reset();
        _paddleTop = _breakout.getPaddleTop();
        _paddleTopF = static_cast<float>(_paddleTop);
    }
    else
    {
        _ballSim.configure(_ballCount, _ballViscosity);
        _ballSim.reset();
    }
}
