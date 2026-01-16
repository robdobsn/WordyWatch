////////////////////////////////////////////////////////////////////////////////
//
// DeviceLEDCharlie.h
//
// RaftDevice implementation for a charlieplexed LED panel driven by LEDCharliePanel
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftDevice.h"
#include "LEDCharliePanel.h"

class RestAPIEndpointManager;
class APISourceInfo;
class DeviceTypeRecordDynamic;

class DeviceLEDCharlie : public RaftDevice
{
public:
    DeviceLEDCharlie(const char* pClassName, const char* pDevConfigJson);
    virtual ~DeviceLEDCharlie() override;

    static RaftDevice* create(const char* pClassName, const char* pDevConfigJson)
    {
        return new DeviceLEDCharlie(pClassName, pDevConfigJson);
    }

    void setup() override final;
    void loop() override final;
    void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;

    uint32_t getDeviceInfoTimestampMs(bool includeElemOnlineStatusChanges,
        bool includePollDataUpdates) const override final;
    String getStatusJSON() const override final;
    std::vector<uint8_t> getStatusBinary() const override final;
    bool getDeviceTypeRecord(DeviceTypeRecordDynamic& devTypeRec) const override final;

    /// @brief Send a JSON command to the device
    /// @param jsonCmd JSON command
    /// @return RaftRetCode
    virtual RaftRetCode sendCmdJSON(const char* jsonCmd) override final;

private:
    // Display time on LED panel
    void displayTime(int hour, int minute);

    // Handle REST API control commands
    RaftRetCode apiControl(const String& reqStr, String& respStr, const APISourceInfo& sourceInfo);
    bool applyConfiguration();

    LEDCharliePanel _panel;
    bool _isConfigured = false;
    bool _autostart = true;
    uint32_t _lastMutateMs = 0;

    uint32_t _debugLastMs = 0;
};
