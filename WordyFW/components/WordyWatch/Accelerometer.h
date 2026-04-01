/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Accelerometer - LSM6DS accelerometer with wrist tilt detection
//
// Rob Dobson 2025
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftCore.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "RTC.h"

// Debug: install GPIO ISR on wake pin to count wrist-tilt interrupts
// #define DEBUG_WRIST_TILT_INT

// Debug: use simple motion wake-up interrupt instead of wrist tilt (easier to trigger)
// #define DEBUG_USE_MOTION_WAKEUP_INT

// Use basic tilt interrupt (embedded function, ~35deg change) instead of wrist tilt
// Wrist tilt INT2 output generates constant periodic pulses (~0.8Hz) regardless of motion
// #define DEBUG_USE_BASIC_TILT_INT

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

    /// @brief Set wrist tilt sensitivity config (BANK_B)
    /// @param latency Latency in 40ms units
    /// @param threshold Threshold in 15.625mg units
    /// @param mask Axis direction mask bits [7:2]
    void setWristTiltConfig(uint8_t latency, uint8_t threshold, uint8_t mask)
    {
        _wristTiltLatency = latency;
        _wristTiltThreshold = threshold;
        _wristTiltMask = mask;
    }

    /// @brief Add accelerometer device to I2C bus
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

        // Configure accelerometer device
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

    /// @brief Initialize LSM6DS accelerometer with wrist tilt detection
    /// @return True if successful
    bool init()
    {
        if (!_devHandle)
        {
            LOG_E(MODULE_PREFIX, "init: Device not added to I2C bus");
            return false;
        }

        // Read WHO_AM_I register to identify chip variant
        uint8_t whoAmIReg = 0x0F;
        uint8_t whoAmI = 0;
        esp_err_t err = i2c_master_transmit_receive(_devHandle, &whoAmIReg, 1, &whoAmI, 1, 1000);
        if (err == ESP_OK)
        {
            LOG_I(MODULE_PREFIX, "init: WHO_AM_I = 0x%02x (%s)", whoAmI,
                  whoAmI == 0x69 ? "LSM6DS3/DS33" :
                  whoAmI == 0x6A ? "LSM6DS3TR-C/DSM/DSL" :
                  whoAmI == 0x6C ? "LSM6DSOX" : "unknown");
        }
        else
        {
            LOG_W(MODULE_PREFIX, "init: Failed to read WHO_AM_I: %s", esp_err_to_name(err));
        }

        // Software reset to clear all stale register state from previous boots
        // (IMU stays powered across ESP soft resets)
        {
            uint8_t swReset[] = {0x12, 0x01};  // CTRL3_C: SW_RESET (bit0)
            i2c_master_transmit(_devHandle, swReset, 2, 1000);
            vTaskDelay(pdMS_TO_TICKS(50));  // Wait for reset to complete
            LOG_I(MODULE_PREFIX, "init: Software reset complete");
        }

        // Initialization sequence for LSM6DS interrupt configuration
        // Two modes available:
        //   DEBUG_USE_MOTION_WAKEUP_INT: simple motion wake-up interrupt (easy to trigger for debugging)
        //   Default: wrist tilt interrupt (requires embedded function support, LSM6DSM/DSOX only)
        struct RegValue
        {
            uint8_t reg;
            uint8_t value;
        };

#ifdef DEBUG_USE_MOTION_WAKEUP_INT
        // Motion wake-up interrupt - fires on any acceleration change exceeding threshold
        // Works on all LSM6DS variants (DS3, DSM, DSOX). Easy to trigger by moving the device.
        const RegValue initSequence[] = {
            {0x10, 0x20},  // CTRL1_XL: 26Hz ODR, ±2g (ODR=0010, FS=00)
            {0x11, 0x00},  // CTRL2_G: Gyro OFF
            {0x12, 0x74},  // CTRL3_C: BDU (bit6), active-low (bit5), open-drain (bit4), IF_INC (bit2)
            {0x5B, 0x02},  // WAKE_UP_THS: low threshold (~63mg at ±2g, 2*FS/64)
            {0x5C, 0x00},  // WAKE_UP_DUR: no duration requirement
            {0x58, 0x80},  // TAP_CFG: INTERRUPTS_ENABLE (bit7)
            {0x5F, 0x20}   // MD2_CFG: INT2_WU (bit5), route wake-up to INT2
        };
        LOG_I(MODULE_PREFIX, "init: Using motion wake-up interrupt mode (DEBUG)");
#elif defined(DEBUG_USE_BASIC_TILT_INT)
        // Basic tilt interrupt for LSM6DS3TR-C — tests embedded function engine
        // Uses tilt_en (bit3) in CTRL10_C + func_en (bit2), routed via MD2_CFG int2_tilt (bit1)
        // Triggers on ~35° tilt change. Simpler than wrist tilt.
        const RegValue initSequence[] = {
            {0x10, 0x50},  // CTRL1_XL: 208Hz ODR, ±2g (higher ODR for better tilt sensitivity)
            {0x11, 0x00},  // CTRL2_G: Gyro OFF
            {0x12, 0x74},  // CTRL3_C: BDU (bit6), active-low (bit5), open-drain (bit4), IF_INC (bit2)
            {0x58, 0x80},  // TAP_CFG: INTERRUPTS_ENABLE (bit7) - master enable for interrupts
            {0x19, 0x0C},  // CTRL10_C: tilt_en (bit3) + func_en (bit2) - enable embedded tilt
            {0x5F, 0x02}   // MD2_CFG: INT2_TILT (bit1) - route tilt to INT2
        };
        LOG_I(MODULE_PREFIX, "init: Using basic tilt interrupt mode (DEBUG)");
#else
        // Wrist tilt interrupt for LSM6DS3TR-C
        // CTRL10_C: func_en (bit2) MUST be set for embedded functions engine to run,
        //   plus wrist_tilt_en (bit7) to enable wrist tilt detection
        // INT2 routing for wrist tilt is via DRDY_PULSE_CFG_G (0x0B) bit0 (int2_wrist_tilt),
        //   NOT via MD2_CFG int2_tilt (that's for basic tilt only)
        const RegValue initSequence[] = {
            {0x10, 0x60},  // CTRL1_XL: 104Hz ODR, ±2g (ODR=0100, FS=00) — higher ODR for wrist tilt
            {0x11, 0x00},  // CTRL2_G: Gyro OFF
            {0x12, 0x74},  // CTRL3_C: BDU (bit6), active-low (bit5), open-drain (bit4), IF_INC (bit2)
            {0x0E, 0x00},  // INT2_CTRL: clear all DRDY/FIFO routing on INT2
            {0x5F, 0x00},  // MD2_CFG: clear all routing
            {0x58, 0x81},  // TAP_CFG: INTERRUPTS_ENABLE (bit7) + LIR (bit0) - latch interrupt until read
            {0x19, 0x84},  // CTRL10_C: wrist_tilt_en (bit7) + func_en (bit2) - enable embedded functions
            {0x0B, 0x01}   // DRDY_PULSE_CFG_G: int2_wrist_tilt (bit0) - route wrist tilt to INT2
        };
#endif

        // Write each register
        for (size_t i = 0; i < sizeof(initSequence) / sizeof(initSequence[0]); i++)
        {
            uint8_t write_buf[2] = {initSequence[i].reg, initSequence[i].value};
            esp_err_t err = i2c_master_transmit(_devHandle, write_buf, sizeof(write_buf), 1000);
            if (err != ESP_OK)
            {
                LOG_E(MODULE_PREFIX, "init: Failed to write reg 0x%02x: %s", 
                      initSequence[i].reg, esp_err_to_name(err));
                return false;
            }
            
            // Small delay between writes
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        LOG_I(MODULE_PREFIX, "init: Configured LSM6DS for wrist tilt detection at address 0x%02X", _i2cAddr);

        // Read back all written registers to verify writes took effect
        for (size_t i = 0; i < sizeof(initSequence) / sizeof(initSequence[0]); i++)
        {
            uint8_t reg = initSequence[i].reg;
            uint8_t readVal = 0;
            esp_err_t err = i2c_master_transmit_receive(_devHandle, &reg, 1, &readVal, 1, 1000);
            if (err == ESP_OK)
            {
                LOG_I(MODULE_PREFIX, "init: verify reg 0x%02x wrote=0x%02x read=0x%02x %s",
                      reg, initSequence[i].value, readVal,
                      (readVal == initSequence[i].value) ? "OK" : "MISMATCH");
            }
            else
            {
                LOG_W(MODULE_PREFIX, "init: verify reg 0x%02x read failed: %s", reg, esp_err_to_name(err));
            }
        }

#if !defined(DEBUG_USE_MOTION_WAKEUP_INT) && !defined(DEBUG_USE_BASIC_TILT_INT)
        // Access embedded function BANK B to read/configure wrist tilt parameters
        // FUNC_CFG_ACCESS (0x01): func_cfg_en is bits [7:5]
        //   USER_BANK=0x00, BANK_A=0x80, BANK_B=0xA0
        {
            uint8_t bankB[] = {0x01, 0xA0};  // FUNC_CFG_ACCESS: BANK_B (bits 7:5 = 101)
            i2c_master_transmit(_devHandle, bankB, 2, 1000);
            vTaskDelay(pdMS_TO_TICKS(5));

            // Read current wrist tilt config registers in BANK B
            const uint8_t bankBRegs[] = {0x50, 0x54, 0x59};  // A_WRIST_TILT_LAT, A_WRIST_TILT_THS, A_WRIST_TILT_MASK
            const char* bankBNames[] = {"TILT_LAT", "TILT_THS", "TILT_MASK"};
            for (size_t i = 0; i < sizeof(bankBRegs); i++)
            {
                uint8_t reg = bankBRegs[i];
                uint8_t val = 0;
                err = i2c_master_transmit_receive(_devHandle, &reg, 1, &val, 1, 1000);
                LOG_I(MODULE_PREFIX, "init: BANK_B defaults reg 0x%02x (%s) = 0x%02x", reg, bankBNames[i], val);
            }

            // Configure wrist tilt with ST recommended default parameters
            // A_WRIST_TILT_LAT (0x50): latency, 1 LSB = 40ms, default 0x0F (600ms)
            //   Use default — filters out brief bumps, requires deliberate gesture
            uint8_t latency[] = {0x50, _wristTiltLatency};
            i2c_master_transmit(_devHandle, latency, 2, 1000);
            vTaskDelay(pdMS_TO_TICKS(5));

            // A_WRIST_TILT_THS (0x54): threshold, 1 LSB = 15.625mg, default 0x20 (500mg)
            //   Use default — good balance for wrist raise detection
            uint8_t threshold[] = {0x54, _wristTiltThreshold};
            i2c_master_transmit(_devHandle, threshold, 2, 1000);
            vTaskDelay(pdMS_TO_TICKS(5));

            // A_WRIST_TILT_MASK (0x59): axis direction enable mask
            //   Bits [7:2]: zpos, zneg, ypos, yneg, xpos, xneg
            //   Set to 0xFC to enable ALL directions
            uint8_t mask[] = {0x59, _wristTiltMask};
            i2c_master_transmit(_devHandle, mask, 2, 1000);
            vTaskDelay(pdMS_TO_TICKS(5));

            // Read back to verify
            for (size_t i = 0; i < sizeof(bankBRegs); i++)
            {
                uint8_t reg = bankBRegs[i];
                uint8_t val = 0;
                i2c_master_transmit_receive(_devHandle, &reg, 1, &val, 1, 1000);
                LOG_I(MODULE_PREFIX, "init: BANK_B configured reg 0x%02x (%s) = 0x%02x", reg, bankBNames[i], val);
            }

            // Switch back to USER BANK
            uint8_t userBank[] = {0x01, 0x00};
            i2c_master_transmit(_devHandle, userBank, 2, 1000);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
#endif
        
        // Clear any pending interrupts
        clearInterrupt();
        
        return true;
    }

    /// @brief Read tilt interrupt status without clearing other registers
    /// @param tiltDetected Set to true if tilt_ia (basic tilt) or wrist_tilt_ia was flagged
    /// @return true if registers were read successfully
    bool readTiltStatus(bool& tiltDetected)
    {
        tiltDetected = false;
        if (!_devHandle)
            return false;

        // FUNC_SRC1 (0x53): bit5 = tilt_ia (basic tilt)
        uint8_t reg = 0x53;
        uint8_t funcSrc1 = 0;
        esp_err_t err = i2c_master_transmit_receive(_devHandle, &reg, 1, &funcSrc1, 1, 1000);
        if (err != ESP_OK)
            return false;

        // FUNC_SRC2 (0x54): bit0 = wrist_tilt_ia
        reg = 0x54;
        uint8_t funcSrc2 = 0;
        err = i2c_master_transmit_receive(_devHandle, &reg, 1, &funcSrc2, 1, 1000);
        if (err != ESP_OK)
            return false;

        tiltDetected = ((funcSrc1 & 0x20) != 0) || ((funcSrc2 & 0x01) != 0);
        LOG_I(MODULE_PREFIX, "readTiltStatus: FUNC_SRC1=0x%02x FUNC_SRC2=0x%02x tilt=%s",
              funcSrc1, funcSrc2, tiltDetected ? "YES" : "no");
        return true;
    }

#ifdef DEBUG_WRIST_TILT_INT
    /// @brief Setup GPIO ISR on wake pin to count wrist tilt interrupts
    /// @param pinNum GPIO pin number for the interrupt
    void setupWristTiltInterrupt(int pinNum)
    {
        if (pinNum < 0)
        {
            LOG_W(MODULE_PREFIX, "setupWristTiltInterrupt: no wake pin configured");
            return;
        }

        _wristTiltIntPin = pinNum;
        _wristTiltIntCount = 0;

        // Configure GPIO for falling edge interrupt (active-low, open-drain with pull-up)
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << pinNum);
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "setupWristTiltInterrupt: gpio_config failed: %s", esp_err_to_name(err));
            return;
        }

        // Install ISR service and add handler
        err = gpio_install_isr_service(0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        {
            LOG_E(MODULE_PREFIX, "setupWristTiltInterrupt: gpio_install_isr_service failed: %s", esp_err_to_name(err));
            return;
        }

        err = gpio_isr_handler_add((gpio_num_t)pinNum, wristTiltISR, this);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "setupWristTiltInterrupt: gpio_isr_handler_add failed: %s", esp_err_to_name(err));
            return;
        }

        LOG_I(MODULE_PREFIX, "setupWristTiltInterrupt: ISR installed on GPIO %d (falling edge)", pinNum);
    }

    /// @brief Log wrist tilt interrupt count periodically
    void logWristTiltInterrupts(RTC& rtc)
    {
        if (!Raft::isTimeout(millis(), _wristTiltIntLastLogMs, WRIST_TILT_INT_LOG_INTERVAL_MS))
            return;
        _wristTiltIntLastLogMs = millis();

        uint32_t count = _wristTiltIntCount;
        uint32_t delta = count - _wristTiltIntLastLoggedCount;
        _wristTiltIntLastLoggedCount = count;

        // Read GPIO level BEFORE any I2C reads
        int pinBefore = _wristTiltIntPin >= 0 ? gpio_get_level((gpio_num_t)_wristTiltIntPin) : -1;

        // Calculate time from last diagnostic read to ISR
        int64_t isrTimeUs = _wristTiltIntLastIsrTimeUs;
        int64_t timeSinceLastDiagUs = isrTimeUs - _wristTiltIntLastDiagTimeUs;

        LOG_I(MODULE_PREFIX, "wristTiltInt: count=%lu delta=%lu pinBefore=%d isrAfterDiag=%lldus",
              (unsigned long)count, (unsigned long)delta, pinBefore, timeSinceLastDiagUs);

        // Poll accelerometer status registers for diagnostics (this clears IMU latch)
        debugPollStatus();

        // Read GPIO level AFTER IMU register reads (latch should be released if IMU was source)
        int pinAfterImu = _wristTiltIntPin >= 0 ? gpio_get_level((gpio_num_t)_wristTiltIntPin) : -1;

        // Read RTC status register to check if RTC is asserting INT#
        uint8_t rtcStatus = 0xFF;
        rtc.readStatusRegister(rtcStatus);

        // Read GPIO level AFTER RTC status read
        int pinAfterRtc = _wristTiltIntPin >= 0 ? gpio_get_level((gpio_num_t)_wristTiltIntPin) : -1;

        LOG_I(MODULE_PREFIX, "wristTiltInt: pinAfterImu=%d pinAfterRtc=%d rtcStatus=0x%02x",
              pinAfterImu, pinAfterRtc, rtcStatus);

        // Record when we finished diagnostic reads (for next ISR timing comparison)
        _wristTiltIntLastDiagTimeUs = esp_timer_get_time();
    }
