////////////////////////////////////////////////////////////////////////////////
//
// LEDCharliePanel.cpp
//
// Implements a charlieplexed LED panel using precomputed GPIO masks and a GPTimer
// driven refresh ISR. Assumes all GPIOs reside on the same port (0..31) for
// optimal register writes.
//
////////////////////////////////////////////////////////////////////////////////

#include "LEDCharliePanel.h"

#include <algorithm>
#include <cstring>
#include "driver/gpio.h"
#include "esp_err.h"
#include "RaftUtils.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"

namespace
{
    static constexpr const char* MODULE_PREFIX = "LEDCharliePanel";
}

LEDCharliePanel::LEDCharliePanel()
    : _fbLock(portMUX_INITIALIZER_UNLOCKED)
{
}

LEDCharliePanel::~LEDCharliePanel()
{
    stop();
}

bool LEDCharliePanel::configure(const std::vector<int>& pins,
    uint16_t width,
    uint16_t height,
    uint32_t refreshHz)
{
    stop();

    if (pins.size() < 2)
    {
        LOG_E(MODULE_PREFIX, "configure insufficient pins %u", pins.size());
        return false;
    }
    if (width == 0 || height == 0)
    {
        LOG_E(MODULE_PREFIX, "configure invalid geometry %u x %u", width, height);
        return false;
    }
    if (width > pins.size())
    {
        LOG_E(MODULE_PREFIX, "configure width %u exceeds pins %u", width, pins.size());
        return false;
    }
    if (height > pins.size() - 1)
    {
        LOG_E(MODULE_PREFIX, "configure height %u exceeds pins-1", height);
        return false;
    }

    std::vector<int> uniquePins = pins;
    std::sort(uniquePins.begin(), uniquePins.end());
    auto dupIt = std::adjacent_find(uniquePins.begin(), uniquePins.end());
    if (dupIt != uniquePins.end())
    {
        LOG_E(MODULE_PREFIX, "configure duplicate pin %d", *dupIt);
        return false;
    }
    for (int pin : pins)
    {
        if (pin < 0 || pin > 31)
        {
            LOG_E(MODULE_PREFIX, "configure pin %d out of range for port 0", pin);
            return false;
        }
    }

    _pins = pins;
    _width = width;
    _height = height;
    _refreshHz = refreshHz > 0 ? refreshHz : 500;
    _numLEDs = static_cast<size_t>(_width) * _height;
    if (_numLEDs == 0)
    {
        LOG_E(MODULE_PREFIX, "configure zero LEDs");
        return false;
    }

    _pinMaskAll = 0;
    for (int pin : _pins)
        _pinMaskAll |= (1u << pin);

    _ledMasks.clear();
    _ledMasks.resize(_numLEDs);
    _framebuffer.assign(_numLEDs, 0);
    _framebufferRaw = _framebuffer.data();
    _maskRaw = _ledMasks.data();

    for (uint16_t col = 0; col < _width; ++col)
    {
        std::vector<size_t> otherIndices;
        otherIndices.reserve(_pins.size() - 1);
        for (size_t idx = 0; idx < _pins.size(); ++idx)
        {
            if (idx == col)
                continue;
            otherIndices.push_back(idx);
        }
        if (otherIndices.size() < _height)
        {
            LOG_E(MODULE_PREFIX, "configure column %u insufficient anodes", col);
            return false;
        }

        for (uint16_t row = 0; row < _height; ++row)
        {
            size_t ledIdx = static_cast<size_t>(col) * _height + row;
            size_t hiPinIdx = otherIndices[row];
            int hiGpio = _pins[hiPinIdx];
            int loGpio = _pins[col];
            uint32_t hiMask = (1u << hiGpio);
            uint32_t loMask = (1u << loGpio);
            _ledMasks[ledIdx].hiMask = hiMask;
            _ledMasks[ledIdx].loMask = loMask;
            _ledMasks[ledIdx].enableMask = hiMask | loMask;
        }
    }

    // Prepare GPIOs: default to inputs and cleared outputs
    resetPinsToInputs();
    REG_WRITE(GPIO_OUT_W1TC_REG, _pinMaskAll);

    uint64_t denom = static_cast<uint64_t>(_refreshHz) * static_cast<uint64_t>(_numLEDs);
    if (denom == 0)
    {
        LOG_E(MODULE_PREFIX, "configure denom zero for refresh");
        return false;
    }
    uint64_t ticks = (_timerResolutionHz + denom - 1) / denom;
    if (ticks == 0)
        ticks = 1;
    _ticksPerSlot = static_cast<uint32_t>(ticks);

    _scanIndex = 0;
    _isConfigured = true;
    _isRunning = false;

    LOG_I(MODULE_PREFIX, "configured width %u height %u pins %u refresh %uHz slotTicks %u", 
        _width, _height, _pins.size(), _refreshHz, _ticksPerSlot);

    return true;
}

bool LEDCharliePanel::start()
{
    if (!_isConfigured)
    {
        LOG_E(MODULE_PREFIX, "start called before configure");
        return false;
    }
    if (_isRunning)
        return true;

    if (!configureTimer())
        return false;

    _isRunning = true;
    return true;
}

void LEDCharliePanel::stop()
{
    if (_isRunning && _timer)
    {
        gptimer_stop(_timer);
    }
    if (_timer)
    {
        gptimer_disable(_timer);
        gptimer_del_timer(_timer);
        _timer = nullptr;
    }
    _isRunning = false;
    blankAllPins();
}

