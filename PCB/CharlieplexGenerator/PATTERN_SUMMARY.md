# Charlieplex Pattern Summary

## Pattern Analysis from Template

The template file `SCH_TestCharlieplex1_2025-10-31.json` establishes a specific charlieplex pattern with 3 ports (P1, P2, P3) and 3 LEDs per column.

### Key Measurements from Template

- **Column spacing**: 145 units (190 → 335 → 475)
- **LED spacing**: 90 units vertically (-660 → -570 → -475)
- **Starting position**: (190, -660)
- **Port label offset**: -35 units from column X position
- **Port label Y position**: -700 (40 units above first LED)

### Component Structure

#### 1. Port Labels (Top)
- **Position**: Column X - 35, Y = -700
- **Rotation**: 270° (vertical text)
- **Format**: P1, P2, P3, ... P12
- **Pattern**: One per column at the top

#### 2. LEDs
- **Position**: (Column X, Row Y)
- **Rotation**: 90° (all pointing down)
- **Designators**: LED1, LED2, LED3... LED132
- **Numbering**: Sequential across all columns

#### 3. Connections

**Anode (top pin of LED)**:
- Connected to vertical bus of its column
- Junction at anode position
- Bus runs from top port down through all LEDs

**Cathode (bottom pin of LED)**:
- Horizontal wire to another column's vertical bus
- Junction at target column
- Port label on right side (rotation 180°)

#### 4. Vertical Bus Wires
- **Path**: From port label → through all LED anodes in column
- **One per column** (12 total)
- **Connects**: Port at top to all LED anodes in that column

## Generated Matrix (12x11)

### Statistics

| Component | Count | Notes |
|-----------|-------|-------|
| LEDs | 132 | 12 columns × 11 LEDs |
| Port Labels | 144 | 12 top + 132 right side |
| Wires | 144 | 12 vertical buses + 132 horizontal cathode connections |
| Junctions | 264 | 132 anodes + 132 cathodes |
| **Total Shapes** | **685** | Including frame and all components |

### Matrix Layout

```
     P1      P2      P3      P4   ...   P11     P12
     |       |       |       |           |       |
    LED1    LED12   LED23   LED34      LED121  LED132
    →P2     →P1     →P1     →P1        →P1     →P1
     |       |       |       |           |       |
    LED2    LED13   LED24   LED35      LED122  LED133
    →P3     →P3     →P2     →P2        →P2     →P2
     |       |       |       |           |       |
    LED3    LED14   LED25   LED36      LED123  LED134
    →P4     →P4     →P4     →P3        →P3     →P3
     ...     ...     ...     ...         ...     ...
    LED11   LED22   LED33   LED44      LED131  LED143
    →P12    →P12    →P12    →P12       →P11    →P11
```

### Charlieplex Topology

**Column Structure**:
- Each column represents one GPIO port
- Each column has 11 LEDs (N-1 for N ports)
- All LEDs in a column share the same anode connection (vertical bus)

**LED Connections**:
- **Anode**: Connected to column's vertical bus (port)
- **Cathode**: Connected horizontally to a different port's bus
- Each LED connects between two different ports

**Example - Column P1**:
- LED1: Anode on P1, Cathode to P2
- LED2: Anode on P1, Cathode to P3
- LED3: Anode on P1, Cathode to P4
- ...
- LED11: Anode on P1, Cathode to P12

## File Dimensions

### Spatial Extent

- **Width**: ~1835 units (start at 190, rightmost port label at 1835)
- **Height**: ~1000 units (port at -700, last LED cathode around +300)
- **Columns**: 12 (spacing 145 units)
- **Rows per column**: 11 (spacing 90 units)

### Coordinate Ranges

- **X range**: 155 to 1855
  - 155: Leftmost port label (P1)
  - 190: First LED column
  - 1785: Last LED column (190 + 11×145)
  - 1835: Rightmost cathode port labels

- **Y range**: -700 to +310
  - -700: Top port labels
  - -660: First LED row
  - +300: Last LED row (-660 + 10×90)
  - +335: Bottom cathode port labels (+300 + 35)

## Usage

### Importing to EasyEDA

1. Open EasyEDA (https://easyeda.com/)
2. Go to **File → Open → EasyEDA Source**
3. Select `output_charlieplex_12x11_v3.json`
4. The 12×11 charlieplex matrix will appear

### Driving the Matrix

To light a specific LED:
1. Set the LED's anode port HIGH
2. Set the LED's cathode port LOW
3. Set all other ports to HIGH-Z (tri-state/input mode)
4. Only one LED (or set on same port pair) can be lit at once
5. Use multiplexing to scan through LEDs quickly

### Example Code Logic

```python
# To light LED1 (P1 anode, P2 cathode):
P1 = OUTPUT_HIGH
P2 = OUTPUT_LOW
P3-P12 = INPUT (high-Z)

# To light LED2 (P1 anode, P3 cathode):
P1 = OUTPUT_HIGH
P3 = OUTPUT_LOW
P2, P4-P12 = INPUT (high-Z)
```

## Files Generated

1. **generate_charlieplex_v3.py** - Generator script following template pattern
2. **output_charlieplex_12x11_v3.json** - Generated 12×11 matrix (280KB)
3. **PATTERN_SUMMARY.md** - This documentation

## Advantages of This Topology

1. **Clear visual structure** - Easy to trace connections
2. **Organized by port** - Each column represents one GPIO
3. **Matches reference designs** - Standard charlieplex wiring
4. **Efficient routing** - Minimal wire crossings
5. **Easy to debug** - Can trace each port's LEDs visually

## Next Steps

1. **Test in EasyEDA** - Import and verify the design
2. **Add microcontroller** - Connect to your MCU
3. **Generate PCB** - Convert schematic to PCB layout
4. **Write control code** - Implement multiplexing firmware
5. **Add current limiting** - Consider resistors if needed