#endif

    /// @brief Clear accelerometer interrupt by reading status registers
    void clearInterrupt()
    {
        if (!_devHandle)
        {
            return;
        }

        // Read status registers to clear interrupts
        // For LSM6DS3TR-C with wrist tilt, we need to read:
        // - 0x1A (ALL_INT_SRC) - All interrupt source register
        // - 0x1D (WAKE_UP_SRC) - Wake-up interrupt source
        // - 0x1C (TAP_SRC) - Tap source (may include tilt status)
        // - 0x53 (FUNC_SRC1) - Embedded function source (tilt_ia in bit5)
        // - 0x54 (FUNC_SRC2) - Embedded function source 2 (wrist_tilt_ia in bit0)
        // - 0x55 (WRIST_TILT_IA) - Wrist tilt direction flags
        const uint8_t statusRegs[] = {0x1A, 0x1D, 0x1C, 0x53, 0x54, 0x55};
        
        for (size_t i = 0; i < sizeof(statusRegs); i++)
        {
            uint8_t reg = statusRegs[i];
            uint8_t value = 0;
            
            esp_err_t err = i2c_master_transmit_receive(_devHandle, &reg, 1, &value, 1, 1000);
            if (err == ESP_OK)
            {
                LOG_I(MODULE_PREFIX, "clearInterrupt: Read reg 0x%02x = 0x%02x", reg, value);
            }
            else
            {
                LOG_W(MODULE_PREFIX, "clearInterrupt: Failed to read reg 0x%02x: %s", 
                      reg, esp_err_to_name(err));
            }
        }
    }

