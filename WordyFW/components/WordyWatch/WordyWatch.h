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

class RaftJsonIF;

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
        WAKING_UP
    };

    // Get voltage
    float getVoltageFromADCReading(uint32_t adcReading) const;

    // Check if shutdown initiated
    bool isShutdownRequested()
    {
        return _shutdownInitiated;
    }

    // Handle shutdown
    void shutdown();

    // Sleep/wake methods
    void prepareForSleep();
    void enterLightSleep();
    void enterDeepSleep();
    void handleWakeup();
    void updateTimeDisplay();
    bool shouldGoToSleep();

    // Apply configuration
    bool applyConfiguration();

    // VSENSE threshold when button is pressed
    static const uint32_t VSENSE_BUTTON_LEVEL_DEFAULT = 2300;
    uint32_t _vsenseButtonLevel = VSENSE_BUTTON_LEVEL_DEFAULT;

    // Configuration applied
    bool _isConfigured = false;

    // VSENSE to voltage conversion for WordyWatch
    // Note that this is overridden by values in the sysTypes if present
    // Measurements from multimeter
    // 3.584V = 1600
    // 3.697V = 1659
    // 3.804V = 1704
    // 3.899V = 1748
    // 4.003V = 1790
    // 4.107V = 1845
    // 4.203V = 1887
    // Using excel to fit a curve with 0 intercept ...
    static constexpr float VSENSE_SLOPE_DEFAULT = 0.00223;
    static constexpr float VSENSE_INTERCEPT_DEFAULT = 0.0;
    double _vsenseSlope = VSENSE_SLOPE_DEFAULT;
    double _vsenseIntercept = VSENSE_INTERCEPT_DEFAULT;

    // Shutdown due to battery low threshold
    static constexpr float BATTERY_LOW_V_DEFAULT = 3.55;
    float _batteryLowV = BATTERY_LOW_V_DEFAULT;

    // Shutdown initiated
    bool _shutdownInitiated = false;
    
    // Power control pin
    int _powerCtrlPin = -1;

    // Time to hold power control pin low for shutdown
    static constexpr uint32_t TIME_TO_HOLD_POWER_CTRL_PIN_LOW_MS = 500;

    // VSENSE pin
    int _vsensePin = -1;

    // VSENSE averaging
    SimpleMovingAverage<100> _vsenseAvg;
    uint32_t _sampleCount = 0;

    // Debounce button
    bool _buttonPressed = false;
    uint32_t _buttonPressChangeTimeMs = 0;
    uint32_t _buttonPressDownTimeMs = 0;

    // Off time threshold for button press ms
    static constexpr uint32_t BUTTON_OFF_TIME_MS_DEFAULT = 2000;
    uint32_t _buttonOffTimeMs = BUTTON_OFF_TIME_MS_DEFAULT;

    // Sleep/wake state management
    WatchState _currentState = RUNNING;
    uint32_t _wakeTimeMs = 0;
    uint32_t _sleepAfterWakeMs = 10000;  // Default 10 seconds
    bool _autoSleepEnable = true;

    // Wake pin configuration
    int _wakePinNum = -1;
    bool _wakePinPullup = false;

    // Debug
    uint32_t _lastDebugTimeMs = 0;
    uint32_t _lastWarnBatLowShutdownTimeMs = 0;
    uint32_t _lastWarnUserShutdownTimeMs = 0;
};
