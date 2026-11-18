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

#define DEBUG_SET_SYSTEM_TIME_FROM_RTC
#define DEBUG_RTC_READ_TIME
#define DEBUG_RAW_RTC_DATA
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

        // Enable backup switchover so the RTC keeps time from VBACKUP (VBAT) when VDD (+3V3) is cut
        if (_deviceType == RTCDeviceType::RV3028C7)
        {
            configureBackupSwitchover();
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
#ifdef DEBUG_RAW_RTC_DATA
        LOG_I(MODULE_PREFIX, "readTime: Raw RTC data: %02X %02X %02X %02X %02X %02X %02X",
              time_data[0], time_data[1], time_data[2], time_data[3], 
              time_data[4], time_data[5], time_data[6]);
#endif

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

#ifdef DEBUG_RTC_READ_TIME
        LOG_I(MODULE_PREFIX, "readTime: Time read from RTC: %04d-%02d-%02d %02d:%02d:%02d",
              timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
              timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
#endif  
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

#ifdef DEBUG_SET_SYSTEM_TIME_FROM_RTC 
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

    /// @brief Disable all RTC interrupt sources and clear pending flags (RV-3028-C7)
    /// Call this during init to prevent INT# from firing due to stale config
    void disableInterrupts()
    {
        if (!_devHandle || _deviceType != RTCDeviceType::RV3028C7)
            return;

        // Read current status and control registers for diagnostics
        uint8_t statusReg = 0, ctrl1Reg = 0, ctrl2Reg = 0;
        uint8_t timerVal0 = 0, timerVal1 = 0;
        readRegister(0x0E, statusReg);
        readRegister(0x0F, ctrl1Reg);
        readRegister(0x10, ctrl2Reg);
        readRegister(0x0B, timerVal0);
        readRegister(0x0C, timerVal1);

        LOG_I(MODULE_PREFIX, "init: RTC regs Status=0x%02x Ctrl1=0x%02x Ctrl2=0x%02x TimerVal=0x%02x%02x",
              statusReg, ctrl1Reg, ctrl2Reg, timerVal1, timerVal0);

        // Disable all interrupt enables in Control 2 (TIE, AIE, EIE, UIE, CLKIE)
        // Keep 12/24 bit as-is, clear everything else
        uint8_t ctrl2_write[] = {0x10, 0x00};
        i2c_master_transmit(_devHandle, ctrl2_write, sizeof(ctrl2_write), 1000);

        // Disable timer in Control 1 (clear TE bit)
        uint8_t ctrl1_write[] = {0x0F, (uint8_t)(ctrl1Reg & ~0x04)};
        i2c_master_transmit(_devHandle, ctrl1_write, sizeof(ctrl1_write), 1000);

        // Clear all status flags (write 0 to clear)
        uint8_t status_write[] = {0x0E, 0x00};
        i2c_master_transmit(_devHandle, status_write, sizeof(status_write), 1000);

        // Read back to confirm
        readRegister(0x0E, statusReg);
        readRegister(0x0F, ctrl1Reg);
        readRegister(0x10, ctrl2Reg);
        LOG_I(MODULE_PREFIX, "init: RTC after disable ints: Status=0x%02x Ctrl1=0x%02x Ctrl2=0x%02x",
              statusReg, ctrl1Reg, ctrl2Reg);
    }

    /// @brief Read RTC Status register (0x0E) for interrupt source detection
    /// @param status Output status byte
    /// @return True if read succeeded
    bool readStatusRegister(uint8_t& status)
    {
        if (!_devHandle)
            return false;
        return readRegister(0x0E, status);
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
    /// @brief Read a single register
    bool readRegister(uint8_t reg, uint8_t& value)
    {
        return i2c_master_transmit_receive(_devHandle, &reg, 1, &value, 1, 1000) == ESP_OK;
    }

    /// @brief Write a single register
    bool writeRegister(uint8_t reg, uint8_t value)
    {
        uint8_t buf[2] = {reg, value};
        return i2c_master_transmit(_devHandle, buf, sizeof(buf), 1000) == ESP_OK;
    }

    /// @brief Wait for the EEPROM busy flag (Status reg 0x0E bit 7) to clear
    /// @return True if EEPROM became idle within the timeout
    bool waitEEPROMIdle()
    {
        for (int i = 0; i < 50; i++)   // ~500ms max; tEEbusy is typically <63ms
        {
            uint8_t status = 0;
            if (readRegister(0x0E, status) && ((status & 0x80) == 0))
                return true;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        LOG_W(MODULE_PREFIX, "waitEEPROMIdle: EEPROM still busy after timeout");
        return false;
    }

    /// @brief Enable automatic backup switchover (RV-3028-C7)
    /// Configures the EEPROM Backup register (0x37) for Level Switching Mode so the RTC
    /// runs from VBACKUP (wired to VBAT) when VDD (+3V3) collapses on power-down/low battery.
    /// Persists the setting to configuration EEPROM so it survives a full power loss.
    void configureBackupSwitchover()
    {
        // Backup register (0x37) bit layout:
        //   [7] EEOffset[0]  [6] BSIE  [5] TCE  [4] FEDE  [3:2] BSM[1:0]  [1:0] TCR[1:0]
        //   BSM: 00=disabled, 01=Direct Switching, 11=Level Switching (LSM)
        constexpr uint8_t BSM_MASK = 0x0C;   // bits 3:2
        constexpr uint8_t BSM_LSM  = 0x0C;   // 11 = Level Switching Mode
        constexpr uint8_t TCE_BIT  = 0x20;   // bit 5 = trickle charger enable
        constexpr uint8_t FEDE_BIT = 0x10;   // bit 4 = fast edge detection (app manual: always write 1)

        uint8_t backupReg = 0;
        if (!readRegister(0x37, backupReg))
        {
            LOG_E(MODULE_PREFIX, "configureBackupSwitchover: failed to read Backup register");
            return;
        }

        // Select Level Switching Mode, keep trickle charger disabled (backup is the main battery)
        uint8_t newBackupReg = (backupReg & ~(BSM_MASK | TCE_BIT)) | BSM_LSM | FEDE_BIT;
        if (newBackupReg == backupReg)
        {
            LOG_I(MODULE_PREFIX, "configureBackupSwitchover: already configured (0x%02x)", backupReg);
            return;
        }

        // Disable automatic EEPROM refresh (Control 1 reg 0x0F bit 3 = EERD) before EEPROM access
        uint8_t ctrl1 = 0;
        readRegister(0x0F, ctrl1);
        writeRegister(0x0F, ctrl1 | 0x08);

        if (waitEEPROMIdle())
        {
            // Update the RAM mirror so the new mode takes effect immediately
            writeRegister(0x37, newBackupReg);

            // Persist a single byte to configuration EEPROM: EEADDR, EEDATA, then command 0x00->0x21
            writeRegister(0x25, 0x37);           // EEPROM address = Backup register
            writeRegister(0x26, newBackupReg);   // EEPROM data
            writeRegister(0x27, 0x00);           // clear command
            writeRegister(0x27, 0x21);           // write one EEPROM byte
            waitEEPROMIdle();
        }

        // Re-enable automatic EEPROM refresh
        readRegister(0x0F, ctrl1);
        writeRegister(0x0F, ctrl1 & ~0x08);

        uint8_t verify = 0;
        readRegister(0x37, verify);
        if (verify == newBackupReg)
        {
            LOG_I(MODULE_PREFIX, "configureBackupSwitchover: Backup register 0x%02x -> 0x%02x (LSM enabled)",
                  backupReg, verify);
        }
        else
        {
            LOG_E(MODULE_PREFIX, "configureBackupSwitchover: FAILED Backup register 0x%02x, wanted 0x%02x, read 0x%02x",
                  backupReg, newBackupReg, verify);
        }
    }

    uint8_t _i2cAddr;                           // I2C address
    i2c_master_dev_handle_t _devHandle;         // I2C device handle
    RTCDeviceType _deviceType;                  // Device type

    static constexpr const char* MODULE_PREFIX = "RTC";
};
