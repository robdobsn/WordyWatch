/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wordy Watch
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "WordyWatch.h"
#include "RaftJsonIF.h"
#include "RaftJson.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/i2c_master.h"
#include "SysManager.h"
#include "APISourceInfo.h"
#include "RestAPIEndpointManager.h"
#include <time.h>
#include <sys/time.h>

// Debug control - uncomment to enable specific debugging
#define DEBUG_LOOP_STATE_MACHINE

namespace
{
    static constexpr const char* MODULE_PREFIX = "WordyWatch";
    static constexpr const char* SETTINGS_NAMESPACE = "WordyWatchSettings";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor for WordyWatch
/// @param pModuleName Module name
/// @param sysConfig System configuration interface
WordyWatch::WordyWatch(const char *pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig),
    _display(*this),
    _settingsNVS(SETTINGS_NAMESPACE)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Destructor for WordyWatch
WordyWatch::~WordyWatch()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Setup the WordyWatch device
void WordyWatch::setup()
{
 // Get power control pin
    String pinName = config.getString("powerCtrlPin", "");
    int powerCtrlPin = ConfigPinMap::getPinFromName(pinName.c_str());

    // Get the strap control pin
    pinName = config.getString("strapCtrlPin", "");
    int strapCtrlPin = ConfigPinMap::getPinFromName(pinName.c_str());

    // Setup VSENSE pin
    pinName = config.getString("vsensePin", "");
    int vsensePin = ConfigPinMap::getPinFromName(pinName.c_str());

    // Get battery low voltage
    float batteryLowV = config.getDouble("batteryLowV", 3.55);

    // Get power button VSENSE level
    uint32_t powerButtonVsenseLevel = config.getLong("powerButtonVsenseLevel",
                                                     config.getLong("vsenseButtonLevel", 2300));

    // Get power button off time
    uint32_t powerButtonOffTimeMs = config.getLong("powerButtonOffTimeMs",
                                                   config.getLong("buttonOffTimeMs", 2000));

    // Get ADC calibration
    double vsenseSlope = 0.00223;  // VSENSE_SLOPE_DEFAULT
    double vsenseIntercept = 0.0;   // VSENSE_INTERCEPT_DEFAULT
    double v1 = config.getDouble("adcCalib/v1", 0);
    int a1 = config.getLong("adcCalib/a1", 0);
    double v2 = config.getDouble("adcCalib/v2", 0);
    int a2 = config.getLong("adcCalib/a2", 0);

    // If a1 and a2 specified then use them
    if ((a1 > 0) && (a2 > 0))
    {
        vsenseSlope = (v2 - v1) / (a2 - a1);
        vsenseIntercept = v1 - vsenseSlope * a1;
    }

    // Get WAKE_INT pin configuration
    pinName = config.getString("wakeIntPinNum", "");
    if (pinName.length() == 0)
        pinName = config.getString("wakePinNum", "");
    int wakeIntPinNum = ConfigPinMap::getPinFromName(pinName.c_str());
    bool wakeIntPinPullup = config.getBool("wakeIntPinPullup",
                                           config.getBool("wakePinPullup", false));

    // Get BOOT button configuration
    pinName = config.getString("bootButtonPinNum", "");
    _bootButtonPinNum = ConfigPinMap::getPinFromName(pinName.c_str());
    _bootButtonPullup = config.getBool("bootButtonPullup", false);
    if (_bootButtonPinNum >= 0)
    {
        pinMode(_bootButtonPinNum, INPUT);
        if (_bootButtonPullup)
            gpio_pullup_en((gpio_num_t)_bootButtonPinNum);
    }

    // Configure power management
    _powerAndSleep.configure(powerCtrlPin, strapCtrlPin, vsensePin,
                     vsenseSlope, vsenseIntercept,
                     batteryLowV, powerButtonVsenseLevel, powerButtonOffTimeMs,
                     wakeIntPinNum, wakeIntPinPullup);

    // Get sleep configuration
    _showTimeForMs = config.getLong("showTimeForMs", 10000);

    // Battery gauge display configuration
    _batteryGaugeShowMs = config.getLong("batteryGaugeShowMs", 1500);
    _batteryGaugeSweepMs = config.getLong("batteryGaugeSweepMs", 333);
    _batteryGaugeMinV = config.getDouble("batteryGaugeMinV", batteryLowV);
    _batteryGaugeMaxV = config.getDouble("batteryGaugeMaxV", 4.2);
    if (_batteryGaugeMaxV <= _batteryGaugeMinV)
    {
        _batteryGaugeMinV = batteryLowV;
        _batteryGaugeMaxV = 4.2f;
    }
    if (_batteryGaugeSweepMs == 0)
        _batteryGaugeSweepMs = 1;

    // Ball simulation game configuration
    int ballCount = config.getLong("balls/count", 12);
    float ballViscosity = config.getDouble("balls/viscosity", 0.5);
    _gameMode.configureBallSim(ballCount, ballViscosity);

    // Breakout game configuration
    int breakoutBrickRows = config.getLong("breakout/brickRows", 0);
    uint32_t breakoutBallTickMs = static_cast<uint32_t>(config.getLong("breakout/ballTickMs", 100));
    float breakoutPaddleSpeed = config.getDouble("breakout/paddleSpeed", 0.25);
    _gameMode.configureBreakout(breakoutBrickRows, breakoutBallTickMs, breakoutPaddleSpeed);

    // Override with persisted settings if present
    long persistedShowTimeMs = _settingsNVS.getLong("showTimeForMs", -1);
    if (persistedShowTimeMs >= 0)
    {
        _showTimeForMs = static_cast<uint32_t>(persistedShowTimeMs);
    }
    else
    {
        long persistedShowSecs = _settingsNVS.getLong("showsecs", -1);
        if (persistedShowSecs >= 0)
        {
            uint64_t showTimeMs = static_cast<uint64_t>(persistedShowSecs) * 1000ULL;
            if (showTimeMs > UINT32_MAX)
                showTimeMs = UINT32_MAX;
            _showTimeForMs = static_cast<uint32_t>(showTimeMs);
        }
    }

    // Get minute resolution
    _minuteResolution = config.getLong("minuteResolution", 5);

    auto clampByte = [](long value) {
        if (value < 0)
            return static_cast<uint8_t>(0);
        if (value > 0xFF)
            return static_cast<uint8_t>(0xFF);
        return static_cast<uint8_t>(value);
    };

    uint8_t wristLatency = clampByte(config.getLong("WristTilt/latency", 8));
    uint8_t wristThreshold = clampByte(config.getLong("WristTilt/threshold", 16));
    uint8_t wristMask = clampByte(config.getLong("WristTilt/mask", 0xFC));
    _accelerometer.setWristTiltConfig(wristLatency, wristThreshold, wristMask);

    // Get I2C configuration
    pinName = config.getString("i2cSdaPin", "");
    _i2cSdaPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = config.getString("i2cSclPin", "");
    _i2cSclPin = ConfigPinMap::getPinFromName(pinName.c_str());
    _i2cFreqHz = config.getLong("i2cFreqHz", 100000);
    _accelerometer.setI2CAddress(config.getLong("accelI2CAddr", 0x6a));
    _rtc.setI2CConfig(config.getLong("rtcI2CAddr", 0x52), config.getString("rtcDevType", ""));

    // Initialize I2C if pins configured
    if (initI2C())
    {
        LOG_I(MODULE_PREFIX, "I2C initialized on SDA=%d SCL=%d freq=%ldHz", _i2cSdaPin, _i2cSclPin, _i2cFreqHz);
        
        // Initialize accelerometer
        _accelerometer.init();
        
        // Initialize RTC
        _rtc.init();
        _rtc.disableInterrupts();
        _rtc.setSystemTimeFromRTC();
    }
    else
    {
        LOG_E(MODULE_PREFIX, "Failed to initialize I2C");
    }

#ifdef DEBUG_WRIST_TILT_INT
    // Install GPIO ISR to count wrist tilt interrupts on WAKE_INT pin
    _accelerometer.setupWristTiltInterrupt(wakeIntPinNum);
#endif

    // Debug - do initial power reading
    bool isShutdownRequired = _powerAndSleep.update();
    LOG_I(MODULE_PREFIX, "setup powerCtrlPin %d strapCtrlPin %d vSensePin %d currentADC %d currentVoltage %.2fV batteryLowV %.2f isShutdownRequired %d powerButtonVsenseLevel %d powerButtonOffTime %dms", 
                powerCtrlPin, strapCtrlPin, vsensePin, 
                (int)_powerAndSleep.getVSENSEReading(), 
                _powerAndSleep.getBatteryVoltage(), 
                batteryLowV, isShutdownRequired, powerButtonVsenseLevel, powerButtonOffTimeMs);
    
    // Initialize time last woken
    _timeLastWokenMs = millis();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Main loop for the WordyWatch device (called frequently)
void WordyWatch::loop()
{
#ifdef DEBUG_WRIST_TILT_INT
    // Periodically log wrist tilt interrupt count
    _accelerometer.logWristTiltInterrupts(_rtc);
#endif

    // Update power management in all states - returns true if shutdown required
    bool shutdownRequired = _powerAndSleep.update();
    if (shutdownRequired && (_currentState != SHUTTING_DOWN) && (_currentState != SHUTDOWN_REQUESTED))
    {
        LOG_I(MODULE_PREFIX, "loop shutdown requested by power management");
        setState(SHUTDOWN_REQUESTED);
    }

    // Handle state machine
    switch (_currentState)
    {
        case INITIAL_STATE:
            // Initial state - go to displaying time
            LOG_I(MODULE_PREFIX, "loop initial state, going to display time");
            updateDisplayWithMinuteIndicators(true);
            setState(DISPLAYING_TIME);
            return;
        case DISPLAYING_TIME:
            // Displaying time
            {
                bool bootButtonPressed = false;
                if (_bootButtonPinNum >= 0)
                    bootButtonPressed = (gpio_get_level((gpio_num_t)_bootButtonPinNum) == 0);

                if (bootButtonPressed)
                {
                    if (!_bootButtonDown)
                    {
                        _bootButtonDown = true;
                        _bootButtonPressStartMs = millis();
                    }
                }
                else
                {
                    if (_bootButtonDown)
                    {
                        uint32_t pressDurationMs = Raft::timeElapsed(millis(), _bootButtonPressStartMs);
                        _bootButtonDown = false;
                        _bootButtonPressStartMs = 0;
                        if ((pressDurationMs < 1500) && !_batteryGaugeActive)
                        {
                            float batteryV = _powerAndSleep.getBatteryVoltage();
                            if (batteryV < 0.0f)
                                batteryV = 0.0f;

                            float fraction = 0.0f;
                            if (_batteryGaugeMaxV > _batteryGaugeMinV)
                            {
                                fraction = (batteryV - _batteryGaugeMinV) / (_batteryGaugeMaxV - _batteryGaugeMinV);
                            }

                            if (fraction < 0.0f)
                                fraction = 0.0f;
                            if (fraction > 1.0f)
                                fraction = 1.0f;

                            uint8_t ledCount = static_cast<uint8_t>(fraction * LED_GRID_WIDTH + 0.5f);
                            if (ledCount > LED_GRID_WIDTH)
                                ledCount = LED_GRID_WIDTH;

#ifdef DEBUG_LOOP_STATE_MACHINE
                            LOG_I(MODULE_PREFIX, "battery gauge show V=%.2f min=%.2f max=%.2f leds=%u", batteryV, _batteryGaugeMinV,
                                  _batteryGaugeMaxV, static_cast<unsigned>(ledCount));
#endif
                            _batteryGaugeActive = true;
                            _batteryGaugeStartMs = millis();
                            _batteryGaugeSweepStartMs = _batteryGaugeStartMs;
                            _batteryGaugeTargetLeds = ledCount;
                            _batteryGaugeLastShown = 0;
                        }
                    }
                }

                if (_bootButtonDown && !_batteryGaugeActive)
                {
                    if (Raft::timeElapsed(millis(), _bootButtonPressStartMs) >= 1500)
                    {
                        _batteryGaugeActive = false;
                        _gameMode.start(millis());
                        setState(GAME_MODE);
                        return;
                    }
                }

                if (_batteryGaugeActive)
                {
                    if (_batteryGaugeTargetLeds == 0)
                    {
                        if (_batteryGaugeLastShown != 0)
                        {
                            _display.showBatteryGaugeWithMinuteIndicators(0);
                            _batteryGaugeLastShown = 0;
                        }
                        else if (_batteryGaugeLastShown == 0)
                        {
                            _display.showBatteryGaugeWithMinuteIndicators(0);
                        }
                    }
                    else
                    {
                        const uint32_t sweepDurationMs = _batteryGaugeSweepMs;
                        uint32_t elapsedMs = Raft::timeElapsed(millis(), _batteryGaugeSweepStartMs);
                        uint32_t stepMs = sweepDurationMs / _batteryGaugeTargetLeds;
                        if (stepMs == 0)
                            stepMs = 1;
                        uint32_t stepIndex = (elapsedMs % sweepDurationMs) / stepMs;
                        uint8_t animLeds = static_cast<uint8_t>(stepIndex + 1);
                        if (animLeds > _batteryGaugeTargetLeds)
                            animLeds = _batteryGaugeTargetLeds;

                        if (animLeds != _batteryGaugeLastShown)
                        {
                            _display.showBatteryGaugeWithMinuteIndicators(animLeds);
                            _batteryGaugeLastShown = animLeds;
                        }
                    }

                    if (Raft::isTimeout(millis(), _batteryGaugeStartMs, _batteryGaugeShowMs))
                    {
                        _batteryGaugeActive = false;
                        updateDisplayWithMinuteIndicators(true);
                    }
                    return;
                }

                updateDisplayWithMinuteIndicators(false);
            }
            if (Raft::isTimeout(millis(), _currentStateStartMs, _showTimeForMs))
            {
#ifdef DEBUG_LOOP_STATE_MACHINE
                LOG_I(MODULE_PREFIX, "loop time display duration expired, going to sleep");
#endif
                setState(PREPARING_TO_SLEEP);
                return;
            }
            break;

        case GAME_MODE:
        {
            bool bootButtonPressed = false;
            if (_bootButtonPinNum >= 0)
                bootButtonPressed = (gpio_get_level((gpio_num_t)_bootButtonPinNum) == 0);

            GameMode::UpdateResult result = _gameMode.update(_accelerometer, bootButtonPressed, millis());
            _gameMode.render(_display);

            if (result.exitRequested)
            {
                _bootButtonDown = false;
                _bootButtonPressStartMs = 0;
                updateDisplayWithMinuteIndicators(true);
                setState(DISPLAYING_TIME);
                return;
            }
            return;
        }

        case PREPARING_TO_SLEEP:
            _display.clear();
            setState(SLEEPING);
            return;

        case SLEEPING:
#ifdef FEATURE_POWER_CONTROL_ENABLE_SLEEP
            _powerAndSleep.enterDeepSleep(-1);

            // Note that we will not return from deep sleep as it will reset the CPU
            setState(WOKEN_UP);
#else
            return;
#endif
            return;

        case WOKEN_UP:
            // Check wakeup reason
            _powerAndSleep.checkWakeupReason(_accelerometer, _rtc);
            _timeLastWokenMs = millis();
            _lastDisplayedSecond = -1;
            _lastDisplayedMinute = -1;
            setState(DISPLAYING_TIME);
            return;
        case SHUTDOWN_REQUESTED:
            // Clear the display
            _display.clear();
            _powerAndSleep.shutdown();
            setState(SHUTTING_DOWN);
            return;
        case SHUTTING_DOWN:
            // Do nothing - waiting for shutdown            
            return;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Update display with minute indicators
bool WordyWatch::updateDisplayWithMinuteIndicators(bool force)
{
    time_t now = time(nullptr);
    if (now <= 0)
        return false;

    struct tm timeinfo = {};
    localtime_r(&now, &timeinfo);

    if (!force && (timeinfo.tm_sec == _lastDisplayedSecond) && (timeinfo.tm_min == _lastDisplayedMinute))
        return false;

    _display.showTimeWithMinuteIndicators(timeinfo);
    _lastDisplayedSecond = static_cast<int8_t>(timeinfo.tm_sec);
    _lastDisplayedMinute = static_cast<int8_t>(timeinfo.tm_min);
    return true;
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// /// @brief Check wakeup button press state
// void WordyWatch::checkWakeupButtonPress()
// {
//     // Disable hold on power control pin to allow changes
//     _powerAndSleep.disablePowerHold();

//     // Restore pull-up on wake pin after waking from sleep
//     if (_wakePinNum >= 0 && _wakePinPullup)
//     {
//         pinMode(_wakePinNum, INPUT);
//         gpio_pullup_en((gpio_num_t)_wakePinNum);
//         // Small delay to let pull-up stabilize
//         delayMicroseconds(100);
//     }

//     // Clear IMU interrupt (INT2 shares the wake pin)
//     _accelerometer.clearInterrupt();

//     // Check if wake button is pressed by reading GPIO directly
//     bool wakeButtonPressed = false;
//     if (_wakePinNum >= 0)
//     {
//         // Active LOW
//         wakeButtonPressed = (gpio_get_level((gpio_num_t)_wakePinNum) == 0);

// #ifdef DEBUG_BOOT_BUTTON_PRESS
//         LOG_I(MODULE_PREFIX, "Wake: btn=%s", wakeButtonPressed ? "PRESS" : "none");
// #endif
//     }

//     // Re-enable hold on power control pin
//     _powerAndSleep.enablePowerHold();

//     // If button is pressed, show time and stay awake
//     if (wakeButtonPressed)
//     {
//         // Get LED device and restart it
//         RaftDevice* pDevice = getDeviceByName("LEDPanel");
//         if (pDevice)
//         {
//             pDevice->sendCmdJSON("{\"cmd\":\"start\"}");
            
//             // Display current time
//             showTimeOnDisplay();
            
// #ifdef DEBUG_SLEEP_WAKEUP
//             LOG_I(MODULE_PREFIX, "handleWakeup showing time for %dms", _showTimeForMs);
// #endif
//         }
//     }
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Add REST API endpoints
/// @param endpointManager Manager for REST API endpoints
void WordyWatch::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    endpointManager.addEndpoint("settime",
        RestAPIEndpoint::ENDPOINT_CALLBACK,
        RestAPIEndpoint::ENDPOINT_GET,
        std::bind(&WordyWatch::apiSetTime, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        "settime/YYYY-MM-DDTHH:MM:SS - Set RTC time");

    endpointManager.addEndpoint("settings",
        RestAPIEndpoint::ENDPOINT_CALLBACK,
        RestAPIEndpoint::ENDPOINT_GET,
        std::bind(&WordyWatch::apiSettings, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        "settings?showsecs=10 - Set persistent display duration");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Get the device status as JSON
/// @return JSON string
String WordyWatch::getStatusJSON() const
{
    String json = "{\"vSense\":" + String(_powerAndSleep.getVSENSEReading()) +
        ",\"avgVSense\":" + String(_powerAndSleep.getVSENSEAverage()) +
        ",\"calculatedV\":" + String(_powerAndSleep.getBatteryVoltage(), 2) +
        ",\"powerButtonPressed\":" + String(_powerAndSleep.isPowerButtonPressed() ? "true" : "false") +
        "}";
    return json;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Get settings JSON
/// @return JSON string
String WordyWatch::getSettingsJSON() const
{
    uint32_t showSecs = _showTimeForMs / 1000;
    String json = "{\"showsecs\":" + String(showSecs) +
        ",\"showTimeForMs\":" + String(_showTimeForMs) +
        "}";
    return json;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Persist settings to NVS
bool WordyWatch::persistSettings()
{
    String json = getSettingsJSON();
    return _settingsNVS.setJsonDoc(json.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Initialize I2C master bus
/// @return True if successful
bool WordyWatch::initI2C()
{
    if ((_i2cSdaPin < 0) || (_i2cSclPin < 0))
    {
        LOG_W(MODULE_PREFIX, "initI2C I2C SDA or SCL pin not configured");
        return false;
    }

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

    // Add accelerometer to bus
    if (!_accelerometer.addToI2CBus(_i2cBusHandle, _i2cFreqHz))
    {
        LOG_E(MODULE_PREFIX, "initI2C failed to add accelerometer device");
        i2c_del_master_bus(_i2cBusHandle);
        _i2cBusHandle = nullptr;
        return false;
    }

    // Add RTC to bus
    if (!_rtc.addToI2CBus(_i2cBusHandle, _i2cFreqHz))
    {
        LOG_E(MODULE_PREFIX, "initI2C failed to add RTC device");
        i2c_del_master_bus(_i2cBusHandle);
        _i2cBusHandle = nullptr;
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief REST API endpoint to set time
RaftRetCode WordyWatch::apiSetTime(const String& reqStr, String& respStr, const APISourceInfo& /*sourceInfo*/)
{
    // Parse URL: settime/YYYY-MM-DDTHH:MM:SS
    std::vector<String> params;
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);

    if (params.size() < 2)
    {
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false, "missing time parameter");
    }

    // Parse ISO8601 format: YYYY-MM-DDTHH:MM:SS
    String timeStr = params[1];
    struct tm timeinfo = {};
    
    int year, month, day, hour, min, sec;
    if (sscanf(timeStr.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6)
    {
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = min;
        timeinfo.tm_sec = sec;
        timeinfo.tm_isdst = -1;
        
        // Calculate day of week
        mktime(&timeinfo);

        // Write to RTC
        if (_rtc.writeTime(&timeinfo))
        {
            // Update system time
            struct timeval tv;
            tv.tv_sec = mktime(&timeinfo);
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);

            LOG_I(MODULE_PREFIX, "apiSetTime: Time set to %04d-%02d-%02d %02d:%02d:%02d",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            
            return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
        }
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false, "RTC write failed");
    }
    
    return Raft::setJsonBoolResult(reqStr.c_str(), respStr, false, "invalid time format");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief REST API endpoint to configure settings
RaftRetCode WordyWatch::apiSettings(const String& reqStr, String& respStr, const APISourceInfo& /*sourceInfo*/)
{
    std::vector<String> params;
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);

    if (nameValues.empty())
    {
        return Raft::setJsonResult(reqStr.c_str(), respStr, true, "", getSettingsJSON().c_str());
    }

    RaftJson nameValuesJson = RaftJson::getJSONFromNVPairs(nameValues, true);
    int showSecs = nameValuesJson.getInt("showsecs", -1);
    if (showSecs < 0)
    {
        return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "badParams");
    }

    uint64_t showTimeMs = static_cast<uint64_t>(showSecs) * 1000ULL;
    if (showTimeMs > UINT32_MAX)
    {
        return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "showsecsTooLarge");
    }

    _showTimeForMs = static_cast<uint32_t>(showTimeMs);
    if (!persistSettings())
    {
        return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "persistFailed");
    }

    return Raft::setJsonResult(reqStr.c_str(), respStr, true, "", getSettingsJSON().c_str());
}
