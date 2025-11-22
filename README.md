# WordyWatch

A wearable word clock watch that displays time using military time format in words. Inspired by [QlockTwo](https://www.qlocktwo.com/en-us) word clocks, WordyWatch brings this elegant time display concept to a wrist-worn device.

## Overview

WordyWatch displays time using illuminated words instead of traditional digits. Instead of showing "11:05", it displays "eleven oh five hours". The watch uses a charlieplexed LED matrix to light up words on a custom PCB, creating a unique and readable time display.

## Military Time Format

The watch displays time in military/24-hour format with 5-minute intervals:

- `00:00` → "zero hundred hours"
- `00:05` → "zero oh five hours"  
- `11:00` → "eleven hundred hours"
- `11:05` → "eleven oh five hours"
- `23:55` → "twenty three fifty five hours"

## Key Features

- **Word-based time display** with charlieplexed LED matrix
- **Military time format** for unambiguous time reading
- **5-minute intervals** for practical readability
- **Compact wearable design** optimized for wrist-worn use
- **Custom PCB** with integrated word layout
- **ESP32-C6 based** firmware for reliable operation

## Project Structure

### 📱 **WordyWatchWebApp/**
React-based web application for designing and testing word clock layouts. This tool allows you to:
- Visualize different word arrangements and grid layouts
- Test military time display with various times
- Customize fonts and spacing
- Export layouts for use in firmware
- Experiment with new word patterns before manufacturing

Built with React 18, TypeScript, Vite, and Tailwind CSS.

See [WordyWatchWebApp/README.md](WordyWatchWebApp/README.md) for detailed usage instructions.

### 🔧 **WordyFW/**
ESP32-C6 firmware for the WordyWatch hardware. Handles:
- Real-time clock management
- LED matrix control via charlieplexing
- Power management for battery operation
- Word selection and timing logic
- User interface and button controls

### 🔌 **PCB/**
Custom PCB designs and tools for the WordyWatch hardware:

#### CharlieplexGenerator/
Python scripts for generating charlieplexed LED matrix layouts in EasyEDA format:
- `generate_charlieplex_sch.py` - Generates schematics for 12×11 LED matrices (132 LEDs)
- Automatic routing and connection generation
- Supports customizable grid spacing and positioning

See [PCB/CharlieplexGenerator/README.md](PCB/CharlieplexGenerator/README.md) for details.

### 🎨 **Design/**
Design files, documentation, and optimization experiments:
- Grid layout optimization attempts
- Word pattern analysis
- Design specifications and requirements
- Visual mockups and concepts

## Technology Stack

- **Hardware**: ESP32-C6, charlieplexed LED matrix (132 LEDs from 12 GPIO lines)
- **Firmware**: C/C++ with ESP-IDF
- **PCB Design**: EasyEDA with custom Python generators
- **Web Tools**: React 18 + TypeScript + Vite
- **LED Components**: XL-1608UWC-04 white LEDs (LCSC C965808)

## Getting Started

### For Web App Development
```bash
cd WordyWatchWebApp
npm install
npm run dev
```

### For PCB Generation
```bash
cd PCB/CharlieplexGenerator
python generate_charlieplex_sch.py SCH_TestCharlieplex1_2025-11-02.json output.json
```

### For Firmware Development
```bash
cd WordyFW
# Follow ESP-IDF build instructions
```

## How Charlieplexing Works

WordyWatch uses charlieplexing to control 132 LEDs with only 12 GPIO pins:
- With N GPIO lines, you can control N×(N-1) LEDs
- Each LED is connected between two different GPIO lines
- Only one LED (or set on the same line pair) lights at a time
- Rapid multiplexing creates the illusion of multiple LEDs being lit simultaneously

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is open source and available under the MIT License. See [LICENSE](LICENSE) for details.

Copyright (c) 2025 Rob Dobson

## Acknowledgments

- Inspired by [QlockTwo](https://www.qlocktwo.com/en-us) word clocks
- Built for experimentation with charlieplexing and wearable displays




