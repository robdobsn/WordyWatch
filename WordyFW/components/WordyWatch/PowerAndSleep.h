/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Power And Sleep Management
// Header-only class for voltage sensing, battery monitoring, and CPU power & sleep management
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "Accelerometer.h"
#include "RTC.h"

// Debug control
#define DEBUG_POWER_MANAGEMENT
#define DEBUG_VSENSE_READING
#define DEBUG_BATTERY_CHECK
#define DEBUG_SLEEP_AND_WAKEUP

class PowerAndSleep
{
public:
    /// @brief Constructor
    PowerAndSleep() = default;
    
    /// @brief Destructor
    ~PowerAndSleep() = default;

    /// @brief Configure power management
    /// @param powerCtrlPin GPIO pin for power control (-1 to disable)
    /// @param strapCtrlPin GPIO pin for strap control (-1 to disable)
    /// @param vsensePin GPIO pin for voltage sensing (-1 to disable)
    /// @param vsenseSlope Slope for ADC to voltage conversion
    /// @param vsenseIntercept Intercept for ADC to voltage conversion
    /// @param batteryLowV Battery low voltage threshold
    /// @param vsenseButtonLevel VSENSE ADC level when button is pressed
    /// @param buttonOffTimeMs Time threshold for button off detection
    void configure(int powerCtrlPin, int strapCtrlPin, int vsensePin,
                   double vsenseSlope, double vsenseIntercept, 
                   float batteryLowV, uint32_t vsenseButtonLevel,
                   uint32_t buttonOffTimeMs,
                   int wakePinNum, bool wakePinPullup)
    {
        // Store configuration
        _powerCtrlPin = powerCtrlPin;
        _strapCtrlPin = strapCtrlPin;
        _vsensePin = vsensePin;
        _vsenseSlope = vsenseSlope;
        _vsenseIntercept = vsenseIntercept;
        _batteryLowV = batteryLowV;
        _vsenseButtonLevel = vsenseButtonLevel;
        _buttonOffTimeMs = buttonOffTimeMs;
        _wakePinNum = wakePinNum;
        _wakePinPullup = wakePinPullup;
        
        // Enable power hold
        enablePowerHold();
        
        // Unisolate strapping pins
        unisolateStrappingPins();
        
        // Initialize VSENSE pin
        if (_vsensePin >= 0)
        {
            pinMode(_vsensePin, INPUT);
        }

        // Configure wake pin if specified
        if (_wakePinNum >= 0)
        {
            pinMode(_wakePinNum, INPUT);
            if (_wakePinPullup)
            {
                gpio_pullup_en((gpio_num_t)_wakePinNum);
            }
        }

        // Debug
        LOG_I(MODULE_PREFIX, "config powerCtrlPin %d strapCtrlPin %d vSensePin %d vSenseSlope %.2e vSenseIntercept %.2f wakePinNum %d wakePinPullup %s",
                    powerCtrlPin, strapCtrlPin, vsensePin,
                    vsenseSlope, vsenseIntercept,
                    wakePinNum, wakePinPullup ? "Y" : "N");

        // Mark as configured
        _configured = true;
    }
    
    /// @brief Update power management (call frequently from main loop)
    /// @return true if shutdown required
    /// Should be called at least every 100ms to maintain responsiveness
    bool update()
    {
        if (!_configured)
            return false;
            
        // Read VSENSE and update button state
        readVSENSE();
                
        // Check battery level periodically
        if (Raft::isTimeout(millis(), _lastBatteryCheckMs, BATTERY_CHECK_INTERVAL_MS))
        {
            _lastBatteryCheckMs = millis();
            checkBatteryLevel();
        }
        return _shutdownRequired;
    }
    
    /// @brief Check if power button is pressed
    /// @return true if button is currently pressed
    bool isPowerButtonPressed() const
    {
        return _buttonPressed;
    }

