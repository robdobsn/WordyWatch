# Charlieplex LED Matrix Generator for EasyEDA

The `generate_charlieplex_sch.py` script generates a charlieplexed LED matrix schematic design for EasyEDA.

## Scripts in this Directory

- **`generate_charlieplex_sch.py`** - ✅ **Use this script** - Generates EasyEDA schematics for charlieplexed LED matrices
- **`generate_charlieplex_pcb.py`** - ❌ Deprecated/non-functional - Do not use
- **`verify_pcb_labels.py`** - Helper script for PCB verification

For PCB generation information, see `PCB_GENERATOR_README.md`.

## Overview

The script creates a 12x11 charlieplexed LED matrix (132 LEDs) that can be controlled with 12 GPIO lines. Charlieplexing is a technique that allows controlling N×(N-1) LEDs using only N GPIO pins by taking advantage of the tri-state capability of microcontroller pins.

## How Charlieplexing Works

- With 12 GPIO lines, you can control 12×11 = 132 LEDs
- Each LED is connected between two different GPIO lines
- Only one LED (or one set of LEDs on the same line pair) can be lit at a time
- By rapidly multiplexing, you can create the illusion of multiple LEDs being lit simultaneously

## Usage

```bash
python generate_charlieplex_sch.py <input_file> [output_file]
```

### Parameters:
- `input_file`: Template EasyEDA JSON file (e.g., SCH_TestCharlieplex1_2025-10-31.json)
- `output_file`: Output JSON file (default: output_charlieplex.json)

### Example:
```bash
python generate_charlieplex_sch.py SCH_TestCharlieplex1_2025-10-31.json output_charlieplex.json
```

## Configuration

You can modify these constants at the top of the script to customize the layout:

- `NUM_LINES`: Number of GPIO lines (default: 12)
- `LED_SPACING_X`: Horizontal spacing between LEDs (default: 125)
- `LED_SPACING_Y`: Vertical spacing between LEDs (default: 70)
- `START_X`: Starting X position for LED matrix (default: 200)
- `START_Y`: Starting Y position for LED matrix (default: -100)
- `PORT_SPACING`: Vertical spacing between port labels (default: 70)
- `PORT_X`: X position for port labels (default: 100)
- `PORT_START_Y`: Starting Y position for port labels (default: -100)

## Generated Design

The script generates:
- **12 GPIO net ports** (GPIO0 through GPIO11) positioned at the top
- **132 LEDs** arranged in 12 vertical columns (11 LEDs per column)
- **Vertical bus lines** for each GPIO (running through all LEDs in that column)
- **Horizontal wires** connecting each LED's cathode to other GPIO columns
- **Junction points** at all connection intersections
- **Net labels** on cathode connections for easy debugging

### LED Matrix Layout

The charlieplex topology follows this structure:
- **Vertical columns**: Each GPIO has its own vertical column with (N-1) LEDs
- **Anode connections**: All LEDs in a column connect their anodes to that column's vertical bus (GPIO line)
- **Cathode connections**: Each LED's cathode connects horizontally to a different GPIO's vertical bus

For example, in the GPIO0 column:
- LED at position 0 has cathode connected to GPIO1
- LED at position 1 has cathode connected to GPIO2
- LED at position 2 has cathode connected to GPIO3
- And so on...

LED orientation:
- **All LEDs point down (Rotation 90°)**: Anode at top, cathode at bottom

## LED Component

The script uses the white LED component from LCSC:
- **Part Number**: XL-1608UWC-04
- **Package**: LED0603-RD_WHITE
- **Manufacturer**: XINGLIGHT(成兴光)
- **LCSC Part**: C965808

## Future Enhancements

Potential improvements for future versions:
- Add the charlieplexed components to an existing design (merge mode)
- Support different LED packages
- Add optional current limiting resistors
- Generate different matrix sizes
- Add visual grouping or color coding
- Export to other EDA formats

## File Structure

The generated JSON file follows the EasyEDA schematic format:
- Top-level: `editorVersion`, `docType`, `title`, `schematics`
- Schematic sheet: `dataStr` containing `head`, `canvas`, `shape`, `BBox`
- Shape array: Contains all components, wires, labels, and graphical elements

## Notes

- The design preserves the original template's frame and title block
- All components have unique IDs to prevent conflicts
- Net labels help with debugging and understanding the connections
- The horizontal bus lines simplify routing and make the design cleaner

## Importing into EasyEDA

1. Open EasyEDA (https://easyeda.com/)
2. Go to File → Open → EasyEDA Source
3. Select the generated JSON file
4. The charlieplexed LED matrix will appear in the editor

You can then:
- Add additional components (microcontroller, connectors, etc.)
- Route connections to your microcontroller
- Generate a PCB layout
- Order the PCB

## License

This script is provided as-is for educational and personal use.