#ifdef DEBUG_WRIST_TILT_INT
    /// @brief Read accelerometer data and interrupt status for diagnostics
    void debugPollStatus()
    {
        if (!_devHandle)
            return;

        // Read acceleration data (OUTX_L_XL=0x28, 6 bytes for X,Y,Z each 16-bit)
        uint8_t accelReg = 0x28;
        uint8_t accelData[6] = {};
        esp_err_t err = i2c_master_transmit_receive(_devHandle, &accelReg, 1, accelData, 6, 1000);
        int16_t ax = 0, ay = 0, az = 0;
        if (err == ESP_OK)
        {
            ax = (int16_t)(accelData[0] | (accelData[1] << 8));
            ay = (int16_t)(accelData[2] | (accelData[3] << 8));
            az = (int16_t)(accelData[4] | (accelData[5] << 8));
        }

        // Read interrupt/status registers
        // STATUS_REG (0x1E), ALL_INT_SRC (0x1A), FUNC_SRC1 (0x53), FUNC_SRC2 (0x54), WRIST_TILT_IA (0x55),
        // CTRL10_C (0x19), DRDY_PULSE_CFG_G (0x0B), INT2_CTRL (0x0E), MD2_CFG (0x5F), TAP_CFG (0x58)
        const uint8_t diagRegs[] = {0x1E, 0x1A, 0x53, 0x54, 0x55, 0x19, 0x0B, 0x0E, 0x5F, 0x58};
        uint8_t diagVals[sizeof(diagRegs)] = {};
        for (size_t i = 0; i < sizeof(diagRegs); i++)
        {
            uint8_t reg = diagRegs[i];
            i2c_master_transmit_receive(_devHandle, &reg, 1, &diagVals[i], 1, 1000);
        }

        LOG_I(MODULE_PREFIX, "diag: ax=%d ay=%d az=%d STATUS=0x%02x ALL_INT=0x%02x FUNC1=0x%02x FUNC2=0x%02x WRIST_IA=0x%02x CTRL10=0x%02x DRDY_CFG=0x%02x INT2_CTRL=0x%02x MD2_CFG=0x%02x TAP_CFG=0x%02x",
              ax, ay, az, diagVals[0], diagVals[1], diagVals[2], diagVals[3], diagVals[4], diagVals[5], diagVals[6], diagVals[7], diagVals[8], diagVals[9]);
    }
