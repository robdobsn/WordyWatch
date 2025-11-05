#!/usr/bin/env python3
"""
Generate a 12x11 charlieplexed LED matrix schematic for EasyEDA
Following the exact pattern from the template file
"""

import json
import copy
from typing import List, Dict, Tuple

# Configuration - following the pattern from template
NUM_PORTS = 12       # Number of GPIO ports
COL_SPACING = 145    # Horizontal spacing between columns (190 to 335 to 475)
LED_SPACING_Y = 90   # Vertical spacing between LEDs
START_X = 190        # Starting X position for first column
START_Y = -660       # Starting Y position for first LED
PORT_TOP_OFFSET_X = -35  # Port label X offset from column (-35 from 190 = 155)
PORT_TOP_Y = -700    # Y position for top port labels
PORT_RIGHT_OFFSET_Y = 35  # Y offset for right-side port labels from LED cathode

def load_template(filename: str) -> Dict:
    """Load the template EasyEDA JSON file"""
    with open(filename, 'r', encoding='utf-8') as f:
        return json.load(f)

def generate_unique_id(prefix: str, index: int) -> str:
    """Generate a unique ID for components"""
    import hashlib
    import random
    # Generate a unique hash-like ID similar to EasyEDA format
    unique_str = f"{prefix}{index}{random.randint(0, 99999)}"
    hash_val = hashlib.md5(unique_str.encode()).hexdigest()[:16]
    return f"gge{hash_val}"

def create_led(x: float, y: float, led_num: int) -> str:
    """Create an LED component (rotation 90 - anode up, cathode down)"""
    uid = generate_unique_id('led', led_num)
    uid1 = generate_unique_id('led', led_num + 1)
    uid2 = generate_unique_id('led', led_num + 2)
    uid3 = generate_unique_id('led', led_num + 3)
    uid4 = generate_unique_id('led', led_num + 4)
    uid5 = generate_unique_id('led', led_num + 5)
    uid6 = generate_unique_id('led', led_num + 6)
    uid7 = generate_unique_id('led', led_num + 7)
    uid8 = generate_unique_id('led', led_num + 8)
    uid9 = generate_unique_id('led', led_num + 9)
    uid10 = generate_unique_id('led', led_num + 10)
    uid11 = generate_unique_id('led', led_num + 11)
    uid12 = generate_unique_id('led', led_num + 12)
    
    # Rotation 270: Anode (pin 1) at top (y-20), Cathode (pin 2) at bottom (y+20)
    led_str = f"LIB~{x}~{y}~package`LED0402-FD_YELLOW`Contributor`lcsc`Supplier`LCSC`Supplier Part`C434450`Manufacturer`yongyu(永裕光电)`Manufacturer Part`SZYY0402Y`JLCPCB Part Class`Extended Part`spicePre`Z`spiceSymbolName`SZYY0402Y_C434450`~270~0~{uid}~15ae3cf5adbd445ca10066bc58786d84~b106fb803393431a9aa5596d75152a94~0~~yes~yes~4a184aa6289b47e29467d92858c05a4b~1706500802~387e8451b7aa40ab87922675a70d709c#@$T~N~{x+19}~{y+12.33}~0~#000080~Arial~~~~~comment~SZYY0402Y_C434450~1~start~{uid1}~0~#@$T~P~{x+19}~{y+3.26}~0~#000080~Arial~~~~~comment~Z{led_num}~1~start~{uid2}~0~#@$PT~M {x+13} {y+16} L {x+11} {y+12} L {x+9} {y+14} Z ~#880000~1~0~none~{uid3}~0~#@$PT~M {x+17} {y+12} L {x+15} {y+8} L {x+13} {y+10} Z ~#880000~1~0~none~{uid4}~0~#@$PL~{x+6} {y+9} {x+10} {y+13}~#880000~1~0~#880000~{uid5}~0#@$PL~{x+10} {y+5} {x+14} {y+9}~#880000~1~0~#880000~{uid6}~0#@$PL~{x} {y+5} {x} {y+10}~#880000~1~0~none~{uid7}~0#@$PL~{x} {y-10} {x} {y-5}~#880000~1~0~none~{uid8}~0#@$PL~{x+7} {y+5} {x-7} {y+5}~#880000~1~0~none~{uid9}~0#@$PT~M {x+7} {y-5} L {x} {y+5} L {x-7} {y-5} Z ~#880000~1~0~#FFFF00~{uid10}~0~#@$P~show~0~2~{x}~{y+20}~270~{uid11}~0^^{x}~{y+20}^^M {x} {y+20} v -10~#880000^^0~{x+3}~{y+7}~270~K~start~~~#0000FF^^0~{x-1}~{y+13}~270~2~end~~~#0000FF^^0~{x}~{y+13}^^0~M {x+3} {y+10} L {x} {y+7} L {x-3} {y+10}#@$P~show~0~1~{x}~{y-20}~90~{uid12}~0^^{x}~{y-20}^^M {x} {y-20} v 10~#880000^^0~{x+3}~{y-7}~270~A~end~~~#0000FF^^0~{x-1}~{y-13}~270~1~start~~~#0000FF^^0~{x}~{y-13}^^0~M {x-3} {y-10} L {x} {y-7} L {x+3} {y-10}"
    
    # Return LED string and pin positions
    # With rotation 270: Anode (pin 1) at top (y-20), Cathode (pin 2) at bottom (y+20)
    anode_pos = (x, y - 20)  # Pin 1 - top (anode)
    cathode_pos = (x, y + 20)  # Pin 2 - bottom (cathode)
    return led_str, anode_pos, cathode_pos