    /// @brief Check and log wakeup reason (includes power button, accelerometer tilt, and RTC status)
    void checkWakeupReason(Accelerometer& accelerometer, RTC& rtc)
    {
        // Get wakeup reason
        esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

        const char* wakeupSource = "unknown";
        switch (wakeupReason)
        {
            case ESP_SLEEP_WAKEUP_UNDEFINED:
                wakeupSource = "power-on (not from sleep)";
                break;
            case ESP_SLEEP_WAKEUP_EXT1:
            {
                // EXT1 wakeup on GPIO (shared line) - determine which device triggered it
                wakeupSource = "EXT1 GPIO";

                // Check if the button is pressed (VSENSE above threshold)
                bool buttonPressed = isPowerButtonPressed();

                // Check if the IMU flagged a tilt event
                bool tiltDetected = false;
                accelerometer.readTiltStatus(tiltDetected);

                // Check RTC status for alarm/timer flags
                uint8_t rtcStatus = 0;
                rtc.readStatusRegister(rtcStatus);

                if (buttonPressed)
                    wakeupSource = "button press";
                else if (tiltDetected)
                    wakeupSource = "wrist tilt";
                else if (rtcStatus & 0x03)  // AF or TF flags
                    wakeupSource = "RTC alarm/timer";
                else
                    wakeupSource = "EXT1 (source unclear)";

                LOG_I(MODULE_PREFIX, "checkWakeupReason EXT1 detail: button=%s tilt=%s rtcStatus=0x%02x",
                      buttonPressed ? "YES" : "no", tiltDetected ? "YES" : "no", rtcStatus);
                break;
            }
            case ESP_SLEEP_WAKEUP_TIMER:
                wakeupSource = "timer";
                break;
            default:
                break;
        }

        LOG_I(MODULE_PREFIX, "checkWakeupReason: %s (reason %d)", wakeupSource, wakeupReason);
    }
    
    /// @brief Get button press duration in milliseconds
    /// @return Duration button has been pressed, or 0 if not pressed
    uint32_t getButtonPressDuration() const
    {
        if (!_buttonPressed || _buttonPressDownTimeMs == 0)
            return 0;
        return Raft::timeElapsed(millis(), _buttonPressDownTimeMs);
    }
    
    /// @brief Get current battery voltage
    /// @return Calculated voltage in volts
    float getBatteryVoltage() const
    {
        if (_vsenseSampleCount < 10)
            return 0.0f;
        return getVoltageFromADCReading(_vsenseAvg.getAverage());
    }
    
    /// @brief Get instantaneous VSENSE ADC reading
    /// @return Current ADC value
    uint32_t getVSENSEReading() const
    {
        return _vsenseCurVal;
    }
    
    /// @brief Get average VSENSE ADC reading
    /// @return Average ADC value
    uint32_t getVSENSEAverage() const
    {
        return _vsenseAvg.getAverage();
    }
    
    /// @brief Execute shutdown sequence
    /// This will hold power control pin low and enter light sleep with no wakeup
    void shutdown()
    {
        LOG_I(MODULE_PREFIX, "Executing shutdown sequence");
        
        // Disable hold on power control pin and set LOW
        disablePowerHold();
        
        // Wait for capacitors to discharge
        delay(TIME_TO_HOLD_POWER_CTRL_PIN_LOW_MS);
        
        // Enter light sleep with no wakeup
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_light_sleep_start();
    }
    
