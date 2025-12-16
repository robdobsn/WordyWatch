/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UART Logger
// Simple UART TX-only logger for debugging
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <driver/uart.h>

class UARTLogger
{
public:
    UARTLogger();
    ~UARTLogger();

    /// @brief Initialize UART logger
    /// @param txPin GPIO pin for TX
    /// @param baudRate Baud rate (default 115200)
    /// @return true if successful
    bool begin(int txPin, uint32_t baudRate = 115200);

    /// @brief Write string to UART
    /// @param str String to write
    void write(const char* str);

    /// @brief Write formatted string to UART (printf style)
    /// @param format Format string
    /// @param ... Arguments
    void printf(const char* format, ...);

    /// @brief Check if UART is initialized
    /// @return true if initialized
    bool isInitialized() const { return _initialized; }

private:
    bool _initialized = false;
    uart_port_t _uartNum = UART_NUM_0;
    int _txPin = -1;
};