bool LEDCharliePanel::setPixel(uint16_t x, uint16_t y, bool on)
{
    if (x >= _width || y >= _height || !_framebufferRaw)
        return false;
    size_t idx = static_cast<size_t>(x) * _height + y;
    taskENTER_CRITICAL(&_fbLock);
    _framebuffer[idx] = on ? 1 : 0;
    taskEXIT_CRITICAL(&_fbLock);
    return true;
}

bool LEDCharliePanel::getPixel(uint16_t x, uint16_t y) const
{
    if (x >= _width || y >= _height || !_framebufferRaw)
        return false;
    size_t idx = static_cast<size_t>(x) * _height + y;
    return _framebufferRaw[idx] != 0;
}

void LEDCharliePanel::clear()
{
    if (!_framebufferRaw)
        return;
    taskENTER_CRITICAL(&_fbLock);
    std::fill(_framebuffer.begin(), _framebuffer.end(), 0);
    taskEXIT_CRITICAL(&_fbLock);
}

void LEDCharliePanel::fill(bool on)
{
    if (!_framebufferRaw)
        return;
    taskENTER_CRITICAL(&_fbLock);
    std::fill(_framebuffer.begin(), _framebuffer.end(), on ? 1 : 0);
    taskEXIT_CRITICAL(&_fbLock);
}

void LEDCharliePanel::setAll(const std::vector<uint8_t>& frameBits)
{
    if (!_framebufferRaw || frameBits.size() < _framebuffer.size())
        return;
    taskENTER_CRITICAL(&_fbLock);
    std::memcpy(_framebuffer.data(), frameBits.data(), _framebuffer.size());
    taskEXIT_CRITICAL(&_fbLock);
}

size_t LEDCharliePanel::getLitCount() const
{
    if (!_framebufferRaw)
        return 0;
    size_t lit = 0;
    for (size_t i = 0; i < _numLEDs; ++i)
        lit += (_framebufferRaw[i] != 0) ? 1 : 0;
    return lit;
}

bool LEDCharliePanel::configureTimer()
{
    destroyTimer();

    gptimer_config_t config = {};
    config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    config.direction = GPTIMER_COUNT_UP;
    config.resolution_hz = _timerResolutionHz;

    esp_err_t err = gptimer_new_timer(&config, &_timer);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "gptimer_new_timer failed %d", err);
        _timer = nullptr;
        return false;
    }

    gptimer_event_callbacks_t cbs = {};
    cbs.on_alarm = &LEDCharliePanel::onTimerAlarm;
    err = gptimer_register_event_callbacks(_timer, &cbs, this);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "gptimer_register_event_callbacks failed %d", err);
        destroyTimer();
        return false;
    }

    gptimer_alarm_config_t alarm = {};
    alarm.alarm_count = _ticksPerSlot;
    alarm.reload_count = 0;
    alarm.flags.auto_reload_on_alarm = true;

    err = gptimer_set_alarm_action(_timer, &alarm);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "gptimer_set_alarm_action failed %d", err);
        destroyTimer();
        return false;
    }

    err = gptimer_enable(_timer);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "gptimer_enable failed %d", err);
        destroyTimer();
        return false;
    }

    err = gptimer_start(_timer);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "gptimer_start failed %d", err);
        destroyTimer();
        return false;
    }

    return true;
}

void LEDCharliePanel::destroyTimer()
{
    if (_timer)
    {
        gptimer_stop(_timer);
        gptimer_disable(_timer);
        gptimer_del_timer(_timer);
        _timer = nullptr;
    }
}

void LEDCharliePanel::resetPinsToInputs() const
{
    for (int pin : _pins)
    {
        gpio_reset_pin(static_cast<gpio_num_t>(pin));
        gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_INPUT);
        gpio_set_pull_mode(static_cast<gpio_num_t>(pin), GPIO_FLOATING);
    }
}

void IRAM_ATTR LEDCharliePanel::blankAllPins() const
{
    if (_pinMaskAll == 0)
        return;
    REG_WRITE(GPIO_ENABLE_W1TC_REG, _pinMaskAll);
    REG_WRITE(GPIO_OUT_W1TC_REG, _pinMaskAll);
}

bool IRAM_ATTR LEDCharliePanel::onTimerAlarm(gptimer_handle_t /*timer*/,
    const gptimer_alarm_event_data_t* /*event_data*/,
    void* user_ctx)
{
    auto* panel = static_cast<LEDCharliePanel*>(user_ctx);
    if (panel)
        panel->driveNextFromISR();
    return false;
}

void IRAM_ATTR LEDCharliePanel::driveNextFromISR()
{
    if (!_isConfigured || !_maskRaw || !_framebufferRaw)
    {
        blankAllPins();
        return;
    }

    blankAllPins();

    if (_numLEDs == 0)
        return;

    size_t idx = _scanIndex;
    size_t iterations = 0;
    while (iterations < _numLEDs)
    {
        if (idx >= _numLEDs)
            idx = 0;
        if (_framebufferRaw[idx])
        {
            const LedMaskEntry& entry = _maskRaw[idx];
            REG_WRITE(GPIO_OUT_W1TS_REG, entry.hiMask);
            REG_WRITE(GPIO_ENABLE_W1TS_REG, entry.enableMask);
            _scanIndex = idx + 1;
            if (_scanIndex >= _numLEDs)
                _scanIndex = 0;
            return;
        }
        idx++;
        iterations++;
    }

    _scanIndex = (_scanIndex + 1) % _numLEDs;
}

