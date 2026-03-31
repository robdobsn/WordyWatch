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
#include "DeviceManager.h"
#include "APISourceInfo.h"
#include "RestAPIEndpointManager.h"
#include "wordclock_patterns.h"
#include <time.h>
#include <sys/time.h>

// Debug control - uncomment to enable specific debugging
#define DEBUG_LOOP_STATE_MACHINE
#define DEBUG_TIME_DISPLAY

namespace
{
    static constexpr const char* MODULE_PREFIX = "WordyWatch";

    const led_pattern_t* getPatternForTime(int hour, int minute)
    {
        if (hour < 0 || minute < 0)
            return nullptr;

        hour = hour % 24;
        minute = (minute / LED_MINUTE_GRANULARITY) * LED_MINUTE_GRANULARITY;
        if (minute >= 60)
            minute = 0;

        for (const auto& pattern : led_patterns)
        {
            if (pattern.hour == hour && pattern.minute == minute)
            {
                return &pattern;
            }
        }

        return nullptr;
    }
}

#ifdef DEBUG_WRIST_TILT_INT
// Static members for ISR access
volatile uint32_t WordyWatch::_wristTiltIntCount = 0;
volatile int64_t WordyWatch::_wristTiltIntLastIsrTimeUs = 0;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor for WordyWatch
/// @param pModuleName Module name
/// @param sysConfig System configuration interface
WordyWatch::WordyWatch(const char *pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig)
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

    // Get VSENSE button level
    uint32_t vsenseButtonLevel = config.getLong("vsenseButtonLevel", 2300);

    // Get button off time
    uint32_t buttonOffTimeMs = config.getLong("buttonOffTimeMs", 2000);

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

    // Get wake pin configuration
    pinName = config.getString("wakePinNum", "");
    int wakePinNum = ConfigPinMap::getPinFromName(pinName.c_str());
    bool wakePinPullup = config.getBool("wakePinPullup", false);

    // Configure power management
    _powerAndSleep.configure(powerCtrlPin, strapCtrlPin, vsensePin,
                     vsenseSlope, vsenseIntercept,
                     batteryLowV, vsenseButtonLevel, buttonOffTimeMs,
                     wakePinNum, wakePinPullup);

    // Get sleep configuration
    _showTimeForMs = config.getLong("showTimeForMs", 10000);

    // Get minute resolution
    _minuteResolution = config.getLong("minuteResolution", 5);

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
    // Install GPIO ISR to count wrist tilt interrupts on wake pin
    setupWristTiltInterrupt(wakePinNum);
#endif

    // Debug - do initial power reading
    bool isShutdownRequired = _powerAndSleep.update();
    LOG_I(MODULE_PREFIX, "setup powerCtrlPin %d strapCtrlPin %d vSensePin %d currentADC %d currentVoltage %.2fV batteryLowV %.2f isShutdownRequired %d vSenseButtonLevel %d buttonOffTime %dms", 
                powerCtrlPin, strapCtrlPin, vsensePin, 
                (int)_powerAndSleep.getVSENSEReading(), 
                _powerAndSleep.getBatteryVoltage(), 
                batteryLowV, isShutdownRequired, vsenseButtonLevel, buttonOffTimeMs);
    
    // Initialize time last woken
    _timeLastWokenMs = millis();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Main loop for the WordyWatch device (called frequently)
