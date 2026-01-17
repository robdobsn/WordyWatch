/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RTC - Real-Time Clock (RV-4162-C7 / RV-3028-C7)
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include <time.h>
#include "driver/i2c_master.h"

#define DEBUG_SYSTEM_TIME_FROM_RTC
class RTC
{
public:
    /// @brief Enumeration for RTC device types
    enum class RTCDeviceType
    {
        RV4162C7,
        RV3028C7,
        UNKNOWN
    };

    /// @brief Constructor
    /// @param i2cAddr I2C address of the RTC (default 0x68 for RV-4162-C7, 0x52 for RV-3028-C7)
    explicit RTC(uint8_t i2cAddr = 0x68)
        : _i2cAddr(i2cAddr)
        , _devHandle(nullptr)
        , _deviceType(RTCDeviceType::RV3028C7)
    {
    }

    /// @brief Destructor
    ~RTC()
    {
        deinit();
    }

    /// @brief Get I2C address
    /// @return I2C address
    uint8_t getI2CAddress() const
    {
        return _i2cAddr;
    }

    /// @brief Set I2C config
    /// @param addr New I2C address
    /// @param devType Device type string
    void setI2CConfig(uint8_t addr, const String& devType)
    {
        _i2cAddr = addr;

        // Remove dashes from device type
        String devTypeClean = devType;
        devTypeClean.replace("-", "");
        if (devTypeClean.equalsIgnoreCase("RV4162C7"))
            _deviceType = RTCDeviceType::RV4162C7;
        else if (devTypeClean.equalsIgnoreCase("RV3028C7"))
            _deviceType = RTCDeviceType::RV3028C7;
        else
            _deviceType = RTCDeviceType::UNKNOWN;
    }

