# WordyWatch Deep Sleep Current Reduction

## Baseline Measurement

- **218µA** average deep sleep current (measured 2026-04-09)

## Current Budget Estimate (Before Changes)

| Source | Est. Current | Notes |
|--------|-------------|-------|
| LSM6DS3TR-C (U14) | ~75µA | 104Hz ODR, wrist tilt enabled, embedded functions running |
| W25Q32 flash (U2) | ~25µA | Standby (not deep power-down) |
| ESP32-C6 (U1) | ~7µA | Deep sleep with RTC + EXT1 wakeup |
| XC6210 LDO (U6) | ~6-15µA | Quiescent current |
| PWR_CTRL divider (R13 470kΩ) | ~7µA | Held HIGH via gpio_hold |
| STRAP_CTRL (R27 470kΩ) | ~7µA | If pin floats high |
| MCP73831 (U4) | ~2µA | No USB connected |
| RV-3028-C7 (U15) | ~0.1µA | Negligible |
| VSENSE divider (R11+R12, 6.9MΩ) | ~0.5µA | Negligible |
| GPIO leakage (charlieplex, pull-ups) | ~50-100µA | 12 charlieplex GPIOs undefined, I2C pull-ups, internal pull-up on WAKE_INT# |

## Changes Applied

### 1. Enable flash deep power-down during sleep

- **Change:** Added `CONFIG_ESP_SLEEP_POWER_DOWN_FLASH=y` to `systypes/WordyWatchV11/sdkconfig.defaults`
- **Expected saving:** ~24µA (flash standby → deep power-down)
- **Result:** No measurable difference. Flash may already have been in a low-power state, or the ESP-IDF handles this internally for deep sleep.

### 2. IMU power-down before deep sleep

- **Change:** Added `powerDown()` method to `Accelerometer.h` — writes `0x00` to CTRL1_XL (ODR=0) and CTRL10_C (disable embedded functions). Called in `PREPARING_TO_SLEEP` state before `clearInterrupt()` in `WordyWatch.cpp`. IMU is reinitialized on wake since deep sleep resets the ESP32.
- **Expected saving:** ~72µA (75µA → ~3µA)
- **Result:** Deep sleep current dropped from 218µA to **56.6µA** — a 161.4µA saving. Larger than expected, suggesting the IMU embedded functions engine was consuming more than the datasheet typical.

## Planned Changes (Not Yet Applied)

### 3. Charlieplex GPIO hold during deep sleep

- **Change:** Added `holdPinsForSleep()` — set all 12 GPIOs to output LOW with `gpio_hold_en()`.
- **Result:** No current saving. Caused display not to turn back on after wake (gpio_hold persists across deep sleep reset and blocks reconfiguration). **Reverted.**

### 4. Remove redundant internal pull-up on WAKE_INT#

- **Change:** Set `wakeIntPinPullup` to `false` in `systypes/WordyWatchV11/SysTypes.json`. External R24 (47kΩ) already pulls up WAKE_INT#.
- **Expected saving:** minor (~1-5µA)
- **Result:** ~0.6µA saving (56.6µA → 56.0µA). Marginal but confirmed the internal pull-up was slightly redundant.

### 5. Check R18 (10kΩ) on RTC EVI pin

If U15 EVI pin has any leakage to GND, R18 at 10kΩ could waste significant current. Verify with current probe. Consider removing or increasing to 100kΩ+.

- **Expected saving:** unknown, possibly significant

## Target

Goal: reduce deep sleep current to <30µA (ESP32-C6 + LDO + RTC baseline).
