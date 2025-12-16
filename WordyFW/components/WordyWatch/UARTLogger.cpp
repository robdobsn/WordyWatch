/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UART Logger
// Simple UART TX-only logger for debugging
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "UARTLogger.h"
#include <string.h>
#include <stdarg.h>
#include <driver/gpio.h>

UARTLogger::UARTLogger()
{
}

UARTLogger::~UARTLogger()
{
    if (_initialized)
    {
        uart_driver_delete(_uartNum);
    }
}

bool UARTLogger::begin(int txPin, uint32_t baudRate)
{
    if (txPin < 0)
        return false;

    _txPin = txPin;

    // Configure UART parameters
    uart_config_t uart_config = {};
    uart_config.baud_rate = (int)baudRate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    // Install UART driver (TX only, minimal RX buffer for driver compatibility)
    esp_err_t err = uart_driver_install(_uartNum, 256, 256, 0, NULL, 0);
    if (err != ESP_OK)
        return false;

    // Configure UART parameters
    err = uart_param_config(_uartNum, &uart_config);
    if (err != ESP_OK)
    {
        uart_driver_delete(_uartNum);
        return false;
    }

    // Set UART pins (TX only, RX disabled)
    err = uart_set_pin(_uartNum, _txPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        uart_driver_delete(_uartNum);
        return false;
    }

    _initialized = true;
    return true;
}

void UARTLogger::write(const char* str)
{
    if (!_initialized || !str)
        return;

    uart_write_bytes(_uartNum, str, strlen(str));
}

void UARTLogger::printf(const char* format, ...)
{
    if (!_initialized || !format)
        return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    write(buffer);
}