    /// @brief Add RTC device to I2C bus
    /// @param busHandle I2C bus handle
    /// @param sclSpeedHz SCL speed in Hz
    /// @return True if successful
    bool addToI2CBus(i2c_master_bus_handle_t busHandle, uint32_t sclSpeedHz)
    {
        if (!busHandle)
        {
            LOG_E(MODULE_PREFIX, "addToI2CBus: Invalid bus handle");
            return false;
        }

        // Configure RTC device
        i2c_device_config_t dev_config = {};
        dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_config.device_address = _i2cAddr;
        dev_config.scl_speed_hz = sclSpeedHz;

        // Add device to bus
        esp_err_t err = i2c_master_bus_add_device(busHandle, &dev_config, &_devHandle);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "addToI2CBus: Failed to add device: %s", esp_err_to_name(err));
            return false;
        }

        return true;
    }

    /// @brief Initialize RTC device
    /// @return True if successful
    bool init()
    {
        if (!_devHandle)
        {
            LOG_E(MODULE_PREFIX, "init: Device not added to I2C bus");
            return false;
        }

        // Enable 32.768 kHz output on CLKOUT pin (device-specific register)
        uint8_t clkout_config[2];
        if (_deviceType == RTCDeviceType::RV3028C7)
        {
            clkout_config[0] = 0x35;  // CLKOUT Control register
            clkout_config[1] = 0x80;  // Enable CLKOUT at 32.768 kHz (bit 7=1, FD[1:0]=00)
        }
        else  // RV4162C7
        {
            clkout_config[0] = 0x0F;  // Extension Register
            clkout_config[1] = 0x00;  // FD[2:0] = 000 for 32.768 kHz
        }
        
        esp_err_t err = i2c_master_transmit(_devHandle, clkout_config, sizeof(clkout_config), 1000);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "init: Failed to configure CLKOUT: %s", esp_err_to_name(err));
            // Don't fail initialization, just log the error
        }
        else
        {
            LOG_I(MODULE_PREFIX, "init: CLKOUT configured for 32.768 kHz");
        }

        return true;
    }

    /// @brief Read time from RTC
    /// @param timeinfo Pointer to tm structure to fill with time data
    /// @return True if successful
    bool readTime(struct tm* timeinfo)
    {
        if (!_devHandle || !timeinfo)
        {
            return false;
        }

        // Time registers start postion
        // Read 7 bytes: Seconds, Minutes, Hours, Weekday, Date, Month, Year
        uint8_t reg_addr = _deviceType == RTCDeviceType::RV3028C7 ? 0x00 : 0x01;
        uint8_t time_data[7];
        
        esp_err_t err = i2c_master_transmit_receive(_devHandle, &reg_addr, 1, time_data, sizeof(time_data), 1000);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "readTime: I2C read failed: %s", esp_err_to_name(err));
            return false;
        }

        // Log raw RTC data for debugging
        LOG_I(MODULE_PREFIX, "readTime: Raw RTC data: %02X %02X %02X %02X %02X %02X %02X",
              time_data[0], time_data[1], time_data[2], time_data[3], 
              time_data[4], time_data[5], time_data[6]);
        
        // Convert BCD to decimal
        auto bcd_to_dec = [](uint8_t bcd) -> uint8_t {
            return ((bcd >> 4) * 10) + (bcd & 0x0F);
        };

        // Parse time data (all in BCD format)
        timeinfo->tm_sec = bcd_to_dec(time_data[0] & 0x7F);      // 0x01: Seconds (0-59)
        timeinfo->tm_min = bcd_to_dec(time_data[1] & 0x7F);      // 0x02: Minutes (0-59)
        timeinfo->tm_hour = bcd_to_dec(time_data[2] & 0x3F);     // 0x03: Hours (0-23)
        timeinfo->tm_wday = bcd_to_dec(time_data[3] & 0x07);     // 0x04: Weekday (0-6)
        timeinfo->tm_mday = bcd_to_dec(time_data[4] & 0x3F);     // 0x05: Date (1-31)
        timeinfo->tm_mon = bcd_to_dec(time_data[5] & 0x1F) - 1;  // 0x06: Month (1-12) -> (0-11)
        timeinfo->tm_year = bcd_to_dec(time_data[6]) + 100;      // 0x07: Year (00-99) -> years since 1900

        // Set DST flag to -1 (unknown)
        timeinfo->tm_isdst = -1;

        return true;
    }

    /// @brief Set system time from RTC
    /// @return True if successful
    bool setSystemTimeFromRTC()
    {
        struct tm timeinfo;
        if (!readTime(&timeinfo))
        {
            return false;
        }

        // Set system time
        struct timeval tv;
        tv.tv_sec = mktime(&timeinfo);
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

#ifdef DEBUG_SYSTEM_TIME_FROM_RTC 
        LOG_I(MODULE_PREFIX, "System time set from RTC: %04d-%02d-%02d %02d:%02d:%02d", 
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
#endif

        return true;
    }

    /// @brief Get time from RTC falling back to system time if RTC read fails
    /// @param timeinfo Pointer to tm structure to fill with time data
    /// @return True if successful
    bool getTime(struct tm* timeinfo)
    {
        if (!readTime(timeinfo))
        {
            // Fallback to system time
            time_t now = time(nullptr);
            struct tm* sysTime = localtime(&now);
            if (sysTime)
                *timeinfo = *sysTime;
            else
                return false;
        }
        return true;
    }

    /// @brief Write time to RTC
    /// @param timeinfo Pointer to tm structure with time data to write
    /// @return True if successful
    bool writeTime(const struct tm* timeinfo)
    {
        if (!_devHandle || !timeinfo)
        {
            return false;
        }

        // Convert decimal to BCD
        auto dec_to_bcd = [](uint8_t dec) -> uint8_t {
            return ((dec / 10) << 4) | (dec % 10);
        };

        // Prepare time data (register address + 7 bytes of time data)
        uint8_t write_buf[8];
        write_buf[0] = _deviceType == RTCDeviceType::RV3028C7 ? 0x00 : 0x01;  // Starting register address
        write_buf[1] = dec_to_bcd(timeinfo->tm_sec);           // Seconds (0-59)
        write_buf[2] = dec_to_bcd(timeinfo->tm_min);           // Minutes (0-59)
        write_buf[3] = dec_to_bcd(timeinfo->tm_hour);          // Hours (0-23)
        write_buf[4] = dec_to_bcd(timeinfo->tm_wday);          // Weekday (0-6)
        write_buf[5] = dec_to_bcd(timeinfo->tm_mday);          // Date (1-31)
        write_buf[6] = dec_to_bcd(timeinfo->tm_mon + 1);       // Month (0-11) -> (1-12)
        write_buf[7] = dec_to_bcd(timeinfo->tm_year - 100);    // Year (years since 1900) -> (00-99)

        esp_err_t err = i2c_master_transmit(_devHandle, write_buf, sizeof(write_buf), 1000);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "writeTime: I2C write failed: %s", esp_err_to_name(err));
            return false;
        }

        LOG_I(MODULE_PREFIX, "writeTime: Time written to RTC: %04d-%02d-%02d %02d:%02d:%02d",
              timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
              timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        
         return true;
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

    /// @brief Check if RTC is initialized
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
    RTCDeviceType _deviceType;                  // Device type

    static constexpr const char* MODULE_PREFIX = "RTC";
};
