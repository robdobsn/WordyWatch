////////////////////////////////////////////////////////////////////////////////
//
// WordySysMod.cpp
//
////////////////////////////////////////////////////////////////////////////////

#include "WordySysMod.h"
#include "RaftUtils.h"

WordySysMod::WordySysMod(const char *pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig)
{
}

WordySysMod::~WordySysMod()
{
}

void WordySysMod::setup()
{
    int pwrCtrlPin = config.getInt("pwrCtrlPin", -1);
    int pwrOnLevel = config.getInt("pwrOnLevel", -1);
    pinMode(pwrCtrlPin, OUTPUT);
    digitalWrite(pwrCtrlPin, pwrOnLevel);
    LOG_I(MODULE_PREFIX, "Power Control Pin: %d, Power On Level: %d", pwrCtrlPin, pwrOnLevel);
}

void WordySysMod::loop()
{
    // // Check for loop rate
    // if (Raft::isTimeout(millis(), _lastLoopMs, 1000))
    // {
    //     // Update last loop time
    //     _lastLoopMs = millis();

    //     // Put some code here that will be executed once per second
    //     // ...
    // }
}

