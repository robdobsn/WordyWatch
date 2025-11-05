# Charlieplex PCB Generator

## Overview

!!! Please don't use this script as it doesn't work well and has been retired in favour of using the array layout option in EasyEDA which works well.

This script (`generate_charlieplex_pcb.py`) attempts to generate EasyEDA PCB layouts for a 12x11 charlieplexed LED matrix.

## Features
- **132 LEDs** arranged in 12 columns of 11 LEDs each
- **Template-based** LED component generation using the actual LED footprint from the template
- **264 Vias** (2 per LED) placed at both ends of each LED
  - Vias positioned along LED centerline at 135° rotation
  - Anode via: offset (-1, +1) from LED center
  - Cathode via: offset (+1, -1) from LED center
- **No traces/tracks** generated (ready for manual routing or future automation)
- **Clear, repeatable pattern** with regular spacing
- **LED Component**: LED0402-FD_YELLOW (SZYY0402Y)
- **Component Prefix**: LEDG

## Usage
```bash
python generate_charlieplex_pcb.py <template_pcb.json> <output_pcb.json>
```

Example:
```bash
python generate_charlieplex_pcb.py PCB_PCB_TestCharlieplex1_2025-11-02.json output_pcb_12x11.json
```

## PCB Layout Pattern

### LED Placement
- **Spacing**: 11.811mm horizontally, 9.8425mm vertically
- **Rotation**: 135 degrees
- **Starting Position**: (4031.811, 3425.571)

### Via Placement
Each LED has two vias placed along its centerline:

1. **Anode Via**:
   - Positioned at LED center + (-1, +1)
   - Example: LED at (3, 17) → via at (2, 18)
   - Connected to anode port net (P1-P12 depending on column)

2. **Cathode Via**:
   - Positioned at LED center + (+1, -1)
   - Example: LED at (3, 17) → via at (4, 16)
   - Connected to cathode port net (varies per LED in charlieplex pattern)

### Net Naming
- **Anode nets**: P1, P2, P3, ... P12 (one per column)
- **Cathode nets**: Distributed across all ports following charlieplex pattern

### Routing
- **No traces/tracks are generated** by this script
- Vias are ready for manual routing or automated routing tools
- Suggested routing: vertical anode buses, horizontal cathode connections

## Charlieplex Matrix Organization

For a 12-port charlieplex:
- **Column 1** (Anode = P1): Cathodes connect to P2, P3, P4, ... P12
- **Column 2** (Anode = P2): Cathodes connect to P1, P3, P4, ... P12
- **Column 3** (Anode = P3): Cathodes connect to P1, P2, P4, ... P12
- And so on...

Each column has N-1 LEDs where N is the number of ports (12).

## Configuration
You can modify these constants in the script:
- `LED_SPACING_X`: Horizontal spacing (default: 11.811)
- `LED_SPACING_Y`: Vertical spacing (default: 9.8425)
- `START_X`: Starting X coordinate (default: 4031.811)
- `START_Y`: Starting Y coordinate (default: 3425.571)
- `LED_ROTATION`: LED rotation angle (default: 135)

## Output
The script generates an EasyEDA PCB JSON file that can be imported directly into EasyEDA.

The output includes:
- **132 LED components** positioned in a grid
- **264 vias** (2 per LED) with proper net assignments
- **Board outline** automatically sized for the LED matrix
- **No traces/tracks** - ready for your routing strategy

## Notes
- The script uses the LED component from the template file, ensuring proper footprint geometry
- All unique IDs are regenerated to avoid conflicts
- Via positions are calculated based on LED centerline at 135° rotation
- Vias are properly assigned to their respective port nets (P1-P12)
- The pattern provides a clear, organized starting point for PCB routing