void WordyWatch::loop()
{
#ifdef DEBUG_WRIST_TILT_INT
    // Periodically log wrist tilt interrupt count
    logWristTiltInterrupts();
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
            showTimeOnDisplay();
            setState(DISPLAYING_TIME);
            return;
        case DISPLAYING_TIME:
            // Displaying time
            if (Raft::isTimeout(millis(), _currentStateStartMs, _showTimeForMs))
            {
#ifdef DEBUG_LOOP_STATE_MACHINE
                LOG_I(MODULE_PREFIX, "loop time display duration expired, going to sleep");
#endif
                setState(PREPARING_TO_SLEEP);
                return;
            }
            break;

        case PREPARING_TO_SLEEP:
            clearDisplay();
            setState(SLEEPING);
            return;

        case SLEEPING:
            _powerAndSleep.enterDeepSleep(-1);

            // Note that we will not return from deep sleep as it will reset the CPU
            setState(WOKEN_UP);
            return;

        case WOKEN_UP:
            // Check wakeup reason
            checkWakeupReason();
            _timeLastWokenMs = millis();
            setState(DISPLAYING_TIME);
            return;
        case SHUTDOWN_REQUESTED:
            // Clear the display
            clearDisplay();
            _powerAndSleep.shutdown();
            setState(SHUTTING_DOWN);
            return;
        case SHUTTING_DOWN:
            // Do nothing - waiting for shutdown            
            return;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Check reason for wakeup from sleep
void WordyWatch::checkWakeupReason()
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
            // EXT1 wakeup on GPIO 5 (shared line) - determine which device triggered it
            wakeupSource = "EXT1 GPIO";

            // Check if the button is pressed (VSENSE above threshold)
            bool buttonPressed = _powerAndSleep.isPowerButtonPressed();

            // Check if the IMU flagged a tilt event
            bool tiltDetected = false;
            _accelerometer.readTiltStatus(tiltDetected);

            // Check RTC status for alarm/timer flags
            uint8_t rtcStatus = 0;
            _rtc.readStatusRegister(rtcStatus);

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

#ifdef DEBUG_WRIST_TILT_INT
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief ISR for wrist tilt interrupt (falling edge on shared INT line)
void IRAM_ATTR WordyWatch::wristTiltISR(void* arg)
{
    _wristTiltIntCount = _wristTiltIntCount + 1;
    _wristTiltIntLastIsrTimeUs = esp_timer_get_time();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Setup GPIO ISR on wake pin to count wrist tilt interrupts
/// @param pinNum GPIO pin number for the interrupt
void WordyWatch::setupWristTiltInterrupt(int pinNum)
{
    if (pinNum < 0)
    {
        LOG_W(MODULE_PREFIX, "setupWristTiltInterrupt: no wake pin configured");
        return;
    }

    _wristTiltIntPin = pinNum;
    _wristTiltIntCount = 0;

    // Configure GPIO for falling edge interrupt (active-low, open-drain with pull-up)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pinNum);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "setupWristTiltInterrupt: gpio_config failed: %s", esp_err_to_name(err));
        return;
    }

    // Install ISR service and add handler
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        LOG_E(MODULE_PREFIX, "setupWristTiltInterrupt: gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return;
    }

    err = gpio_isr_handler_add((gpio_num_t)pinNum, wristTiltISR, nullptr);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "setupWristTiltInterrupt: gpio_isr_handler_add failed: %s", esp_err_to_name(err));
        return;
    }

    LOG_I(MODULE_PREFIX, "setupWristTiltInterrupt: ISR installed on GPIO %d (falling edge)", pinNum);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Log wrist tilt interrupt count periodically
