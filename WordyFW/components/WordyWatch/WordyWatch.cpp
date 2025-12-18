/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wordy Watch
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "WordyWatch.h"
#include "RaftJsonIF.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "SysManager.h"
#include "DeviceManager.h"
#include "DeviceLEDCharlie.h"
#include "APISourceInfo.h"
#include <time.h>
#include <sys/time.h>

// Debug control - uncomment to enable specific debugging
// #define DEBUG_LOOP_STATE_MACHINE
#define DEBUG_TIME_DISPLAY
#define DEBUG_USER_BUTTON_PRESS
// #define DEBUG_POWER_CONTROL
#define DEBUG_BATTERY_CHECK
// #define DEBUG_SLEEP_WAKEUP
// #define DEBUG_BOOT_BUTTON_PRESS
#define DEBUG_VSENSE_READING

namespace
{
    static constexpr const char* MODULE_PREFIX = "WordyWatch";
}

/// @brief Constructor for WordyWatch
/// @param pModuleName Module name
/// @param sysConfig System configuration interface
WordyWatch::WordyWatch(const char *pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig)
{
}

/// @brief Destructor for WordyWatch
WordyWatch::~WordyWatch()
{
}

/// @brief Setup the WordyWatch device
void WordyWatch::setup()
{
    _isConfigured = applyConfiguration();
    if (!_isConfigured)
    {
        LOG_E(MODULE_PREFIX, "setup configuration failed");
    }
    
    // Initialize wake time so device will enter sleep after timeout
    _wakeTimeMs = millis();
    LOG_I(MODULE_PREFIX, "setup complete, will enter sleep after %dms", _sleepAfterWakeMs);
}

/// @brief Main loop for the WordyWatch device (called frequently)
void WordyWatch::loop()
{
    // Handle state machine
    switch (_currentState)
    {
        case RUNNING:
            // Normal operation - check battery and button
            break;

        case PREPARING_TO_SLEEP:
            prepareForSleep();
            _currentState = SLEEPING;
            return;

        case SLEEPING:
            enterLightSleep();
            _currentState = WAKING_UP;
            return;

        case WAKING_UP:
            handleWakeup();
            _wakeTimeMs = millis();
            // No longer first boot after waking
            _isFirstBoot = false;
            _currentState = RUNNING;
            
            // If not displaying time, go back to sleep immediately
            if (!_displayingTime && !_isFirstBoot && _sleepAfterWakeMs <= 1)
            {
#ifdef DEBUG_LOOP_STATE_MACHINE
                LOG_I(MODULE_PREFIX, "loop button not pressed and sleepAfterWakeMs is %dms, going back to sleep", _sleepAfterWakeMs);
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("Loop: btn not pressed, sleep after %dms\r\n", _sleepAfterWakeMs);
                }
#endif
                _currentState = PREPARING_TO_SLEEP;
            }
            return;
    }

    // Check if displaying time duration has expired
    if (_displayingTime && Raft::isTimeout(millis(), _displayTimeStartMs, _showTimeForMs))
    {
#ifdef DEBUG_LOOP_STATE_MACHINE
        LOG_I(MODULE_PREFIX, "loop time display duration expired, going to sleep");
        _uartLogger.printf("Loop: time display expired, sleeping\r\n");
#endif
        _displayingTime = false;
        _currentState = PREPARING_TO_SLEEP;
        return;
    }

    // Check if we should go to sleep (if not displaying time)
    if (!_displayingTime && _autoSleepEnable && shouldGoToSleep())
    {
#ifdef DEBUG_LOOP_STATE_MACHINE
        LOG_I(MODULE_PREFIX, "loop auto-sleep triggered after %dms awake", (int)Raft::timeElapsed(millis(), _wakeTimeMs));
        if (_uartLogger.isInitialized())
        {
            _uartLogger.printf("Loop: auto-sleep after %dms\r\n", (int)Raft::timeElapsed(millis(), _wakeTimeMs));
        }
#endif
        _currentState = PREPARING_TO_SLEEP;
    }
}

