/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wordy Watch
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include "SimpleMovingAverage.h"
#include "RaftSysMod.h"
#include "Accelerometer.h"
#include "RTC.h"
#include "Power.h"
#include "driver/i2c_master.h"

class RaftJsonIF;
class APISourceInfo;

class WordyWatch : public RaftSysMod
{
public:
    WordyWatch(const char* pModuleName, RaftJsonIF& sysConfig);
    virtual ~WordyWatch();

    // Create function
    static RaftSysMod* create(const char* pModuleName, RaftJsonIF& sysConfig)
    {
        return new WordyWatch(pModuleName, sysConfig);
    }

    virtual void setup() override final;
    virtual void loop() override final;
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;
    virtual String getStatusJSON() const override final;
 
private:

    // Sleep state enum
    enum WatchState
    {
        DISPLAYING_TIME,
        PREPARING_TO_SLEEP,
        SLEEPING,
        WOKEN_UP,
        SHUTDOWN_REQUESTED,
        SHUTTING_DOWN
    };

    // Configuration applied
    bool _isConfigured = false;

    // Power management
    Power _power;

    // Sleep/wake state management
    WatchState _currentState = DISPLAYING_TIME;
    uint32_t _currentStateStartMs = 0;

    // Time last woken
    uint32_t _timeLastWokenMs = 0;

    // Configuration parameters
    uint32_t _sleepAfterBootMs = 5000;   // Default 5 seconds after boot
    uint32_t _sleepAfterWakeMs = 10000;  // Default 10 seconds after wake
    uint32_t _showTimeForMs = 5000;      // Default 5 seconds to show time when button pressed
    uint32_t _timerWakeupMs = 100;       // Default 100ms timer wakeup interval

    // Wake pin configuration
    int _wakePinNum = -1;
    bool _wakePinPullup = false;
    
    // Time setting configuration
    uint32_t _longPressMs = 2000;
    uint8_t _minuteResolution = 5;
    
    uint32_t _lastUserButtonPressMs = 0;

    // I2C master bus handle
    i2c_master_bus_handle_t _i2cBusHandle = nullptr;
    
    // I2C configuration
    int _i2cSdaPin = -1;
    int _i2cSclPin = -1;
    uint32_t _i2cFreqHz = 100000;  // 100kHz default
    
    // Accelerometer
    Accelerometer _accelerometer;
    
    // RTC
    RTC _rtc;
    
    // Helpers
    void clearDisplay();
    void checkWakeupReason();
    void showTimeOnDisplay();
    bool shouldGoToSleep();
    void checkWakeupButtonPress();
    bool initI2C();
    RaftRetCode apiSetTime(const String& reqStr, String& respStr, const APISourceInfo& sourceInfo);
    void setState(WatchState newState)
    {
        _currentState = newState;
        _currentStateStartMs = millis();
    }
};