    /// @brief Enter deep sleep with timer wakeup
    /// @param wakeupMs Wakeup time in milliseconds (-1 for no timer wakeup)
    /// @note Power control pin will be held HIGH during deep sleep
    void enterDeepSleep(int wakeupMs)
    {
#ifdef FEATURE_POWER_CONTROL_ENABLE_SLEEP
        // Disable wake up sources first
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

        // Check for wakeup time
        if (wakeupMs < 0)
        {
#ifdef DEBUG_SLEEP_AND_WAKEUP
            LOG_I(MODULE_PREFIX, "Entering untimed deep sleep (power pin %d held HIGH)", _powerCtrlPin);
#endif
        }
        else
        {
            // Convert ms to microseconds
            const uint64_t wakeup_time_us = wakeupMs * 1000; // Convert ms to microseconds
            esp_sleep_enable_timer_wakeup(wakeup_time_us);
#ifdef DEBUG_SLEEP_AND_WAKEUP         
            LOG_I(MODULE_PREFIX, "Entering deep sleep for %dms (power pin %d held HIGH)", wakeupMs, _powerCtrlPin);
#endif
        }

        // Check for wake pin
        if (_wakePinNum >= 0)
        {
            esp_sleep_enable_ext1_wakeup(1ULL << _wakePinNum, ESP_EXT1_WAKEUP_ANY_LOW);
#ifdef DEBUG_SLEEP_AND_WAKEUP
            LOG_I(MODULE_PREFIX, "Deep sleep wakeup configured on pin %d (%s)", 
                  _wakePinNum, _wakePinPullup ? "pullup" : "pulldown");
#endif
        }

        // Start deep sleep
        esp_deep_sleep_start();
#endif
    }
    
    /// @brief Reset button press state
    void resetButtonState()
    {
        _buttonPressed = false;
        _buttonPressChangeTimeMs = 0;
        _buttonPressDownTimeMs = 0;
    }

private:
    /// @brief Read VSENSE pin and update average
    void readVSENSE()
    {
        // Check if VSENSE pin configured and time to read
        if ((_vsensePin < 0) || !Raft::isTimeout(millis(), _vsenseLastReadMs, VSENSE_READ_INTERVAL_MS))
            return;
        _vsenseLastReadMs = millis();
        
        // Read VSENSE pin
        _vsenseCurVal = analogRead(_vsensePin);

        // Check if button is pressed (VSENSE above threshold)
        if (_vsenseCurVal > _vsenseButtonLevel)
        {
            // Check if press just started
            if (!_buttonPressed)
            {
                // Record press down time
                _buttonPressDownTimeMs = millis();
                _buttonPressed = true;
            
#ifdef DEBUG_POWER_MANAGEMENT
            LOG_I(MODULE_PREFIX, "Button pressed at %dms", 
                    (int)_buttonPressDownTimeMs);
#endif
            }

            // Pressed
            _buttonPressChangeTimeMs = millis();
        }
        else
        {
            // Not pressed currently - check if recently released
            if (_buttonPressed)
            {
                if (Raft::isTimeout(millis(), _buttonPressChangeTimeMs, 200))
                {
                    // Button released after debounce
#ifdef DEBUG_POWER_MANAGEMENT
                    LOG_I(MODULE_PREFIX, "Button released after %dms", 
                          (int)Raft::timeElapsed(millis(), _buttonPressDownTimeMs));
#endif
                    _buttonPressed = false;
                }
            }
            else
            {
                // Button not pressed so we can use this reading as battery voltage sample
                _vsenseAvg.sample(_vsenseCurVal);
                _vsenseSampleCount++;

                // Calculate battery level from average
                _batteryV = getVoltageFromADCReading(_vsenseAvg.getAverage());
                _batteryValid = (_vsenseSampleCount >= 100);

#ifdef DEBUG_VSENSE_READING
                if (_vsenseSampleCount % 10 == 0)
                {
                    LOG_I(MODULE_PREFIX, "VSENSE: inst=%d avg=%d V=%.2fV batteryReadingValid=%s",
                          (int)_vsenseCurVal, _vsenseAvg.getAverage(), _batteryV, _batteryValid ? "Y" : "N");
                }
#endif

            }
        }
    }
    
    /// @brief Check battery level and require shutdown if low
    void checkBatteryLevel()
    {
        // Check if already shutting down or battery not valid
        if (_shutdownRequired || !_batteryValid)
            return;
            
        // Check for shutdown
        if (_batteryV < _batteryLowV)
        {
#ifdef DEBUG_BATTERY_CHECK
            if (Raft::isTimeout(millis(), _lastWarnBatLowShutdownTimeMs, 1000))
            {
                LOG_I(MODULE_PREFIX, "Battery low %s voltage %.2fV (threshold %.2fV)",
#ifdef FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
                      "shutting down",
#else
                      "!!! SHUTDOWN DISABLED !!!",
#endif
                        _batteryV, _batteryLowV);
                _lastWarnBatLowShutdownTimeMs = millis();
            }
#endif
            
            // Initiate shutdown
#ifdef FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
            _shutdownRequired = true;
#endif
        }
    }
    
