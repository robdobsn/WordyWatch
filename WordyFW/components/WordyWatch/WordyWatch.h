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
#include "UARTLogger.h"
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

    // Monitoring methods
    void checkUserButtonPress(uint32_t vsenseVal);
    void checkBatteryLevel();
    void debugLogPowerStatus();
    void updateTimeDisplay();
    bool shouldGoToSleep();
    void checkWakeupButtonPress();

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
    uint32_t _sleepAfterBootMs = 5000;   // Default 5 seconds after boot
    uint32_t _sleepAfterWakeMs = 10000;  // Default 10 seconds after wake
    uint32_t _showTimeForMs = 5000;      // Default 5 seconds to show time when button pressed
    uint32_t _timerWakeupMs = 100;       // Default 100ms timer wakeup interval
    bool _autoSleepEnable = true;
    bool _isFirstBoot = true;            // Track if this is first boot
    bool _displayingTime = false;        // Track if currently displaying time
    uint32_t _displayTimeStartMs = 0;    // When time display started

    // Battery check management
    uint32_t _lastBatteryCheckMs = 0;
    static constexpr uint32_t BATTERY_CHECK_INTERVAL_MS = 10000;  // Check battery every 10s

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

    // Debug
    uint32_t _lastDebugTimeMs = 0;
    uint32_t _lastWarnBatLowShutdownTimeMs = 0;
    uint32_t _lastWarnUserShutdownTimeMs = 0;

    // UART logger for debugging
    UARTLogger _uartLogger;

    // I2C master bus handle
    i2c_master_bus_handle_t _i2cBusHandle = nullptr;
    i2c_master_dev_handle_t _accelDevHandle = nullptr;
    i2c_master_dev_handle_t _rtcDevHandle = nullptr;
    
    // I2C configuration
    int _i2cSdaPin = -1;
    int _i2cSclPin = -1;
    uint32_t _i2cFreqHz = 100000;  // 100kHz default
    uint8_t _accelI2CAddr = 0x6a;   // LSM6DS default address
    uint8_t _rtcI2CAddr = 0x68;     // RV-4162-C7 default address
    
    // RTC and time setting methods
    bool initI2C();
    bool initAccelerometer();
    void clearAccelInterrupt();
    bool initRTC();
    bool readRTCTime(struct tm* timeinfo);
    bool writeRTCTime(const struct tm* timeinfo);
    void handleTimeSettingMode();
    void enterTimeSettingMode();
    void exitTimeSettingMode(bool save);
    RaftRetCode apiSetTime(const String& reqStr, String& respStr, const APISourceInfo& sourceInfo);
    void deinitI2C();
};
