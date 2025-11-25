////////////////////////////////////////////////////////////////////////////////
//
// DeviceLEDCharlie.cpp
//
////////////////////////////////////////////////////////////////////////////////

#include "DeviceLEDCharlie.h"

#include <algorithm>
#include <functional>
#include <vector>
#include "RaftUtils.h"
#include "RaftJson.h"
#include "RestAPIEndpointManager.h"
#include "RestAPIEndpoint.h"
#include "DeviceTypeRecordDynamic.h"
#include "APISourceInfo.h"
#include "wordclock_patterns.h"

namespace
{
    static constexpr const char* MODULE_PREFIX = "DeviceLEDCharlie";
}

DeviceLEDCharlie::DeviceLEDCharlie(const char* pClassName, const char* pDevConfigJson)
    : RaftDevice(pClassName, pDevConfigJson)
{
}

DeviceLEDCharlie::~DeviceLEDCharlie()
{
    _panel.stop();
}

void DeviceLEDCharlie::setup()
{
    _configured = applyConfiguration();
    if (!_configured)
    {
        LOG_E(MODULE_PREFIX, "setup configuration failed");
        return;
    }

    if (_autostart)
    {
        if (!_panel.start())
        {
            LOG_E(MODULE_PREFIX, "setup failed to start panel");
        }
    }
}

void DeviceLEDCharlie::loop()
{
    if (!_configured)
        return;

    if (Raft::isTimeout(millis(), _debugLastMs, 1000))
    {
        _debugLastMs = millis();
        LOG_I(MODULE_PREFIX, "loop: running=%d, timerCount=%u", _panel.isRunning() ? 1 : 0, _panel.getTimerCount());
    }

    // if (_autostart && !_panel.isRunning())
    // {
    //     // Attempt restart if timer stopped unexpectedly
    //     _panel.start();
    // }
}

void DeviceLEDCharlie::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    endpointManager.addEndpoint("leds",
        RestAPIEndpoint::ENDPOINT_CALLBACK,
        RestAPIEndpoint::ENDPOINT_GET,
        std::bind(&DeviceLEDCharlie::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        "leds - status; leds/set?x=0&y=0&on=1; leds/clear; leds/fill?on=0");
}

uint32_t DeviceLEDCharlie::getDeviceInfoTimestampMs(bool /*includeElemOnlineStatusChanges*/,
    bool includePollDataUpdates) const
{
    if (includePollDataUpdates)
        return _lastMutateMs;
    return 0;
}

String DeviceLEDCharlie::getStatusJSON() const
{
    String json = "{\"width\":" + String(_panel.getWidth()) +
        ",\"height\":" + String(_panel.getHeight()) +
        ",\"refreshHz\":" + String(_panel.getRefreshHz()) +
        ",\"lit\":" + String(_panel.getLitCount()) +
        ",\"configured\":" + String(_configured ? 1 : 0) +
        ",\"running\":" + String(_panel.isRunning() ? 1 : 0) +
        "}";
    return json;
}

std::vector<uint8_t> DeviceLEDCharlie::getStatusBinary() const
{
    return {};
}

bool DeviceLEDCharlie::getDeviceTypeRecord(DeviceTypeRecordDynamic& /*devTypeRec*/) const
{
    return false;
}

RaftRetCode DeviceLEDCharlie::apiControl(const String& reqStr, String& respStr, const APISourceInfo& /*sourceInfo*/)
{
    std::vector<String> params;
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    RaftJson nameValuesJson = RaftJson::getJSONFromNVPairs(nameValues, true);

    if (params.size() < 2)
    {
        return Raft::setJsonResult(reqStr.c_str(), respStr, true, "", getStatusJSON().c_str());
    }

    String command = params[1];

    if (command.equalsIgnoreCase("set"))
    {
        int x = nameValuesJson.getInt("x", -1);
        int y = nameValuesJson.getInt("y", -1);
        int on = nameValuesJson.getInt("on", 1);
        if (x < 0 || y < 0)
            return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "badParams");
        bool ok = _panel.setPixel(static_cast<uint16_t>(x), static_cast<uint16_t>(y), on != 0);
        if (!ok)
            return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "outOfRange");
        _lastMutateMs = millis();
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }
    else if (command.equalsIgnoreCase("clear"))
    {
        _panel.clear();
        _lastMutateMs = millis();
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }
    else if (command.equalsIgnoreCase("fill"))
    {
        int on = nameValuesJson.getInt("on", 1);
        _panel.fill(on != 0);
        _lastMutateMs = millis();
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }
    else if (command.equalsIgnoreCase("status"))
    {
        return Raft::setJsonResult(reqStr.c_str(), respStr, true, "", getStatusJSON().c_str());
    }
    else if (command.equalsIgnoreCase("testAllLEDs"))
    {
        _panel.testAllLEDs();
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }
    else if (command.equalsIgnoreCase("testtime"))
    {
        // Get time from parameters
        int hour = nameValuesJson.getInt("hour", 0);
        int minute = nameValuesJson.getInt("minute", 0);
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
        {
            return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "badParams");
        }

        // Round to minute granularity
        minute = (minute / LED_MINUTE_GRANULARITY) * LED_MINUTE_GRANULARITY;
        
        // Look up LED pattern for time
        const led_pattern_t* pattern = nullptr;
        for (const auto& p : led_patterns)
        {
            if (p.hour == hour && p.minute == minute)
            {
                pattern = &p;
                break;
            }
        }
        if (!pattern)
        {
            return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "patternNotFound");
        }
        // Apply LED pattern
        _panel.clear();
        uint32_t row = 0;
        uint32_t col = 0;
        for (uint16_t maskWordIdx = 0; maskWordIdx < LED_MASK_WORDS; maskWordIdx++)
        {
            uint32_t mask = pattern->led_mask[maskWordIdx];
            for (uint16_t maskBitIdx = 0; maskBitIdx < 32; maskBitIdx++)
            {
                if (mask & (1UL << maskBitIdx))
                {
                    _panel.setPixel(col, row, true);
                }
                col++;
                if (col >= LED_GRID_WIDTH)
                {
                    col = 0;
                    row++;
                    if (row >= LED_GRID_HEIGHT)
                    {
                        break;
                    }
                }
            }
        }
        _lastMutateMs = millis();
        String json = "{\"hour\":" + String(pattern->hour) +
            ",\"minute\":" + String(pattern->minute) + "}";
        return Raft::setJsonResult(reqStr.c_str(), respStr, true, "", json.c_str());
    }

    return Raft::setJsonErrorResult(reqStr.c_str(), respStr, "unknownCommand");
}

bool DeviceLEDCharlie::applyConfiguration()
{
    uint16_t width = static_cast<uint16_t>(deviceConfig.getInt("width", 0));
    uint16_t height = static_cast<uint16_t>(deviceConfig.getInt("height", 0));
    uint32_t refreshHz = static_cast<uint32_t>(deviceConfig.getInt("refreshHz", 800));
    bool originFlip = deviceConfig.getBool("originFlip", false);
    _autostart = deviceConfig.getBool("autoStart", true);

    std::vector<int> pinList;
    deviceConfig.getArrayInts("pins", pinList);
    if (pinList.empty())
    {
        LOG_E(MODULE_PREFIX, "config requires pins array");
        return false;
    }

    bool ok = _panel.configure(pinList, width, height, refreshHz, originFlip);
    if (!ok)
        return false;

    return true;
}