#endif

    /// @brief Read raw accelerometer data
    /// @param ax X axis raw sample
    /// @param ay Y axis raw sample
    /// @param az Z axis raw sample
    /// @return True if successful
    bool readAccelRaw(int16_t& ax, int16_t& ay, int16_t& az)
    {
        if (!_devHandle)
            return false;

        uint8_t accelReg = 0x28;
        uint8_t accelData[6] = {};
        esp_err_t err = i2c_master_transmit_receive(_devHandle, &accelReg, 1, accelData, 6, 1000);
        if (err != ESP_OK)
            return false;

        ax = static_cast<int16_t>(accelData[0] | (accelData[1] << 8));
        ay = static_cast<int16_t>(accelData[2] | (accelData[3] << 8));
        az = static_cast<int16_t>(accelData[4] | (accelData[5] << 8));
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

    // Wrist tilt sensitivity config (BANK_B)
    uint8_t _wristTiltLatency = 0x08;
    uint8_t _wristTiltThreshold = 0x10;
    uint8_t _wristTiltMask = 0xFC;

#ifdef DEBUG_WRIST_TILT_INT
    int _wristTiltIntPin = -1;
    volatile uint32_t _wristTiltIntCount = 0;
    volatile int64_t _wristTiltIntLastIsrTimeUs = 0;
    int64_t _wristTiltIntLastDiagTimeUs = 0;
    uint32_t _wristTiltIntLastLogMs = 0;
    uint32_t _wristTiltIntLastLoggedCount = 0;
    static constexpr uint32_t WRIST_TILT_INT_LOG_INTERVAL_MS = 5000;
public:
    /// @brief Check if new tilt interrupts have occurred and clear the count
    /// @return Number of interrupts since last call
    uint32_t getAndClearTiltIntCount()
    {
        uint32_t count = _wristTiltIntCount;
        uint32_t last = _wristTiltIntLastClearedCount;
        _wristTiltIntLastClearedCount = count;
        return count - last;
    }
private:
    uint32_t _wristTiltIntLastClearedCount = 0;
    static void IRAM_ATTR wristTiltISR(void* arg)
    {
        Accelerometer* self = static_cast<Accelerometer*>(arg);
        if (!self)
            return;
        self->_wristTiltIntCount = self->_wristTiltIntCount + 1;
        self->_wristTiltIntLastIsrTimeUs = esp_timer_get_time();
    }
#endif

    static constexpr const char* MODULE_PREFIX = "Acc";
};