def create_port_label_top(x: float, y: float, port_name: str, index: int) -> str:
    """Create a port label at top (rotation 270)"""
    uid1 = generate_unique_id('port', index * 10)
    uid2 = generate_unique_id('port', index * 10 + 1)
    uid3 = generate_unique_id('port', index * 10 + 2)
    
    port_str = f"F~part_netLabel_netPort~{x}~{y}~270~{uid1}~~0^^{x}~{y}^^{port_name}~#0000FF~{x+3.6}~{y-22}~270~~1~Times New Roman~8pt~{uid2}^^PL~{x} {y} {x-5} {y-5} {x-5} {y-20} {x+5} {y-20} {x+5} {y-5} {x} {y}~#0000FF~1~0~transparent~{uid3}~0"
    return port_str

def create_port_label_right(x: float, y: float, port_name: str, index: int) -> str:
    """Create a port label on right side (rotation 180)"""
    uid1 = generate_unique_id('port', index * 10 + 3)
    uid2 = generate_unique_id('port', index * 10 + 4)
    uid3 = generate_unique_id('port', index * 10 + 5)
    
    port_str = f"F~part_netLabel_netPort~{x}~{y}~180~{uid1}~~0^^{x}~{y}^^{port_name}~#0000FF~{x+22}~{y+3.6}~0~~1~Times New Roman~8pt~{uid2}^^PL~{x} {y} {x+5} {y+5} {x+20} {y+5} {x+20} {y-5} {x+5} {y-5} {x} {y}~#0000FF~1~0~transparent~{uid3}~0"
    return port_str

def create_wire(x1: float, y1: float, x2: float, y2: float, *extra_points, index: int = 0) -> str:
    """Create a wire (can have multiple segments)"""
    uid = generate_unique_id('wire', index)
    points = [(x1, y1), (x2, y2)]
    if extra_points:
        # Insert extra points between first and last
        points = [(x1, y1)] + list(extra_points) + [(x2, y2)]
    
    coords = ' '.join([f"{x} {y}" for x, y in points])
    wire_str = f"W~{coords}~#008800~1~0~none~{uid}~0"
    return wire_str

def create_junction(x: float, y: float, index: int) -> str:
    """Create a junction point"""
    uid = generate_unique_id('junc', index)
    junc_str = f"J~{x}~{y}~2.5~#CC0000~{uid}~0"
    return junc_str

