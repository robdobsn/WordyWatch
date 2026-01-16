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
        RUNNING,
        PREPARING_TO_SLEEP,
        SLEEPING,
        WAKING_UP,
        SETTING_TIME_HOURS,
        SETTING_TIME_MINUTES
    };

    // Sleep/wake methods
    void prepareForSleep();
    void handleWakeup();

    // Monitoring methods
    void updateTimeDisplay();
    bool shouldGoToSleep();
    void checkWakeupButtonPress();

    // Apply configuration
    bool applyConfiguration();

    // Configuration applied
    bool _isConfigured = false;

    // Power management
    Power _power;

    // Sleep/wake state management
    WatchState _currentState = RUNNING;
    uint32_t _wakeTimeMs = 0;
    uint32_t _sleepAfterBootMs = 5000;   // Default 5 seconds after boot
    uint32_t _sleepAfterWakeMs = 10000;  // Default 10 seconds after wake
    uint32_t _showTimeForMs = 5000;      // Default 5 seconds to show time when button pressed
    uint32_t _timerWakeupMs = 100;       // Default 100ms timer wakeup interval
    bool _autoSleepEnable = true;
    bool _isFirstBoot = true;            // Track if this is first boot
    bool _displayingTime = false;        // Track if currently displaying time
    uint32_t _displayTimeStartMs = 0;    // When time display started

    // Wake pin configuration
    int _wakePinNum = -1;
    bool _wakePinPullup = false;
    
    // Wake button press detection
    uint32_t _wakePinPressStartMs = 0;
    bool _wakePinPressed = false;

    // Time setting configuration
    uint32_t _longPressMs = 2000;
    uint32_t _timeSetFlashOnMs = 500;
    uint32_t _timeSetFlashOffMs = 500;
    uint32_t _timeSetTimeoutMs = 30000;
    uint8_t _minuteResolution = 5;
    
    // Time setting state
    uint32_t _timeSetStartMs = 0;
    uint32_t _timeSetLastFlashMs = 0;
    bool _timeSetFlashState = false;
    int _timeSetHours = 0;
    int _timeSetMinutes = 0;
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
    
    // Time setting methods
    bool initI2C();
    void handleTimeSettingMode();
    void enterTimeSettingMode();
    void exitTimeSettingMode(bool save);
    RaftRetCode apiSetTime(const String& reqStr, String& respStr, const APISourceInfo& sourceInfo);
};
