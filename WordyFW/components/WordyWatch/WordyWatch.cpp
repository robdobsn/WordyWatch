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

#define DEBUG_USER_BUTTON_PRESS
#define DEBUG_POWER_CONTROL

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
            _currentState = RUNNING;
            _wakeTimeMs = millis();
            return;
    }

    // Update VSENSE average
    if (_vsensePin < 0)
        return;

    // Get VSENSE value
    uint32_t vsenseVal = analogRead(_vsensePin);

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

        // Check if button press is over the off time threshold
        if (Raft::timeElapsed(millis(), _buttonPressDownTimeMs) > _buttonOffTimeMs)
        {
            // Debug
#ifdef DEBUG_USER_BUTTON_PRESS
            if (Raft::isTimeout(millis(), _lastWarnUserShutdownTimeMs, 1000))
            {
                LOG_I(MODULE_PREFIX, "loop button pressed for %dms (vsense %d buttonLevel %d buttonOffTime %dms)",
                    (int)Raft::timeElapsed(millis(), _buttonPressDownTimeMs), vsenseVal, _vsenseButtonLevel, _buttonOffTimeMs);
                _lastWarnUserShutdownTimeMs = millis();
            }
#endif // DEBUG_USER_BUTTON_PRESS

            // Shutdown initiated
#ifdef FEATURE_POWER_CONTROL_USER_SHUTDOWN
            _shutdownInitiated = true;
#endif // FEATURE_POWER_CONTROL_USER_SHUTDOWN
        }
    }
    else
    {
        // Not pressed - debounce
        if (_buttonPressed)
        {
            if (Raft::isTimeout(millis(), _buttonPressChangeTimeMs, 200))
            {
                // Button pressed
#ifdef DEBUG_USER_BUTTON_PRESS
                LOG_I(MODULE_PREFIX, "loop button pressed for %dms and released (button off time %dms)",
                        (int)Raft::timeElapsed(millis(), _buttonPressDownTimeMs), _buttonOffTimeMs);
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

    // Debug
#ifdef DEBUG_POWER_CONTROL
    if (Raft::isTimeout(millis(), _lastDebugTimeMs, 1000))
    {
        LOG_I(MODULE_PREFIX, "loop vSense %d avgVSense %d Vcalculated %.2fV battLowThreshold %.2fV sampleCount %d buttonLevel %d",
                    analogRead(_vsensePin), 
                    _vsenseAvg.getAverage(),
                    getVoltageFromADCReading(_vsenseAvg.getAverage()),
                    _batteryLowV,
                    _sampleCount,
                    _vsenseButtonLevel);
        _lastDebugTimeMs = millis();
    }
#endif // DEBUG_POWER_CONTROL

    // Check for shutdown due to battery low
    if (!_shutdownInitiated && (_sampleCount > 100))
    {
        // Get voltage
        float voltage = getVoltageFromADCReading(_vsenseAvg.getAverage());

        // Check for shutdown
        if (voltage < _batteryLowV)
        {
            // Debug
#ifdef DEBUG_POWER_CONTROL
            if (Raft::isTimeout(millis(), _lastWarnBatLowShutdownTimeMs, 1000))
            {
                LOG_I(MODULE_PREFIX, "Battery low %s voltage %.2fV instADC %d avgADC %d battLowThreshold %.2fV", 
#ifdef FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
                        "shutting down",
#else
                        "!!! SHUTDOWN DISABLED !!!",
#endif // FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
                    voltage, analogRead(_vsensePin), _vsenseAvg.getAverage(), _batteryLowV);
                _lastWarnBatLowShutdownTimeMs = millis();
            }
#endif // DEBUG_POWER_CONTROL

            // Shutdown initiated
#ifdef FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
            _shutdownInitiated = true;
#endif // FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN
        }
    }

    // Check if should go to sleep
    if (_autoSleepEnable && shouldGoToSleep())
    {
        LOG_I(MODULE_PREFIX, "loop auto-sleep triggered after %dms awake", (int)Raft::timeElapsed(millis(), _wakeTimeMs));
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
    }

    // Signal isolation pin
    pinName = config.getString("sigIsoPin", "");
    _sigIsoPin = ConfigPinMap::getPinFromName(pinName.c_str());
    if (_sigIsoPin >= 0)
    {
        // Set signal isolation pin (to disable isolation)
        pinMode(_sigIsoPin, OUTPUT);
        digitalWrite(_sigIsoPin, HIGH);
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
    _sleepAfterWakeMs = config.getLong("sleepAfterWakeMs", 10000);
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

    // Debug
    if (_vsensePin > 0)
    {
        uint32_t adcReading = _vsensePin > 0 ? analogRead(_vsensePin) : 0;
        LOG_I(MODULE_PREFIX, "setup powerCtrlPin %d vSensePin %d currentADC %d currentVoltage %.2fV vsenseSlope %.5f vsenseIntercept %.2f batteryLowV %.2f vSenseButtonLevel %d buttonOffTime %dms", 
                    _powerCtrlPin, _vsensePin, (int)adcReading, 
                    getVoltageFromADCReading(adcReading), 
                    _vsenseSlope, _vsenseIntercept,
                    _batteryLowV, _vsenseButtonLevel, _buttonOffTimeMs);
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

/// @brief Handle shutdown
void WordyWatch::shutdown()
{
    // Shutdown
    digitalWrite(_powerCtrlPin, LOW);
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

    // Hold power control pin HIGH during sleep
    if (_powerCtrlPin >= 0)
    {
        gpio_hold_en((gpio_num_t)_powerCtrlPin);
    }
}

/// @brief Enter light sleep with wake pin configured
void WordyWatch::enterLightSleep()
{
    // Configure GPIO wakeup if wake pin is specified
    if (_wakePinNum >= 0)
    {
        // Wake on LOW level (button press pulls pin low)
        // ESP32-C6 uses ext1 wakeup for GPIO 0-7
        uint64_t pin_mask = 1ULL << _wakePinNum;
        esp_sleep_enable_ext1_wakeup_io(pin_mask, ESP_EXT1_WAKEUP_ANY_LOW);
        LOG_I(MODULE_PREFIX, "enterLightSleep wake pin %d (mask 0x%llx) configured for LOW wake", _wakePinNum, pin_mask);
    }
    else
    {
        LOG_W(MODULE_PREFIX, "enterLightSleep no wake pin configured!");
    }

    LOG_I(MODULE_PREFIX, "enterLightSleep entering light sleep...");
    esp_light_sleep_start();
}

/// @brief Handle wakeup from sleep
void WordyWatch::handleWakeup()
{
    // Get wakeup cause
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    const char* causeStr = "UNKNOWN";
    switch (wakeup_cause)
    {
        case ESP_SLEEP_WAKEUP_EXT0: causeStr = "EXT0 (GPIO)"; break;
        case ESP_SLEEP_WAKEUP_EXT1: causeStr = "EXT1 (GPIO)"; break;
        case ESP_SLEEP_WAKEUP_TIMER: causeStr = "TIMER"; break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: causeStr = "TOUCHPAD"; break;
        case ESP_SLEEP_WAKEUP_ULP: causeStr = "ULP"; break;
        default: break;
    }
    LOG_I(MODULE_PREFIX, "handleWakeup woke from light sleep, cause: %s", causeStr);

    // Release GPIO holds
    if (_powerCtrlPin >= 0)
    {
        gpio_hold_dis((gpio_num_t)_powerCtrlPin);
    }

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
        }
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

    LOG_I(MODULE_PREFIX, "updateTimeDisplay showing time %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

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

    // Check if awake time exceeded
    return Raft::timeElapsed(millis(), _wakeTimeMs) > _sleepAfterWakeMs;
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