def generate_charlieplex_matrix(template: Dict, num_ports: int = 12) -> Dict:
    """
    Generate charlieplexed LED matrix - LEDs, port labels, vertical anode buses, and anode connections
    LEDs have rotation 90 (anode up, cathode down)
    Each column has one top anode port and cathode port labels to the right
    Vertical bus wires run from top port down through each column
    Horizontal wires connect each LED anode to the vertical bus
    """
    output = copy.deepcopy(template)
    
    # Clear existing shapes except the frame (first element)
    shapes = output['schematics'][0]['dataStr']['shape']
    frame = shapes[0]  # Keep the frame
    shapes.clear()
    shapes.append(frame)
    
    led_counter = 1
    wire_counter = 0
    junc_counter = 0
    
    # Port labels to the right of each LED
    port_label_offset_x = 50  # Distance to right of LED
    
    # Create LEDs and port labels column by column
    for col_idx in range(num_ports):
        anode_port_num = col_idx  # This column's anode port (0-indexed)
        col_x = START_X + (col_idx * COL_SPACING)
        
        # Create top port label for this column (anode port)
        port_top_x = col_x + PORT_TOP_OFFSET_X
        port_label = create_port_label_top(port_top_x, PORT_TOP_Y, f"P{anode_port_num+1}", anode_port_num)
        shapes.append(port_label)
        
        led_row = 0  # Position within this column
        
        # Create (num_ports - 1) LEDs in this column
        # Cathodes connect to all ports EXCEPT the anode port, in sequence
        cathode_port_sequence = []
        for p in range(num_ports):
            if p != anode_port_num:
                cathode_port_sequence.append(p)
        
        for led_in_col, cathode_port_num in enumerate(cathode_port_sequence):
            # Calculate LED position
            led_y = START_Y + (led_row * LED_SPACING_Y)
            
            # Create LED (rotation 90: anode at top, cathode at bottom)
            led_str, anode_pos, cathode_pos = create_led(col_x, led_y, led_counter)
            shapes.append(led_str)
            
            # Create horizontal wire from LED anode to vertical bus on the left
            # Wire goes from LED anode (col_x, anode_y) to vertical bus (port_top_x, anode_y)
            wire = create_wire(anode_pos[0], anode_pos[1], 
                             port_top_x, anode_pos[1],
                             index=wire_counter)
            shapes.append(wire)
            wire_counter += 1
            
            # Add junction at LED anode
            junc = create_junction(anode_pos[0], anode_pos[1], junc_counter)
            shapes.append(junc)
            junc_counter += 1
            
            # Add junction where wire meets vertical bus
            junc = create_junction(port_top_x, anode_pos[1], junc_counter)
            shapes.append(junc)
            junc_counter += 1
            
            # Place cathode port label at the cathode of this LED
            # Position so the tip (left point) of the label touches the cathode
            cathode_port_label_x = cathode_pos[0]
            cathode_port_label_y = cathode_pos[1]
            
            # Create cathode port label at cathode position
            port_label = create_port_label_right(cathode_port_label_x, cathode_port_label_y, 
                                                 f"P{cathode_port_num+1}", led_counter * 10)
            shapes.append(port_label)
            
            # Add junction at cathode to connect to port label
            junc = create_junction(cathode_pos[0], cathode_pos[1], junc_counter)
            shapes.append(junc)
            junc_counter += 1
            
            led_counter += 1
            led_row += 1
        
        # Create vertical anode bus wire for this column
        # Wire goes straight down from the port label (no horizontal movement)
        # Calculate bottom position - reach just below the last LED's anode in this column
        last_led_y = START_Y + ((num_ports - 2) * LED_SPACING_Y)  # Last LED center
        last_led_anode_y = last_led_y - 20  # Anode is at top of LED
        wire_bottom_y = last_led_anode_y + 10  # Extend slightly below last anode
        
        # Wire path: straight down from port label
        # Path: (port_top_x, PORT_TOP_Y) → (port_top_x, wire_bottom_y)
        wire = create_wire(port_top_x, PORT_TOP_Y, 
                          port_top_x, wire_bottom_y,
                          index=wire_counter)
        shapes.append(wire)
        wire_counter += 1
    
    return output

def main():
    """Main function"""
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python generate_charlieplex_v3.py <input_file> [output_file]")
        print("  input_file: Template EasyEDA JSON file")
        print("  output_file: Output JSON file (default: output_charlieplex_12x11.json)")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "output_charlieplex_12x11.json"
    
    print(f"Loading template from: {input_file}")
    template = load_template(input_file)
    
    print(f"Generating {NUM_PORTS}x{NUM_PORTS-1} charlieplexed LED matrix...")
    print(f"  - Column spacing: {COL_SPACING} units")
    print(f"  - LED spacing: {LED_SPACING_Y} units")
    print(f"  - Starting position: ({START_X}, {START_Y})")
    
    output = generate_charlieplex_matrix(template, NUM_PORTS)
    
    print(f"Writing output to: {output_file}")
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2)
    
    total_leds = NUM_PORTS * (NUM_PORTS - 1)
    print(f"Done! Generated {total_leds} LEDs with {NUM_PORTS} GPIO ports")

if __name__ == "__main__":
    main()

