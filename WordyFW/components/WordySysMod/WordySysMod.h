////////////////////////////////////////////////////////////////////////////////
//
// WordySysMod.h
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "RaftSysMod.h"

class WordySysMod : public RaftSysMod
{
public:
    WordySysMod(const char *pModuleName, RaftJsonIF& sysConfig);
    virtual ~WordySysMod();

    // Create function (for use by SysManager factory)
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new WordySysMod(pModuleName, sysConfig);
    }

protected:

    // Setup
    virtual void setup() override final;

    // Loop (called frequently)
    virtual void loop() override final;

private:
    // Debug
    static constexpr const char *MODULE_PREFIX = "WordySysMod";

    // Example of how to control loop rate
    uint32_t _lastLoopMs = 0;
};
