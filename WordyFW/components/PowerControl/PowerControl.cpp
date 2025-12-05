/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Power control
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PowerControl.h"
#include "RaftJsonIF.h"
#include "ConfigPinMap.h"
#include "RaftUtils.h"
#include "esp_sleep.h"

// #define DEBUG_USER_BUTTON_PRESS
// #define DEBUG_POWER_CONTROL

namespace
{
    static constexpr const char* MODULE_PREFIX = "PowerControl";
}

/// @brief Constructor for PowerControl
/// @param pClassName Class name
/// @param pDevConfigJson Device configuration JSON
PowerControl::PowerControl(const char* pClassName, const char* pDevConfigJson)
    : RaftDevice(pClassName, pDevConfigJson)
{
}

/// @brief Destructor for PowerControl
PowerControl::~PowerControl()
{
}

/// @brief Setup the PowerControl device
void PowerControl::setup()
{
    _isConfigured = applyConfiguration();
    if (!_isConfigured)
    {
        LOG_E(MODULE_PREFIX, "setup configuration failed");
    }
}

/// @brief Main loop for the PowerControl device (called frequently)
void PowerControl::loop()
{
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
}

/// @brief Apply configuration
/// @return true if configuration applied successfully
bool PowerControl::applyConfiguration()
{
    // Get power control pin
    String pinName = deviceConfig.getString("powerCtrlPin", "");
    _powerCtrlPin = ConfigPinMap::getPinFromName(pinName.c_str());

    // Set power control pin to ensure power remains on
    if (_powerCtrlPin >= 0)
    {
        // Set power control pin
        pinMode(_powerCtrlPin, OUTPUT);
        digitalWrite(_powerCtrlPin, HIGH);
    }

    // Setup VSENSE pin
    pinName = deviceConfig.getString("vsensePin", "");
    _vsensePin = ConfigPinMap::getPinFromName(pinName.c_str());
    if (_vsensePin >= 0)
    {
        // Set VSENSE pin
        pinMode(_vsensePin, INPUT);
    }

    // Get battery low voltage
    _batteryLowV = deviceConfig.getDouble("batteryLowV", BATTERY_LOW_V_DEFAULT);

    // Get VSENSE button level
    _vsenseButtonLevel = deviceConfig.getLong("vsenseButtonLevel", VSENSE_BUTTON_LEVEL_DEFAULT);

    // Get button off time
    _buttonOffTimeMs = deviceConfig.getLong("buttonOffTimeMs", 2000);

    // Get ADC calibration
    _vsenseSlope = VSENSE_SLOPE_DEFAULT;
    _vsenseIntercept = VSENSE_INTERCEPT_DEFAULT;
    double v1 = deviceConfig.getDouble("adcCalib/v1", 0);
    int a1 = deviceConfig.getLong("adcCalib/a1", 0);
    double v2 = deviceConfig.getDouble("adcCalib/v2", 0);
    int a2 = deviceConfig.getLong("adcCalib/a2", 0);
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
float PowerControl::getVoltageFromADCReading(uint32_t adcReading) const
{
    // Convert to voltage
    return adcReading * _vsenseSlope + _vsenseIntercept;
}

/// @brief Handle shutdown
void PowerControl::shutdown()
{
    // Shutdown
    digitalWrite(_powerCtrlPin, LOW);
    delay(TIME_TO_HOLD_POWER_CTRL_PIN_LOW_MS);

    // Enter light sleep with no wakeup
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_light_sleep_start();
}

/// @brief Add REST API endpoints
/// @param endpointManager Manager for REST API endpoints
void PowerControl::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
}

/// @brief Get time of last device status update
/// @param includeElemOnlineStatusChanges Include element online status changes in the status update time
/// @param includePollDataUpdates Include poll data updates in the status update time
/// @return Time of last device status update in milliseconds
uint32_t PowerControl::getDeviceInfoTimestampMs(bool /*includeElemOnlineStatusChanges*/,
    bool includePollDataUpdates) const
{
    return 0;
}

/// @brief Get the device status as JSON
/// @return JSON string
String PowerControl::getStatusJSON() const
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

/// @brief Get the device status as binary
/// @return Binary status vector
std::vector<uint8_t> PowerControl::getStatusBinary() const
{
    return {};
}

/// @brief Get the device type record for this device so that it can be added to the device type records
/// @param devTypeRec (out) Device type record
/// @return true if the device has a device type record
bool PowerControl::getDeviceTypeRecord(DeviceTypeRecordDynamic& /*devTypeRec*/) const
{
    return false;
}