/// @brief Apply configuration
/// @return true if configuration applied successfully
bool WordyWatch::applyConfiguration()
{
    // Get power control pin
    String pinName = config.getString("powerCtrlPin", "");
    _powerCtrlPin = ConfigPinMap::getPinFromName(pinName.c_str());

    // Set power control pin to ensure power remains on
    if (_powerCtrlPin >= 0)
    {
        // Set power control pin
        pinMode(_powerCtrlPin, OUTPUT);
        digitalWrite(_powerCtrlPin, HIGH);

        // Hold power control pin HIGH during sleep
        gpio_hold_en((gpio_num_t)_powerCtrlPin);

    }

    // Setup VSENSE pin
    pinName = config.getString("vsensePin", "");
    _vsensePin = ConfigPinMap::getPinFromName(pinName.c_str());
    if (_vsensePin >= 0)
    {
        // Set VSENSE pin
        pinMode(_vsensePin, INPUT);
    }

    // Get battery low voltage
    _batteryLowV = config.getDouble("batteryLowV", BATTERY_LOW_V_DEFAULT);

    // Get VSENSE button level
    _vsenseButtonLevel = config.getLong("vsenseButtonLevel", VSENSE_BUTTON_LEVEL_DEFAULT);

    // Get button off time
    _buttonOffTimeMs = config.getLong("buttonOffTimeMs", 2000);

    // Get sleep configuration
    _sleepAfterBootMs = config.getLong("sleepAfterBootMs", 5000);
    _sleepAfterWakeMs = config.getLong("sleepAfterWakeMs", 10000);
    _showTimeForMs = config.getLong("showTimeForMs", 5000);
    _timerWakeupMs = config.getLong("timerWakeupMs", 100);
    _autoSleepEnable = config.getBool("autoSleepEnable", true);

    // Get wake pin configuration
    pinName = config.getString("wakePinNum", "");
    _wakePinNum = ConfigPinMap::getPinFromName(pinName.c_str());
    _wakePinPullup = config.getBool("wakePinPullup", false);

    // Configure wake pin if specified
    if (_wakePinNum >= 0)
    {
        pinMode(_wakePinNum, INPUT);
        if (_wakePinPullup)
        {
            gpio_pullup_en((gpio_num_t)_wakePinNum);
        }
        LOG_I(MODULE_PREFIX, "setup wake pin %d pullup %s", _wakePinNum, _wakePinPullup ? "enabled" : "disabled");
    }

    // Get ADC calibration
    _vsenseSlope = VSENSE_SLOPE_DEFAULT;
    _vsenseIntercept = VSENSE_INTERCEPT_DEFAULT;
    double v1 = config.getDouble("adcCalib/v1", 0);
    int a1 = config.getLong("adcCalib/a1", 0);
    double v2 = config.getDouble("adcCalib/v2", 0);
    int a2 = config.getLong("adcCalib/a2", 0);
    LOG_I(MODULE_PREFIX, "setup powerCtrlPin %d vSensePin %d v1 %.2f a1 %d v2 %.2f a2 %d", 
                _powerCtrlPin, _vsensePin, v1, a1, v2, a2);

    // If a1 and a2 specified then use them
    if ((a1 > 0) && (a2 > 0))
    {
        _vsenseSlope = (v2 - v1) / (a2 - a1);
        _vsenseIntercept = v1 - _vsenseSlope * a1;
    }

    // Setup UART logger
    int uartLogPin = config.getLong("uartLogPin", -1);
    if (uartLogPin >= 0)
    {
        if (_uartLogger.begin(uartLogPin, 115200))
        {
            _uartLogger.printf("\r\n\r\n=== WordyWatch UART Logger Started ===\r\n");
            LOG_I(MODULE_PREFIX, "UART logger initialized on pin %d", uartLogPin);
        }
        else
        {
            LOG_E(MODULE_PREFIX, "Failed to initialize UART logger on pin %d", uartLogPin);
        }
    }

    // Debug
    if (_vsensePin > 0)
    {
        uint32_t adcReading = _vsensePin > 0 ? analogRead(_vsensePin) : 0;
        LOG_I(MODULE_PREFIX, "setup powerCtrlPin %d vSensePin %d currentADC %d currentVoltage %.2fV vsenseSlope %.5f vsenseIntercept %.2f batteryLowV %.2f vSenseButtonLevel %d buttonOffTime %dms", 
                    _powerCtrlPin, _vsensePin, (int)adcReading, 
                    getVoltageFromADCReading(adcReading), 
                    _vsenseSlope, _vsenseIntercept,
                    _batteryLowV, _vsenseButtonLevel, _buttonOffTimeMs);
        
        // Log to UART as well
        if (_uartLogger.isInitialized())
        {
            _uartLogger.printf("Setup: pwr=%d vsense=%d ADC=%d V=%.2f batLow=%.2f\r\n",
                _powerCtrlPin, _vsensePin, (int)adcReading, 
                getVoltageFromADCReading(adcReading), _batteryLowV);
        }
    }
    else
    {
        LOG_I(MODULE_PREFIX, "setup FAILED powerCtrlPin %d vSensePin INVALID vsenseSlope %.5f vsenseIntercept %.2f", 
                    _powerCtrlPin, _vsenseSlope, _vsenseIntercept);
    }

    return (_powerCtrlPin >= 0) && (_vsensePin >= 0);
}