void WordyWatch::logWristTiltInterrupts()
{
    if (!Raft::isTimeout(millis(), _wristTiltIntLastLogMs, WRIST_TILT_INT_LOG_INTERVAL_MS))
        return;
    _wristTiltIntLastLogMs = millis();

    uint32_t count = _wristTiltIntCount;
    uint32_t delta = count - _wristTiltIntLastLoggedCount;
    _wristTiltIntLastLoggedCount = count;

    // Read GPIO level BEFORE any I2C reads
    int pinBefore = _wristTiltIntPin >= 0 ? gpio_get_level((gpio_num_t)_wristTiltIntPin) : -1;

    // Calculate time from last diagnostic read to ISR
    int64_t isrTimeUs = _wristTiltIntLastIsrTimeUs;
    int64_t timeSinceLastDiagUs = isrTimeUs - _wristTiltIntLastDiagTimeUs;

    LOG_I(MODULE_PREFIX, "wristTiltInt: count=%lu delta=%lu pinBefore=%d isrAfterDiag=%lldus",
          (unsigned long)count, (unsigned long)delta, pinBefore, timeSinceLastDiagUs);

    // Poll accelerometer status registers for diagnostics (this clears IMU latch)
    _accelerometer.debugPollStatus();

    // Read GPIO level AFTER IMU register reads (latch should be released if IMU was source)
    int pinAfterImu = _wristTiltIntPin >= 0 ? gpio_get_level((gpio_num_t)_wristTiltIntPin) : -1;

    // Read RTC status register to check if RTC is asserting INT#
    uint8_t rtcStatus = 0xFF;
    _rtc.readStatusRegister(rtcStatus);

    // Read GPIO level AFTER RTC status read
    int pinAfterRtc = _wristTiltIntPin >= 0 ? gpio_get_level((gpio_num_t)_wristTiltIntPin) : -1;

    LOG_I(MODULE_PREFIX, "wristTiltInt: pinAfterImu=%d pinAfterRtc=%d rtcStatus=0x%02x",
          pinAfterImu, pinAfterRtc, rtcStatus);

    // Record when we finished diagnostic reads (for next ISR timing comparison)
    _wristTiltIntLastDiagTimeUs = esp_timer_get_time();
}
#endif

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
/// @brief Show time display on LED panel
void WordyWatch::showTimeOnDisplay()
{
    // Get time from RTC (falling back to system time if RTC read fails)
    struct tm timeinfo = {};
    if (_rtc.getTime(&timeinfo))
    {
#ifdef DEBUG_TIME_DISPLAY
        LOG_I(MODULE_PREFIX, "showTimeOnDisplay showing time %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
#endif
        // Get LED device and display time
        RaftDevice* pDevice = getDeviceByName("LEDPanel");
        if (pDevice)
        {
            const led_pattern_t* pattern = getPatternForTime(timeinfo.tm_hour, timeinfo.tm_min);
            if (!pattern)
            {
                LOG_W(MODULE_PREFIX, "showTimeOnDisplay pattern not found for %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                return;
            }

            pDevice->sendCmdJSON("{\"cmd\":\"start\"}");

            String jsonCmd = "{\"cmd\":\"blitMask\",\"width\":" + String(LED_GRID_WIDTH) +
                ",\"height\":" + String(LED_GRID_HEIGHT) +
                ",\"clear\":1,\"words\":[";
            for (uint16_t idx = 0; idx < LED_MASK_WORDS; idx++)
            {
                jsonCmd += String(pattern->led_mask[idx]);
                if (idx + 1 < LED_MASK_WORDS)
                    jsonCmd += ",";
            }
            jsonCmd += "]}";
            pDevice->sendCmdJSON(jsonCmd.c_str());
        }
    }
#ifdef DEBUG_TIME_DISPLAY
    else
    {
        LOG_E(MODULE_PREFIX, "showTimeOnDisplay failed to get time");
    }
#endif
}

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
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Get the device status as JSON
/// @return JSON string
String WordyWatch::getStatusJSON() const
{
    String json = "{\"vSense\":" + String(_powerAndSleep.getVSENSEReading()) +
        ",\"avgVSense\":" + String(_powerAndSleep.getVSENSEAverage()) +
        ",\"calculatedV\":" + String(_powerAndSleep.getBatteryVoltage(), 2) +
        ",\"buttonPressed\":" + String(_powerAndSleep.isPowerButtonPressed() ? "true" : "false") +
        "}";
    return json;
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
/// @brief Clear the display
void WordyWatch::clearDisplay()
{
#ifdef DEBUG_TIME_DISPLAY
    LOG_I(MODULE_PREFIX, "clearDisplay stopping LED panel");
#endif

    // Get LED device and stop it
    RaftDevice* pDevice = getDeviceByName("LEDPanel");
    if (pDevice)
        pDevice->sendCmdJSON("{\"cmd\":\"stop\"}");
}
