////////////////////////////////////////////////////////////////////////////////
//
// LEDCharliePanel.h
//
// Charlieplexed LED panel driver with precomputed GPIO masks and timer refresh
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "RaftArduino.h"

class LEDCharliePanel
{
public:
    LEDCharliePanel();
    ~LEDCharliePanel();

    bool configure(const std::vector<int>& pins, uint16_t width, uint16_t height, uint32_t refreshHz);
    bool start();
    void stop();

    bool setPixel(uint16_t x, uint16_t y, bool on);
    bool getPixel(uint16_t x, uint16_t y) const;
    void clear();
    void fill(bool on);
    void setAll(const std::vector<uint8_t>& frameBits);

    uint16_t getWidth() const { return _width; }
    uint16_t getHeight() const { return _height; }
    uint32_t getRefreshHz() const { return _refreshHz; }
    size_t getLEDCount() const { return _numLEDs; }
    size_t getLitCount() const;
    bool isConfigured() const { return _isConfigured; }
    bool isRunning() const { return _isRunning; }

private:
    struct LedMaskEntry
    {
        uint32_t hiMask = 0;
        uint32_t loMask = 0;
        uint32_t enableMask = 0;
    };

    static bool onTimerAlarm(gptimer_handle_t timer,
        const gptimer_alarm_event_data_t* event_data,
        void* user_ctx);

    void driveNextFromISR();
    void blankAllPins() const;

    void destroyTimer();
    bool configureTimer();
    void resetPinsToInputs() const;

    std::vector<int> _pins;
    std::vector<LedMaskEntry> _ledMasks;
    std::vector<uint8_t> _framebuffer;

    volatile uint8_t* _framebufferRaw = nullptr;
    LedMaskEntry* _maskRaw = nullptr;

    uint32_t _pinMaskAll = 0;
    uint32_t _ticksPerSlot = 0;
    uint32_t _timerResolutionHz = 1000000;
    uint16_t _width = 0;
    uint16_t _height = 0;
    uint32_t _refreshHz = 0;
    size_t _numLEDs = 0;

    gptimer_handle_t _timer = nullptr;
    volatile size_t _scanIndex = 0;
    volatile bool _isRunning = false;
    bool _isConfigured = false;

    portMUX_TYPE _fbLock;
};