/// @brief Convert ADC reading to voltage
/// @param adcReading ADC reading value
/// @return Calculated voltage
float WordyWatch::getVoltageFromADCReading(uint32_t adcReading) const
{
    // Convert to voltage
    return adcReading * _vsenseSlope + _vsenseIntercept;
}

/// @brief Check button press via VSENSE pin
/// @param vsenseVal Current VSENSE reading
void WordyWatch::checkUserButtonPress(uint32_t vsenseVal)
{
    // The pushbutton is wired to VSENSE so if it is pressed the VSENSE pin
    // will go above a threshold
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
                // Button released
#ifdef DEBUG_USER_BUTTON_PRESS
                LOG_I(MODULE_PREFIX, "checkUserButtonPress button pressed for %dms and released (button off time %dms)",
                        (int)Raft::timeElapsed(millis(), _buttonPressDownTimeMs), _buttonOffTimeMs);
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("Button: released after %dms\r\n", (int)Raft::timeElapsed(millis(), _buttonPressDownTimeMs));
                }
#endif // DEBUG_USER_BUTTON_PRESS
                _buttonPressed = false;
            }
        }
        else
        {
            // Average vsense values that are not button presses
            _vsenseAvg.sample(vsenseVal);
            _sampleCount++;
        }
    }
}

/// @brief Check battery level and initiate shutdown if low
void WordyWatch::checkBatteryLevel()
{
    // Check for shutdown due to battery low
    if (!_shutdownInitiated && (_sampleCount > 100))
    {
        // Get voltage
        float voltage = getVoltageFromADCReading(_vsenseAvg.getAverage());

        // Check for shutdown
        if (voltage < _batteryLowV)
        {
            // Debug
#ifdef DEBUG_BATTERY_CHECK
            if (Raft::isTimeout(millis(), _lastWarnBatLowShutdownTimeMs, 1000))
            {
                LOG_I(MODULE_PREFIX, "Battery low %s voltage %.2fV instADC %d avgADC %d battLowThreshold %.2fV", 
#ifdef FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
                        "shutting down",
#else
                        "!!! SHUTDOWN DISABLED !!!",
#endif // FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
                    voltage, analogRead(_vsensePin), _vsenseAvg.getAverage(), _batteryLowV);
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("Battery: LOW %.2fV (thresh=%.2fV)\r\n", voltage, _batteryLowV);
                }
                _lastWarnBatLowShutdownTimeMs = millis();
            }
#endif // DEBUG_BATTERY_CHECK

            // Shutdown initiated
#ifdef FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
            _shutdownInitiated = true;
#endif // FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
        }
    }
}

/// @brief Debug log power status
void WordyWatch::debugLogPowerStatus()
{
#ifdef DEBUG_POWER_CONTROL
    if (Raft::isTimeout(millis(), _lastDebugTimeMs, 1000))
    {
        uint32_t instVsense = analogRead(_vsensePin);
        uint32_t avgVsense = _vsenseAvg.getAverage();
        float voltage = getVoltageFromADCReading(avgVsense);
        
        LOG_I(MODULE_PREFIX, "debugLogPowerStatus vSense %d avgVSense %d Vcalculated %.2fV battLowThreshold %.2fV sampleCount %d buttonLevel %d",
                    instVsense, avgVsense, voltage, _batteryLowV, _sampleCount, _vsenseButtonLevel);
        
        if (_uartLogger.isInitialized())
        {
            _uartLogger.printf("Power: V=%.2f inst=%d avg=%d samples=%d\r\n", 
                voltage, instVsense, avgVsense, _sampleCount);
        }
        _lastDebugTimeMs = millis();
    }
#endif // DEBUG_POWER_CONTROL
}

