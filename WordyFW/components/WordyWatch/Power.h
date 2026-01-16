/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Power Management
// Header-only class for voltage sensing, battery monitoring, and CPU power management
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

// Debug control
// #define DEBUG_POWER_MANAGEMENT
// #define DEBUG_VSENSE_READING
// #define DEBUG_BATTERY_CHECK

class Power
{
public:
    /// @brief Constructor
    Power() = default;
    
    /// @brief Destructor
    ~Power() = default;

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
                   uint32_t buttonOffTimeMs)
    {
        _powerCtrlPin = powerCtrlPin;
        _strapCtrlPin = strapCtrlPin;
        _vsensePin = vsensePin;
        _vsenseSlope = vsenseSlope;
        _vsenseIntercept = vsenseIntercept;
        _batteryLowV = batteryLowV;
        _vsenseButtonLevel = vsenseButtonLevel;
        _buttonOffTimeMs = buttonOffTimeMs;
        
        // Enable power hold
        enablePowerHold();
        
        // Unisolate strapping pins
        unisolateStrappingPins();
        
        // Initialize VSENSE pin
        if (_vsensePin >= 0)
        {
            pinMode(_vsensePin, INPUT);
        }

        // Debug
        LOG_I(MODULE_PREFIX, "config powerCtrlPin %d  strapCtrlPin %d vSensePin %d v1 %.2f a1 %d v2 %.2f a2 %d",
                    powerCtrlPin, strapCtrlPin, vsensePin, 
                    vsenseSlope, vsenseIntercept);
        
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
            
        // Read VSENSE
        readVSENSE();
        
        // Check for user button press via VSENSE
        checkUserButtonPress();
        
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
    
    /// @brief Enter light sleep with timer wakeup
    /// @param wakeupMs Wakeup time in milliseconds
    void enterLightSleep(uint32_t wakeupMs)
    {
        const uint64_t wakeup_time_us = wakeupMs * 1000; // Convert ms to microseconds
        esp_sleep_enable_timer_wakeup(wakeup_time_us);
        
        LOG_I(MODULE_PREFIX, "Entering light sleep for %dms", wakeupMs);
        esp_light_sleep_start();
    }
    
    /// @brief Enter deep sleep with timer wakeup
    /// @param wakeupMs Wakeup time in milliseconds
    /// @note Power control pin will be held HIGH during deep sleep
    void enterDeepSleep(uint32_t wakeupMs)
    {
        const uint64_t wakeup_time_us = wakeupMs * 1000; // Convert ms to microseconds
        esp_sleep_enable_timer_wakeup(wakeup_time_us);
        
        LOG_I(MODULE_PREFIX, "Entering deep sleep for %dms (power pin %d held HIGH)", wakeupMs, _powerCtrlPin);
        esp_deep_sleep_start();
    }
    
    /// @brief Reset button press state
    void resetButtonState()
    {
        _buttonPressed = false;
        _buttonPressChangeTimeMs = 0;
        _buttonPressDownTimeMs = 0;
    }

private:
    // Configuration
    bool _configured = false;
    
    // GPIO pins
    int _powerCtrlPin = -1;
    int _strapCtrlPin = -1;
    int _vsensePin = -1;
    
    // VSENSE to voltage conversion
    static constexpr float VSENSE_SLOPE_DEFAULT = 0.00223;
    static constexpr float VSENSE_INTERCEPT_DEFAULT = 0.0;
    double _vsenseSlope = VSENSE_SLOPE_DEFAULT;
    double _vsenseIntercept = VSENSE_INTERCEPT_DEFAULT;
    
    // Battery management
    static constexpr float BATTERY_LOW_V_DEFAULT = 3.55;
    float _batteryLowV = BATTERY_LOW_V_DEFAULT;
    bool _shutdownRequired = false;
    uint32_t _lastBatteryCheckMs = 0;
    static constexpr uint32_t BATTERY_CHECK_INTERVAL_MS = 10000;  // Check every 10s
    uint32_t _lastWarnBatLowShutdownTimeMs = 0;
    
    // VSENSE readings
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
    static constexpr const char* MODULE_PREFIX = "Power";
    
    /// @brief Read VSENSE pin and update average
    void readVSENSE()
    {
        if (_vsensePin < 0)
            return;
            
        _vsenseCurVal = analogRead(_vsensePin);
        
#ifdef DEBUG_VSENSE_READING
        if (_vsenseSampleCount % 100 == 0)
        {
            LOG_I(MODULE_PREFIX, "VSENSE: inst=%d avg=%d V=%.2fV",
                  _vsenseCurVal, _vsenseAvg.getAverage(), getBatteryVoltage());
        }
#endif
    }
    
    /// @brief Check for user button press via VSENSE
    /// The button is wired such that when pressed, VSENSE rises above threshold
    void checkUserButtonPress()
    {
        // Read current VSENSE value
        uint32_t vsenseVal = _vsenseCurVal;
        
        // Check if button is pressed (VSENSE above threshold)
        if (vsenseVal > _vsenseButtonLevel)
        {
            // Note time press started
            if (!_buttonPressed)
                _buttonPressDownTimeMs = millis();
                
            // Pressed
            _buttonPressed = true;
            _buttonPressChangeTimeMs = millis();
        }
        else
        {
            // Not pressed - debounce
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
                // Average vsense values when button not pressed
                _vsenseAvg.sample(vsenseVal);
                _vsenseSampleCount++;
            }
        }
    }
    
    /// @brief Check battery level and require shutdown if low
    void checkBatteryLevel()
    {
        // Need enough samples before checking
        if (_shutdownRequired || _vsenseSampleCount < 100)
            return;
            
        // Get voltage
        float voltage = getBatteryVoltage();
        
        // Check for shutdown
        if (voltage < _batteryLowV)
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
                      voltage, _batteryLowV);
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

    /// @brief Get power control pin number
    /// @return GPIO pin number or -1 if not configured
    int getPowerCtrlPin() const { return _powerCtrlPin; }
    
    /// @brief Get strap control pin number
    /// @return GPIO pin number or -1 if not configured
    int getStrapCtrlPin() const { return _strapCtrlPin; }
    
    /// @brief Get VSENSE pin number
    /// @return GPIO pin number or -1 if not configured
    int getVSENSEPin() const { return _vsensePin; }

};
