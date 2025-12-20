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
#include "driver/i2c_master.h"
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
    // Get I2C configuration
    pinName = config.getString("i2cSdaPin", "");
    _i2cSdaPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = config.getString("i2cSclPin", "");
    _i2cSclPin = ConfigPinMap::getPinFromName(pinName.c_str());
    _i2cFreqHz = config.getLong("i2cFreqHz", 100000);
    _accelI2CAddr = config.getLong("accelI2CAddr", 0x6a);
    _rtcI2CAddr = config.getLong("rtcI2CAddr", 0x68);

    // Initialize I2C if pins configured
    if (_i2cSdaPin >= 0 && _i2cSclPin >= 0)
    {
        if (initI2C())
        {
            LOG_I(MODULE_PREFIX, "I2C initialized on SDA=%d SCL=%d freq=%ldHz", _i2cSdaPin, _i2cSclPin, _i2cFreqHz);
            if (_uartLogger.isInitialized())
            {
                _uartLogger.printf("I2C: SDA=%d SCL=%d freq=%ldHz\r\n", _i2cSdaPin, _i2cSclPin, _i2cFreqHz);
            }
            
            // Initialize accelerometer
            if (initAccelerometer())
            {
                LOG_I(MODULE_PREFIX, "Accelerometer initialized at address 0x%02x", _accelI2CAddr);
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("Accel: initialized at 0x%02x\r\n", _accelI2CAddr);
                }
            }
            else
            {
                LOG_E(MODULE_PREFIX, "Failed to initialize accelerometer");
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("Accel: FAILED to initialize\r\n");
                }
            }
            
            // Initialize RTC
            if (initRTC())
            {
                LOG_I(MODULE_PREFIX, "RTC initialized at address 0x%02x", _rtcI2CAddr);
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("RTC: initialized at 0x%02x\r\n", _rtcI2CAddr);
                }
                
                // Read initial time from RTC and set system time
                struct tm timeinfo;
                if (readRTCTime(&timeinfo))
                {
                    struct timeval tv;
                    tv.tv_sec = mktime(&timeinfo);
                    tv.tv_usec = 0;
                    settimeofday(&tv, NULL);
                    LOG_I(MODULE_PREFIX, "System time set from RTC: %04d-%02d-%02d %02d:%02d:%02d", 
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    if (_uartLogger.isInitialized())
                    {
                        _uartLogger.printf("RTC: Time %04d-%02d-%02d %02d:%02d:%02d\r\n",
                                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    }
                }
            }
            else
            {
                LOG_E(MODULE_PREFIX, "Failed to initialize RTC");
                if (_uartLogger.isInitialized())
                {
                    _uartLogger.printf("RTC: FAILED to initialize\r\n");
                }
            }
        }
        else
        {
            LOG_E(MODULE_PREFIX, "Failed to initialize I2C");
            if (_uartLogger.isInitialized())
            {
                _uartLogger.printf("I2C: FAILED to initialize\r\n");
            }
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
    // Get current time from RTC
    struct tm timeinfo;
    if (readRTCTime(&timeinfo))
    {
        // Update system time from RTC
        struct timeval tv;
        tv.tv_sec = mktime(&timeinfo);
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }
    else
    {
        // Fall back to system time if RTC read fails
        time_t now;
        time(&now);
        localtime_r(&now, &timeinfo);
    }

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

/// @brief Initialize I2C master bus
/// @return True if successful
bool WordyWatch::initI2C()
{
    // Configure I2C master bus
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.scl_io_num = (gpio_num_t)_i2cSclPin;
    bus_config.sda_io_num = (gpio_num_t)_i2cSdaPin;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    // Create I2C master bus
    esp_err_t err = i2c_new_master_bus(&bus_config, &_i2cBusHandle);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "initI2C failed to create master bus: %s", esp_err_to_name(err));
        return false;
    }

    // Configure accelerometer device
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = _accelI2CAddr;
    dev_config.scl_speed_hz = _i2cFreqHz;

    // Add device to bus
    err = i2c_master_bus_add_device(_i2cBusHandle, &dev_config, &_accelDevHandle);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "initI2C failed to add device: %s", esp_err_to_name(err));
        i2c_del_master_bus(_i2cBusHandle);
        _i2cBusHandle = nullptr;
        return false;
    }

    return true;
}