/// @brief Handle shutdown
void WordyWatch::shutdown()
{
    // Disable hold on power control pin and set LOW
    if (_powerCtrlPin >= 0)
    {
        gpio_hold_dis((gpio_num_t)_powerCtrlPin);
        digitalWrite(_powerCtrlPin, LOW);
    }

    // Shutdown
    delay(TIME_TO_HOLD_POWER_CTRL_PIN_LOW_MS);

    // Enter light sleep with no wakeup
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_light_sleep_start();
}

/// @brief Prepare for sleep by stopping peripherals and holding GPIO
void WordyWatch::prepareForSleep()
{
    LOG_I(MODULE_PREFIX, "prepareForSleep stopping LED panel and holding power pin");

    // Get LED device and stop it
    DeviceManager* pDevMan = (DeviceManager*)getSysManager()->getSysMod("DevMan");
    if (pDevMan)
    {
        RaftDevice* pDevice = pDevMan->getDevice("LEDPanel");
        if (pDevice)
        {
            DeviceLEDCharlie* pLEDDevice = (DeviceLEDCharlie*)pDevice;
            pLEDDevice->getPanel().stop();
        }
    }
}

/// @brief Enter light sleep with timer wakeup
void WordyWatch::enterLightSleep()
{
    // Configure timer wakeup
    const uint64_t wakeup_time_us = _timerWakeupMs * 1000; // Convert ms to microseconds
    esp_sleep_enable_timer_wakeup(wakeup_time_us);
    LOG_I(MODULE_PREFIX, "enterLightSleep timer wakeup configured for %dms", _timerWakeupMs);

    LOG_I(MODULE_PREFIX, "enterLightSleep entering light sleep...");
    esp_light_sleep_start();
}

/// @brief Enter deep sleep with timer wakeup and power control held
void WordyWatch::enterDeepSleep()
{
    // Configure timer wakeup
    const uint64_t wakeup_time_us = _timerWakeupMs * 1000; // Convert ms to microseconds
    esp_sleep_enable_timer_wakeup(wakeup_time_us);
    LOG_I(MODULE_PREFIX, "enterDeepSleep timer wakeup configured for %dms", _timerWakeupMs);

    LOG_I(MODULE_PREFIX, "enterDeepSleep entering deep sleep with power pin %d held HIGH...", _powerCtrlPin);
    esp_deep_sleep_start();
}

/// @brief Handle wakeup from sleep
void WordyWatch::handleWakeup()
{
    // Check if wakeup button was pressed
    checkWakeupButtonPress();

    // Check battery level occasionally
    if (Raft::isTimeout(millis(), _lastBatteryCheckMs, BATTERY_CHECK_INTERVAL_MS))
    {
        _lastBatteryCheckMs = millis();
        
        if (_vsensePin >= 0)
        {
            // Get VSENSE value and update average
            uint32_t vsenseVal = analogRead(_vsensePin);
            _vsenseAvg.sample(vsenseVal);
            _sampleCount++;

#ifdef DEBUG_VSENSE_READING
      
            LOG_I(MODULE_PREFIX, "handleWakeup vsenseVal %d avgVSense %d calculatedV %.2fV",
                        vsenseVal, _vsenseAvg.getAverage(), getVoltageFromADCReading(_vsenseAvg.getAverage()));
            _uartLogger.printf("Wakeup: vsense=%d avg=%d V=%.2f\r\n",
                        vsenseVal, _vsenseAvg.getAverage(), getVoltageFromADCReading(_vsenseAvg.getAverage()));
#endif

            // Check battery level
            checkBatteryLevel();
            
            // Debug logging
            debugLogPowerStatus();
        }
    }
}

