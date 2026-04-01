/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GameMode
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include "BallSimGame.h"
#include "BreakoutGame.h"

class Accelerometer;
class WordyWatchDisplay;

class GameMode
{
public:
    struct UpdateResult
    {
        bool exitRequested = false;
    };

    void configureBallSim(int count, float viscosity);
    void start(uint32_t nowMs);
    UpdateResult update(Accelerometer& accel, bool bootPressed, uint32_t nowMs);
    void render(WordyWatchDisplay& display);

    bool isActive() const { return _active; }

private:
    void updatePaddle(Accelerometer& accel);
    void updateBallSim(Accelerometer& accel);
    void cycleGame();

    enum class GameType
    {
        Breakout,
        BallSim
    };

    BreakoutGame _breakout;
    BallSimGame _ballSim;
    GameType _currentGame = GameType::Breakout;
    bool _active = false;
    bool _bootPressedPrev = false;
    uint32_t _bootPressStartMs = 0;
    uint32_t _startMs = 0;
    uint32_t _gameOverStartMs = 0;
    uint32_t _lastUpdateMs = 0;
    bool _needsRender = false;
    int _paddleTop = 0;
    float _paddleTopF = 0.0f;

    int _ballCount = 12;
    float _ballViscosity = 0.5f;

    static constexpr uint32_t LONG_PRESS_MS = 1500;
    static constexpr uint32_t GAME_TIMEOUT_MS = 60000;
    static constexpr uint32_t GAME_OVER_EXIT_MS = 10000;
    static constexpr uint32_t TICK_MS_BREAKOUT = 100;
    static constexpr uint32_t TICK_MS_BALLSIM = 33;
};
