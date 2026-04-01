# WordyWatch Functionality Specification

## Overview

WordyWatch is a wrist-worn word clock built on ESP32-C6, displaying the current time as illuminated English words on a 12×11 charlieplexed LED matrix. The watch sleeps between glances and wakes on wrist tilt to show the time for a few seconds before returning to deep sleep.

## Hardware Platform

| Component | Detail |
|---|---|
| MCU | ESP32-C6 (unicore, 160 MHz) |
| Display | 12×11 charlieplexed LED matrix (132 LEDs, 12 GPIO drive pins) |
| Accelerometer | LSM6DS3TR-C (I2C 0x6A) |
| RTC | RV-3028-C7 (I2C 0x52) |
| Battery sense | ADC on GPIO 2 (two-point calibrated) |
| Wake/interrupt pin | GPIO 5 (shared: accelerometer INT2 + RTC INT#) |
| Power latch | GPIO 17 (held HIGH to keep power on) |

Two hardware revisions exist (V10, V11) with different pin assignments, RTC chips, and ADC calibration. V11 is the current active revision.

## Current Functionality

### 1. Time Display (Word Clock)

The watch displays time as English words on the LED matrix using pre-generated patterns.

- **Pattern set**: 288 patterns covering 24 hours at 5-minute resolution (layout "Gracegpt8")
- **Minute granularity**: Time is floored to the nearest 5 minutes
- **Minute indicators**: Five LEDs in column x=0 at y=3..7 show minutes past the last 5-minute word time (solid), with the next indicator flashing at 1 Hz to show seconds
- **Display duration**: 5 seconds (`showTimeForMs`), then the display is cleared and the watch enters deep sleep
- **Rendering**: Patterns are stored as 5×32-bit bitmasks per time entry. The WordyWatch state machine sends a `blitMask` JSON command with those words to the LED device
- **Refresh rate**: 50 Hz ISR-driven scan, with precomputed GPIO masks for each lit LED

### 2. Wake-on-Tilt

The watch wakes from deep sleep when the user raises their wrist.

- **Active mode**: Basic tilt detection using the LSM6DS3TR-C embedded functions engine (`tilt_en + func_en`)
- **Trigger**: ~35° orientation change fires INT2 (active-low, open-drain) on the shared wake pin
- **ODR**: 104 Hz accelerometer sample rate
- **Routing**: Tilt interrupt routed to INT2 via MD2_CFG register
- **Software reset**: The IMU is software-reset on every ESP boot to clear stale register state (the IMU stays powered across ESP soft resets)

### 3. Deep Sleep & Power Management

- **Sleep entry**: After displaying time for 5 seconds, the watch stops the LED panel and enters ESP32 deep sleep with EXT1 wakeup on the WAKE_INT pin (any-low trigger)
- **Power latch**: GPIO 17 is held HIGH via `gpio_hold_en` to maintain power during deep sleep
- **Deep sleep = full reset**: `esp_deep_sleep_start()` resets the CPU; on wake, the firmware restarts from `app_main()`
- **Battery monitoring**: VSENSE ADC is read every second, averaged over 100 samples. Battery voltage is computed via two-point linear calibration
- **Low battery shutdown**: When battery voltage drops below 3.45V, the power latch pin is released, cutting power
- **Power button detection**: A power button press raises the VSENSE ADC above a threshold (1550) and also pulls WAKE_INT low via a FET. Press duration is tracked with 200ms debounce. The BOOT button is GPIO9 and should only use an internal pullup after boot (strapping pin).

### 4. Battery Gauge

- **Trigger**: BOOT button (GPIO9, active-low) while the display is on; internal pullup is enabled only after boot (strapping pin)
- **Display**: Bottom row 12-LED bar gauge (left-to-right)
- **Animation**: Sweeps from 1 LED up to the target count and repeats during the display window
- **Timing**: Gauge shows for `batteryGaugeShowMs`; sweep duration `batteryGaugeSweepMs`
- **Scaling**: Battery voltage mapped between `batteryGaugeMinV` and `batteryGaugeMaxV`

### 5. RTC Timekeeping

- **Clock source**: RV-3028-C7 with 32.768 kHz CLKOUT
- **System time sync**: On every boot, the ESP32 system time is set from the RTC
- **Time read**: BCD-encoded 7-byte I2C read (seconds through year)
- **Interrupt management**: All RTC interrupt sources are disabled on init and status flags cleared to prevent spurious triggers on the shared wake pin

### 6. Time Setting

- **Serial console**: REST API endpoint `settime/YYYY-MM-DDTHH:MM:SS` sets both the RTC and system time
- **BLE Current Time Service**: Configured (read + write) but BLE is currently disabled on V11
- **Button-based time set**: Long press detection (2000ms) is configured but the time-setting state machine is not implemented

### 7. LED Panel Control API

Available via serial console REST API:

| Endpoint | Description |
|---|---|
| `leds` | LED panel status JSON |
| `leds/set?x=N&y=N&on=1` | Set individual pixel |
| `leds/clear` | Clear all LEDs |
| `leds/fill?on=1` | Fill all LEDs |
| `leds/testAllLEDs` | Sequential test of all LEDs |
| `leds/blitMask?mask=0x1,0x2` | Blit a packed mask (comma-separated 32-bit words) |
| `leds/status` | Panel status (dimensions, refresh rate, lit count, running) |

### 8. Connectivity (Currently Disabled on V11)

- **BLE**: Peripheral role with Battery, Heart Rate, Current Time, and Device Info standard services, plus a custom command/response service. Enabled on V10, disabled on V11
- **WiFi**: Disabled on both revisions
- **Serial console**: Active on UART 0 with RICSerial protocol framing

## State Machine

```
INITIAL_STATE ──→ DISPLAYING_TIME ──(5s timeout)──→ PREPARING_TO_SLEEP ──→ SLEEPING
                        ↑                                                      │
                        │                                                      │ (deep sleep resets CPU)
                        └──── WOKEN_UP ←── (EXT1 wakeup) ←── (boot) ←─────────┘

Any state ──(low battery)──→ SHUTDOWN_REQUESTED ──→ SHUTTING_DOWN
```

In practice, deep sleep resets the ESP32-C6, so every wakeup re-enters through `INITIAL_STATE`.

## Build System

- **Framework**: ESP-IDF v5.5.2 via Raft build system
- **Build command**: `raft build`
- **Flash command**: `raft r -p COM5`
- **System types**: `WordyWatchV10` and `WordyWatchV11` (separate configs and build outputs)
- **Docker**: `compose.yaml` + `Dockerfile` available for containerized builds

## Hardware Revision Differences (V10 vs V11)

| Feature | V10 | V11 |
|---|---|---|
| RTC | RV-4162-C7 (0x68) | RV-3028-C7 (0x52) |
| Wake pin | GPIO 9 | GPIO 5 |
| I2C SCL | GPIO 5 | GPIO 8 |
| BLE | Enabled | Disabled |
| Battery low voltage | 3.55V | 3.45V |
| ADC calibration | 3.2V@1002, 4.16V@1320 | 3.52V@1156, 4.32V@1413 |

---

## Planned Improvements

### 1. Seconds Counter

Inspired by the QlockOne clock, individual LEDs in the grid corners (or an unused row/column) could be toggled once per second to give a visual sense of time passing while the display is on. For example, four corner LEDs could cycle in sequence (0–3) to represent seconds modulo 4, producing a subtle ticking animation during the 5-second display window.

This requires a 1 Hz callback or timer while the display is active. The RTC's 32.768 kHz CLKOUT could be divided down, or a simple `esp_timer` periodic callback can toggle the LED state.

### 2. Nearest-5-Minute Rounding

Currently the displayed time is floored to the 5-minute boundary (e.g. 10:08 shows as "TEN OH FIVE"). Rounding to the *nearest* 5 minutes would be more intuitive:

- Minutes 0–2: round down (show current 5-min word)
- Minutes 3–4: round up (show next 5-min word)

Implementation: compute `(minute + 2) / 5 * 5` instead of `minute / 5 * 5`. When the rounded value is 60, increment the hour and use minute 0. This is a one-line change in the time-to-pattern-index mapping, combined with appropriate hour rollover logic.

If dot indicators (improvement 3) are also implemented, rounding becomes unnecessary since the exact minute is always visible.

### 3. Button-Based Time Setting

The hardware already supports long-press detection (2000 ms threshold). A proposed time-setting flow:

1. **Enter set mode**: Long-press (≥2 s) while display is on → enter hour-setting mode
2. **Hour-setting mode**: Display flashes the current hour word. Short presses increment the hour. Long-press confirms and advances to minute-setting mode.
3. **Minute-setting mode**: Display flashes the current 5-minute word. Short presses increment by 5 minutes. Long-press confirms, writes to RTC, and returns to normal display.
4. **Timeout**: If no button press for 30 seconds in either setting mode, cancel and revert to the previous time.

Visual feedback: the word pattern should blink at ~2 Hz while in setting mode to distinguish from normal display. The PowerAndSleep class already tracks power button press duration and provides `isPowerButtonPressed()` and `getPowerButtonPressDuration()`.

---

## Technical Design

### Deep Sleep Mode

The watch spends almost all of its time in ESP32-C6 deep sleep. The lifecycle is:

```
Power on / wakeup
    │
    ├── app_main() → full firmware init (I2C, RTC, IMU, LED panel)
    ├── Read time from RTC → set system clock
    ├── Display word pattern for 5 seconds
    ├── Stop LED panel
    ├── esp_deep_sleep_start()
    │       ├── EXT1 wakeup source: GPIO 5 (WAKE_INT, any-low)
    │       ├── gpio_hold_en(GPIO 17) keeps power latch HIGH
    │       └── All RAM lost, RTC memory preserved
    │
    └── On wakeup: CPU resets, execution restarts from app_main()
```

Key details:

- **Wakeup source**: `esp_sleep_enable_ext1_wakeup(1ULL << 5, ESP_EXT1_WAKEUP_ANY_LOW)` — GPIO 5 is the shared interrupt line from the IMU (tilt) and RTC. The IMU INT2 is configured as active-low open-drain, so a tilt event pulls the line low through the external pullup.
- **Power latch**: GPIO 17 must stay HIGH during deep sleep to keep the LDO enabled. `gpio_hold_en()` freezes the pin state before entering deep sleep. On wakeup, `gpio_hold_dis()` is called (via `unisolateStrappingPins()`) to regain control.
- **Current consumption**: In deep sleep, the ESP32-C6 draws ~5 µA. The IMU (LSM6DS3TR-C at 104 Hz with embedded functions) draws ~50 µA. Total system deep sleep current is dominated by the LDO quiescent current and IMU.
- **Boot time**: Full init takes ~200–300 ms from wakeup to LED display, which is perceptually instant for a wrist-raise gesture.

### RTC Time-Setting Operation

Setting the RTC involves writing BCD-encoded time registers over I2C:

1. Convert ISO 8601 timestamp (`YYYY-MM-DDTHH:MM:SS`) to BCD fields
2. Write 7 bytes starting at register 0x00 (seconds, minutes, hours, weekday, date, month, year)
3. Call `settimeofday()` to sync the ESP32 system clock

Current entry points:
- **Serial REST API**: `settime/2025-06-15T14:30:00` — parsed by the Raft framework, calls `_rtc.setDateTime()`
- **BLE CTS** (V10 only): Write to Current Time Service characteristic triggers the same RTC write path
- **Button-based** (proposed): Would call the same `_rtc.setDateTime()` after the user selects hour and minute via button presses