/// @brief Initialize LSM6DS accelerometer with wrist tilt detection
/// @return True if successful
bool WordyWatch::initAccelerometer()
{
    if (!_accelDevHandle)
    {
        LOG_E(MODULE_PREFIX, "initAccelerometer: I2C device not initialized");
        return false;
    }

    // Initialization sequence from DevTypes.json
    // 0x1018 (26Hz ODR, ±2g), 0x1100 (Gyro OFF), 0x1240 (open-drain INT), 
    // 0x5880 (enable embedded functions), 0x1980 (enable wrist tilt), 
    // 0x5F80 (route to INT2)
    struct RegValue
    {
        uint8_t reg;
        uint8_t value;
    };
    
    const RegValue initSequence[] = {
        {0x10, 0x18},  // CTRL1_XL: 26Hz ODR, ±2g
        {0x11, 0x00},  // CTRL2_G: Gyro OFF
        {0x12, 0x40},  // CTRL3_C: open-drain INT
        {0x58, 0x80},  // TAP_CFG0: enable embedded functions
        {0x19, 0x80},  // CTRL10_C: enable wrist tilt
        {0x5F, 0x80}   // MD2_CFG: route to INT2
    };

    // Write each register
    for (size_t i = 0; i < sizeof(initSequence) / sizeof(initSequence[0]); i++)
    {
        uint8_t write_buf[2] = {initSequence[i].reg, initSequence[i].value};
        esp_err_t err = i2c_master_transmit(_accelDevHandle, write_buf, sizeof(write_buf), 1000);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "initAccelerometer: failed to write reg 0x%02x: %s", 
                  initSequence[i].reg, esp_err_to_name(err));
            return false;
        }
        
        // Small delay between writes
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    LOG_I(MODULE_PREFIX, "initAccelerometer: configured LSM6DS for wrist tilt detection");
    return true;
}

/// @brief Deinitialize I2C (call before sleep if needed)
void WordyWatch::deinitI2C()
{
    if (_accelDevHandle)
    {
        i2c_master_bus_rm_device(_accelDevHandle);
        _accelDevHandle = nullptr;
    }
    
    if (_rtcDevHandle)
    {
        i2c_master_bus_rm_device(_rtcDevHandle);
        _rtcDevHandle = nullptr;
    }
    
    if (_i2cBusHandle)
    {
        i2c_del_master_bus(_i2cBusHandle);
        _i2cBusHandle = nullptr;
    }
}

/// @brief Initialize RV-4162-C7 RTC device
/// @return True if successful
bool WordyWatch::initRTC()
{
    if (!_i2cBusHandle)
    {
        LOG_E(MODULE_PREFIX, "initRTC: I2C bus not initialized");
        return false;
    }

    // Configure RTC device
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = _rtcI2CAddr;
    dev_config.scl_speed_hz = _i2cFreqHz;

    // Add device to bus
    esp_err_t err = i2c_master_bus_add_device(_i2cBusHandle, &dev_config, &_rtcDevHandle);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "initRTC: failed to add device: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

/// @brief Read time from RV-4162-C7 RTC
/// @param timeinfo Pointer to tm structure to fill with time data
/// @return True if successful
bool WordyWatch::readRTCTime(struct tm* timeinfo)
{
    if (!_rtcDevHandle || !timeinfo)
    {
        return false;
    }

    // RV-4162-C7 time registers start at 0x01
    // Read 7 bytes: Seconds, Minutes, Hours, Weekday, Date, Month, Year
    uint8_t reg_addr = 0x01;
    uint8_t time_data[7];
    
    esp_err_t err = i2c_master_transmit_receive(_rtcDevHandle, &reg_addr, 1, time_data, sizeof(time_data), 1000);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "readRTCTime: I2C read failed: %s", esp_err_to_name(err));
        return false;
    }

    // Log raw RTC data for debugging
    LOG_I(MODULE_PREFIX, "readRTCTime: Raw RTC data: %02X %02X %02X %02X %02X %02X %02X",
          time_data[0], time_data[1], time_data[2], time_data[3], 
          time_data[4], time_data[5], time_data[6]);
    if (_uartLogger.isInitialized())
    {
        _uartLogger.printf("RTC raw: %02X %02X %02X %02X %02X %02X %02X\r\n",
                          time_data[0], time_data[1], time_data[2], time_data[3], 
                          time_data[4], time_data[5], time_data[6]);
    }

    // Convert BCD to decimal
    auto bcd_to_dec = [](uint8_t bcd) -> uint8_t {
        return ((bcd >> 4) * 10) + (bcd & 0x0F);
    };

    // Parse time data (all in BCD format)
    timeinfo->tm_sec = bcd_to_dec(time_data[0] & 0x7F);      // 0x01: Seconds (0-59)
    timeinfo->tm_min = bcd_to_dec(time_data[1] & 0x7F);      // 0x02: Minutes (0-59)
    timeinfo->tm_hour = bcd_to_dec(time_data[2] & 0x3F);     // 0x03: Hours (0-23)
    timeinfo->tm_wday = bcd_to_dec(time_data[3] & 0x07);     // 0x04: Weekday (0-6)
    timeinfo->tm_mday = bcd_to_dec(time_data[4] & 0x3F);     // 0x05: Date (1-31)
    timeinfo->tm_mon = bcd_to_dec(time_data[5] & 0x1F) - 1;  // 0x06: Month (1-12) -> (0-11)
    timeinfo->tm_year = bcd_to_dec(time_data[6]) + 100;      // 0x07: Year (00-99) -> years since 1900

    // Set DST flag to -1 (unknown)
    timeinfo->tm_isdst = -1;

    return true;
}
