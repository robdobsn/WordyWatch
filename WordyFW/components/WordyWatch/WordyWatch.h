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
#include "PowerAndSleep.h"
#include "WordyWatchDisplay.h"
#include "RaftJsonNVS.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

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
        INITIAL_STATE,
        DISPLAYING_TIME,
        PREPARING_TO_SLEEP,
        SLEEPING,
        WOKEN_UP,
        SHUTDOWN_REQUESTED,
        SHUTTING_DOWN
    };

    // Configuration applied
    bool _isConfigured = false;

    // Power & Sleep management
    PowerAndSleep _powerAndSleep;

    // Sleep/wake state management
    WatchState _currentState = INITIAL_STATE;
    uint32_t _currentStateStartMs = 0;

    // Time last woken
    uint32_t _timeLastWokenMs = 0;

    // Configuration parameters
    uint32_t _showTimeForMs = 10000;
    uint32_t _batteryGaugeShowMs = 1500;
    uint32_t _batteryGaugeSweepMs = 333;
    float _batteryGaugeMinV = 3.4f;
    float _batteryGaugeMaxV = 4.2f;

    // Time setting configuration
    uint32_t _longPressMs = 2000;
    uint8_t _minuteResolution = 5;
    
    uint32_t _lastBootButtonPressMs = 0;
    bool _batteryGaugeActive = false;
    uint32_t _batteryGaugeStartMs = 0;
    uint8_t _batteryGaugeTargetLeds = 0;
    uint8_t _batteryGaugeLastShown = 0;
    uint32_t _batteryGaugeSweepStartMs = 0;
    bool _lastBootButtonPressed = false;

    int _bootButtonPinNum = -1;
    bool _bootButtonPullup = false;

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

    // Display helper
    WordyWatchDisplay _display;

    // Persisted settings
    RaftJsonNVS _settingsNVS;

    // Helpers
    bool shouldGoToSleep();
    void checkWakeupButtonPress();
    bool initI2C();
    RaftRetCode apiSetTime(const String& reqStr, String& respStr, const APISourceInfo& sourceInfo);
    RaftRetCode apiSettings(const String& reqStr, String& respStr, const APISourceInfo& sourceInfo);
    String getSettingsJSON() const;
    bool persistSettings();
    void setState(WatchState newState)
    {
        _currentState = newState;
        _currentStateStartMs = millis();
    }
};
