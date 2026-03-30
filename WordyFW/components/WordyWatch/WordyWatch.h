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

// Debug: install GPIO ISR on wake pin to count wrist-tilt interrupts
#define DEBUG_WRIST_TILT_INT

// Debug: use simple motion wake-up interrupt instead of wrist tilt (easier to trigger)
// #define DEBUG_USE_MOTION_WAKEUP_INT

// Use basic tilt interrupt (embedded function, ~35° change) instead of wrist tilt
// Wrist tilt INT2 output generates constant periodic pulses (~0.8Hz) regardless of motion
#define DEBUG_USE_BASIC_TILT_INT

#include "Accelerometer.h"
#include "RTC.h"
#include "PowerAndSleep.h"
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

#ifdef DEBUG_WRIST_TILT_INT
    // Wrist tilt interrupt debugging
    int _wristTiltIntPin = -1;
    static volatile uint32_t _wristTiltIntCount;
    static volatile int64_t _wristTiltIntLastIsrTimeUs;
    int64_t _wristTiltIntLastDiagTimeUs = 0;
    uint32_t _wristTiltIntLastLogMs = 0;
    uint32_t _wristTiltIntLastLoggedCount = 0;
    static constexpr uint32_t WRIST_TILT_INT_LOG_INTERVAL_MS = 5000;
    static void IRAM_ATTR wristTiltISR(void* arg);
    void setupWristTiltInterrupt(int pinNum);
    void logWristTiltInterrupts();
#endif
    
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