    /// @brief Convert ADC reading to voltage
    /// @param adcReading ADC reading value
    /// @return Calculated voltage in volts
    float getVoltageFromADCReading(uint32_t adcReading) const
    {
        return adcReading * _vsenseSlope + _vsenseIntercept;
    }

    /// @brief Temporarily disable power hold (for checking wake button)
    /// Call this before checking wake button state, then call enablePowerHold() after
    void disablePowerHold()
    {
        if (_powerCtrlPin >= 0)
        {
            gpio_hold_dis((gpio_num_t)_powerCtrlPin);
            digitalWrite(_powerCtrlPin, LOW);
        }
    }
    
    /// @brief Re-enable power hold after checking wake button
    void enablePowerHold()
    {
        if (_powerCtrlPin >= 0)
        {
            pinMode(_powerCtrlPin, OUTPUT);
            digitalWrite(_powerCtrlPin, HIGH);
            gpio_hold_en((gpio_num_t)_powerCtrlPin);
        }
    }

    /// @brief Unisolate strapping pins by setting strap control pin HIGH
    void unisolateStrappingPins()
    {
        if (_strapCtrlPin >= 0)
        {
            pinMode(_strapCtrlPin, OUTPUT);
            // Un-isolate strapping pins
            digitalWrite(_strapCtrlPin, HIGH);
        }
    }

private:
    // Configuration
    bool _configured = false;
    
    // GPIO pins
    int _powerCtrlPin = -1;
    int _strapCtrlPin = -1;
    int _vsensePin = -1;
    int _wakePinNum = -1;
    bool _wakePinPullup = false;
    
    // VSENSE to voltage conversion
    static constexpr float VSENSE_SLOPE_DEFAULT = 0.00223;
    static constexpr float VSENSE_INTERCEPT_DEFAULT = 0.0;
    double _vsenseSlope = VSENSE_SLOPE_DEFAULT;
    double _vsenseIntercept = VSENSE_INTERCEPT_DEFAULT;
    
    // Battery management
    float _batteryV = 0.0f;
    bool _batteryValid = false;
    static constexpr float BATTERY_LOW_V_DEFAULT = 3.55;
    float _batteryLowV = BATTERY_LOW_V_DEFAULT;
    bool _shutdownRequired = false;
    uint32_t _lastBatteryCheckMs = 0;
    static constexpr uint32_t BATTERY_CHECK_INTERVAL_MS = 10000;
    uint32_t _lastWarnBatLowShutdownTimeMs = 0;
    
    // VSENSE readings
    uint32_t _vsenseLastReadMs = 0;
    static constexpr uint32_t VSENSE_READ_INTERVAL_MS = 100;
    uint32_t _vsenseCurVal = 0;
    SimpleMovingAverage<100> _vsenseAvg;
    uint32_t _vsenseSampleCount = 0;
    
    // Button detection via VSENSE
    static constexpr uint32_t VSENSE_BUTTON_LEVEL_DEFAULT = 2300;
    uint32_t _vsenseButtonLevel = VSENSE_BUTTON_LEVEL_DEFAULT;
    bool _buttonPressed = false;
    uint32_t _buttonPressChangeTimeMs = 0;
    uint32_t _buttonPressDownTimeMs = 0;
    static constexpr uint32_t BUTTON_OFF_TIME_MS_DEFAULT = 2000;
    uint32_t _buttonOffTimeMs = BUTTON_OFF_TIME_MS_DEFAULT;
    
    // Power timing
    static constexpr uint32_t TIME_TO_HOLD_POWER_CTRL_PIN_LOW_MS = 500;

    // Debug
    static constexpr const char* MODULE_PREFIX = "Power&Sleep";
};
