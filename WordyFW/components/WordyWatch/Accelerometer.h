/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Accelerometer - LSM6DS accelerometer with wrist tilt detection
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Accelerometer
{
public:
    /// @brief Constructor
    /// @param i2cAddr I2C address of the accelerometer (default 0x6a for LSM6DS)
    explicit Accelerometer(uint8_t i2cAddr = 0x6a)
        : _i2cAddr(i2cAddr)
        , _devHandle(nullptr)
    {
    }

    /// @brief Destructor
    ~Accelerometer()
    {
        deinit();
    }

    /// @brief Get I2C address
    /// @return I2C address
    uint8_t getI2CAddress() const
    {
        return _i2cAddr;
    }

    /// @brief Set I2C address
    /// @param addr New I2C address
    void setI2CAddress(uint8_t addr)
    {
        _i2cAddr = addr;
    }

    /// @brief Add accelerometer device to I2C bus
    /// @param busHandle I2C bus handle
    /// @param sclSpeedHz SCL speed in Hz
    /// @return True if successful
    bool addToI2CBus(i2c_master_bus_handle_t busHandle, uint32_t sclSpeedHz)
    {
        if (!busHandle)
        {
            ESP_LOGE("Accelerometer", "addToI2CBus: Invalid bus handle");
            return false;
        }

        // Configure accelerometer device
        i2c_device_config_t dev_config = {};
        dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_config.device_address = _i2cAddr;
        dev_config.scl_speed_hz = sclSpeedHz;

        // Add device to bus
        esp_err_t err = i2c_master_bus_add_device(busHandle, &dev_config, &_devHandle);
        if (err != ESP_OK)
        {
            ESP_LOGE("Accelerometer", "addToI2CBus: Failed to add device: %s", esp_err_to_name(err));
            return false;
        }

        return true;
    }

    /// @brief Initialize LSM6DS accelerometer with wrist tilt detection
    /// @return True if successful
    bool init()
    {
        if (!_devHandle)
        {
            ESP_LOGE("Accelerometer", "init: Device not added to I2C bus");
            return false;
        }

        // Initialization sequence for LSM6DS with wrist tilt detection
        // 0x1018 (26Hz ODR, ±2g), 0x1100 (Gyro OFF), 0x1260 (open-drain INT, active low),
        // 0x5880 (enable embedded functions), 0x1980 (enable wrist tilt),
        // 0x5F00 (INT2 disabled for testing)
        struct RegValue
        {
            uint8_t reg;
            uint8_t value;
        };
        
        const RegValue initSequence[] = {
            {0x10, 0x18},  // CTRL1_XL: 26Hz ODR, ±2g
            {0x11, 0x00},  // CTRL2_G: Gyro OFF
            {0x12, 0x60},  // CTRL3_C: open-drain INT (bit 6=1), active low (bit 5=1)
            {0x58, 0x80},  // TAP_CFG0: enable embedded functions
            {0x19, 0x80},  // CTRL10_C: enable wrist tilt
            {0x5F, 0x00}   // MD2_CFG: INT2 disabled for testing
        };

        // Write each register
        for (size_t i = 0; i < sizeof(initSequence) / sizeof(initSequence[0]); i++)
        {
            uint8_t write_buf[2] = {initSequence[i].reg, initSequence[i].value};
            esp_err_t err = i2c_master_transmit(_devHandle, write_buf, sizeof(write_buf), 1000);
            if (err != ESP_OK)
            {
                ESP_LOGE("Accelerometer", "init: Failed to write reg 0x%02x: %s", 
                      initSequence[i].reg, esp_err_to_name(err));
                return false;
            }
            
            // Small delay between writes
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        ESP_LOGI("Accelerometer", "init: Configured LSM6DS for wrist tilt detection");
        
        // Clear any pending interrupts
        clearInterrupt();
        
        return true;
    }

    /// @brief Clear accelerometer interrupt by reading status registers
    /// @param logFunc Optional logging function for debug output
    void clearInterrupt(void (*logFunc)(const char*) = nullptr)
    {
        if (!_devHandle)
        {
            return;
        }

        // Read status registers to clear interrupts
        // For LSM6DS with wrist tilt, we need to read:
        // - 0x1A (ALL_INT_SRC) - All interrupt source register
        // - 0x1D (WAKE_UP_SRC) - Wake-up interrupt source
        // - 0x1C (TAP_SRC) - Tap source (may include tilt status)
        const uint8_t statusRegs[] = {0x1A, 0x1D, 0x1C};
        
        for (size_t i = 0; i < sizeof(statusRegs); i++)
        {
            uint8_t reg = statusRegs[i];
            uint8_t value = 0;
            
            esp_err_t err = i2c_master_transmit_receive(_devHandle, &reg, 1, &value, 1, 1000);
            if (err == ESP_OK)
            {
                if (logFunc)
                {
                    char logBuf[64];
                    snprintf(logBuf, sizeof(logBuf), "IMU: Cleared interrupt reg 0x%02X = 0x%02X\r\n", reg, value);
                    logFunc(logBuf);
                }
            }
            else
            {
                ESP_LOGW("Accelerometer", "clearInterrupt: Failed to read reg 0x%02x: %s", 
                      reg, esp_err_to_name(err));
            }
        }
    }

    /// @brief Remove device from I2C bus and cleanup
    void deinit()
    {
        if (_devHandle)
        {
            i2c_master_bus_rm_device(_devHandle);
            _devHandle = nullptr;
        }
    }

    /// @brief Check if accelerometer is initialized
    /// @return True if initialized
    bool isInitialized() const
    {
        return _devHandle != nullptr;
    }

    /// @brief Get device handle
    /// @return Device handle
    i2c_master_dev_handle_t getDeviceHandle() const
    {
        return _devHandle;
    }

private:
    uint8_t _i2cAddr;                           // I2C address
    i2c_master_dev_handle_t _devHandle;         // I2C device handle
};