void WordyWatch::checkWakeupButtonPress()
{
    // Disable hold on power control pin to allow changes
    if (_powerCtrlPin >= 0)
    {
        gpio_hold_dis((gpio_num_t)_powerCtrlPin);
        digitalWrite(_powerCtrlPin, LOW);
    }

    // Restore pull-up on wake pin after waking from sleep
    if (_wakePinNum >= 0 && _wakePinPullup)
    {
        pinMode(_wakePinNum, INPUT);
        gpio_pullup_en((gpio_num_t)_wakePinNum);
        // Small delay to let pull-up stabilize
        delayMicroseconds(100);
    }

    // Check if wake button is pressed by reading GPIO directly
    bool wakeButtonPressed = false;
    if (_wakePinNum >= 0)
    {
        // Active LOW
        wakeButtonPressed = (gpio_get_level((gpio_num_t)_wakePinNum) == 0);

#ifdef DEBUG_BOOT_BUTTON_PRESS
        // Log to UART
        if (_uartLogger.isInitialized())
        {
            _uartLogger.printf("Wake: btn=%s\r\n", wakeButtonPressed ? "PRESS" : "none");
        }
#endif
    }

    // Re-enable hold on power control pin
    if (_powerCtrlPin >= 0)
    {
        digitalWrite(_powerCtrlPin, HIGH);
        gpio_hold_en((gpio_num_t)_powerCtrlPin);
    }


    // If button is pressed, show time and stay awake
    if (wakeButtonPressed)
    {
        // Get LED device and restart it
        DeviceManager* pDevMan = (DeviceManager*)getSysManager()->getSysMod("DevMan");
        if (pDevMan)
        {
            RaftDevice* pDevice = pDevMan->getDevice("LEDPanel");
            if (pDevice)
            {
                DeviceLEDCharlie* pLEDDevice = (DeviceLEDCharlie*)pDevice;
                pLEDDevice->getPanel().start();
                
                // Display current time
                updateTimeDisplay();
                
                // Set display time tracking
                _displayingTime = true;
                _displayTimeStartMs = millis();
#ifdef DEBUG_SLEEP_WAKEUP
                LOG_I(MODULE_PREFIX, "handleWakeup showing time for %dms", _showTimeForMs);
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("Wakeup: showing time for %dms\r\n", _showTimeForMs);
                }
#endif
            }
        }
    }
    else
    {
        // Button not pressed, don't display time
        _displayingTime = false;
        // LOG_I(MODULE_PREFIX, "handleWakeup button not pressed, will sleep immediately");
    }
}

/// @brief Update time display on LED panel
void WordyWatch::updateTimeDisplay()
{
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

#ifdef DEBUG_TIME_DISPLAY
    LOG_I(MODULE_PREFIX, "updateTimeDisplay showing time %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    if (_uartLogger.isInitialized())
    {
        _uartLogger.printf("Time: %02d:%02d\r\n", timeinfo.tm_hour, timeinfo.tm_min);
    }
#endif

    // Get LED device and display time
    DeviceManager* pDevMan = (DeviceManager*)getSysManager()->getSysMod("DevMan");
    if (pDevMan)
    {
        RaftDevice* pDevice = pDevMan->getDevice("LEDPanel");
        if (pDevice)
        {
            DeviceLEDCharlie* pLEDDevice = (DeviceLEDCharlie*)pDevice;
            pLEDDevice->displayTime(timeinfo.tm_hour, timeinfo.tm_min);
        }
    }
}

/// @brief Check if should go to sleep based on timeout
bool WordyWatch::shouldGoToSleep()
{
    // Don't sleep if time not initialized
    if (_wakeTimeMs == 0)
        return false;

    // Use appropriate timeout based on boot state
    uint32_t sleepTimeout = _isFirstBoot ? _sleepAfterBootMs : _sleepAfterWakeMs;
    
    // Check if awake time exceeded
    return Raft::timeElapsed(millis(), _wakeTimeMs) > sleepTimeout;
}

/// @brief Add REST API endpoints
/// @param endpointManager Manager for REST API endpoints
void WordyWatch::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
}

/// @brief Get the device status as JSON
/// @return JSON string
String WordyWatch::getStatusJSON() const
{
    String json = "{\"vSense\":" + String(analogRead(_vsensePin)) +
        ",\"avgVSense\":" + String(_vsenseAvg.getAverage()) +
        ",\"calculatedV\":" + String(getVoltageFromADCReading(_vsenseAvg.getAverage()), 2) +
        ",\"batteryLowV\":" + String(_batteryLowV, 2) +
        ",\"sampleCount\":" + String(_sampleCount) +
        ",\"buttonLevel\":" + String(_vsenseButtonLevel) +
        "}";
    return json;
}
