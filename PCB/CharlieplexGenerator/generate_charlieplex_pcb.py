#!/usr/bin/env python3
"""
Generate EasyEDA PCB JSON for a charlieplexed LED matrix
Based on the provided template PCB design
"""

import json
import sys
import hashlib
import copy
import re
from typing import Dict, Tuple, List

# PCB Layout Configuration
# EasyEDA PCB units: 1 unit = 0.254mm (10 mils)
# To convert mm to PCB units: mm / 0.254
MM_TO_PCB_UNITS = 1.0 / 0.254  # Approximately 3.937

LED_SPACING_X_MM = 2.5          # Horizontal spacing in millimeters
LED_SPACING_Y_MM = 3.0          # Vertical spacing in millimeters
LED_SPACING_X = LED_SPACING_X_MM * MM_TO_PCB_UNITS  # Convert to PCB units
LED_SPACING_Y = LED_SPACING_Y_MM * MM_TO_PCB_UNITS  # Convert to PCB units

START_X = 4031.811      # Starting X position (PCB units)
START_Y = 3425.571      # Starting Y position (PCB units)
LED_ROTATION = 135      # LED rotation angle

def generate_unique_id(prefix: str, seed: int) -> str:
    """Generate a unique ID similar to EasyEDA format"""
    hash_input = f"{prefix}_{seed}".encode('utf-8')
    hash_val = hashlib.md5(hash_input).hexdigest()
    return f"gge{hash_val[:15]}"

def extract_led_template(template: Dict) -> str:
    """Extract the first LED component from the template to use as a base"""
    for shape in template['shape']:
        if isinstance(shape, str) and shape.startswith('LIB~') and 'LED0402-FD_YELLOW' in shape:
            return shape
    return None

def extract_leds_from_schematic(schematic_file: str) -> List[Dict]:
    """Extract LED information from the schematic JSON
    Returns list of dicts with: led_num, anode_port, cathode_port"""
    try:
        with open(schematic_file, 'r', encoding='utf-8') as f:
            schematic = json.load(f)
        
        leds = []
        shapes = schematic['schematics'][0]['dataStr']['shape']
        
        for shape in shapes:
            if isinstance(shape, str) and 'LIB~' in shape and 'LED0402-FD_YELLOW' in shape:
                # Extract LED number from the component label (Z1, Z2, etc.)
                # Look for the comment field with Z#
                if '~Z' in shape:
                    # Find the Z# label
                    parts = shape.split('~')
                    led_label = None
                    for part in parts:
                        if part.startswith('Z') and part[1:].isdigit():
                            led_label = part
                            break
                    
                    if led_label:
                        led_num = int(led_label[1:])
                        # Store LED info - we'll determine ports from matrix pattern
                        leds.append({'led_num': led_num})
        
        # Sort by LED number
        leds.sort(key=lambda x: x['led_num'])
        
        # Now assign anode and cathode ports based on charlieplex pattern
        num_ports = 12
        for led_info in leds:
            led_num = led_info['led_num']
            # Calculate which column this LED is in (0-indexed)
            col_idx = (led_num - 1) // (num_ports - 1)
            anode_port = col_idx
            
            # Calculate position within column
            led_in_col = (led_num - 1) % (num_ports - 1)
            
            # Cathode port sequence: all ports except the anode port
            cathode_sequence = [p for p in range(num_ports) if p != anode_port]
            cathode_port = cathode_sequence[led_in_col]
            
            led_info['anode_port'] = anode_port + 1  # 1-indexed (P1, P2, etc.)
            led_info['cathode_port'] = cathode_port + 1  # 1-indexed
        
        return leds
    except Exception as e:
        print(f"Warning: Could not extract LEDs from schematic: {e}")
        return []

def create_led_component(x: float, y: float, led_num: int, anode_port: int, cathode_port: int, template_led: str, template_x: float, template_y: float) -> str:
    """Create an LED component at position (x, y) by modifying the template
    This properly offsets ALL coordinates within the LED component"""
    if not template_led:
        # Fallback simplified version
        uid = generate_unique_id('led', led_num * 100)
        return f"LIB~{x}~{y}~package`LED0402-FD_YELLOW`~{LED_ROTATION}~~{uid}~2~~0~~yes~~"
    
    # Calculate offset from template position
    offset_x = x - template_x
    offset_y = y - template_y
    
    # Replace component prefix and label
    led_str = template_led.replace('spicePre`U`', 'spicePre`Z`')
    led_str = led_str.replace('~U1~', f'~Z{led_num}~')
    led_str = led_str.replace('~U2~', f'~Z{led_num}~')
    
    # Update PAD net assignments
    # PAD format: PAD~RECT~x~y~...~net~pad_num~...
    # Need to replace P1 and P2 with the correct anode and cathode port nets
    led_str = re.sub(r'(PAD~[^~]+~[^~]+~[^~]+~[^~]+~[^~]+~[^~]+~)P2(~2~)', 
                     rf'\1P{anode_port}\2', led_str)
    led_str = re.sub(r'(PAD~[^~]+~[^~]+~[^~]+~[^~]+~[^~]+~[^~]+~)P1(~1~)', 
                     rf'\1P{cathode_port}\2', led_str)
    
    # Generate new unique IDs
    id_pattern = r'gge[a-f0-9]+'
    counter = 0
    def replace_id(match):
        nonlocal counter
        counter += 1
        return generate_unique_id(f'led{led_num}', counter)
    led_str = re.sub(id_pattern, replace_id, led_str)
    
    # Now offset all numeric coordinates in the string
    # Pattern to find floating point numbers (coordinates)
    def offset_coordinate(match, is_x_coord):
        """Offset a coordinate value"""
        value = float(match.group(0))
        # Only offset if the value is near the template position (within reasonable range)
        # This avoids offsetting things like rotation angles, sizes, etc.
        if is_x_coord and abs(value - template_x) < 100:
            return str(value + offset_x)
        elif not is_x_coord and abs(value - template_y) < 100:
            return str(value + offset_y)
        return match.group(0)
    
    # Split by ~ to process each part
    parts = led_str.split('~')
    # Update the main position (parts 1 and 2)
    if len(parts) > 2:
        parts[1] = str(x)
        parts[2] = str(y)
    
    # Rejoin and process internal coordinates
    led_str = '~'.join(parts)
    
    # Find all coordinate pairs in the format "number number" (space separated)
    # This is used for things like "M 4031.811 3425.571 L ..."
    coord_pattern = r'(\d+\.?\d*)\s+(\d+\.?\d*)'
    def offset_coord_pair(match):
        x_val = float(match.group(1))
        y_val = float(match.group(2))
        # Only offset if near template position
        if abs(x_val - template_x) < 100 and abs(y_val - template_y) < 100:
            return f'{x_val + offset_x} {y_val + offset_y}'
        return match.group(0)
    
    led_str = re.sub(coord_pattern, offset_coord_pair, led_str)
    
    return led_str

def generate_charlieplex_pcb(template: Dict, led_info_list: List[Dict], num_ports: int = 12) -> Dict:
    """
    Generate charlieplexed LED matrix PCB layout
    - LEDs arranged in columns (one column per anode port)
    - No vias or traces generated
    """
    output = copy.deepcopy(template)
    
    # Extract LED template and its position
    template_led = extract_led_template(template)
    if not template_led:
        print("Warning: Could not find LED template in PCB file")
        return output
    
    # Extract template LED position
    template_parts = template_led.split('~')
    template_x = float(template_parts[1])
    template_y = float(template_parts[2])
    print(f"  - Template LED at ({template_x}, {template_y})")
    
    # Clear existing shapes except board outline
    shapes = output['shape']
    board_outline = [s for s in shapes if 'BoardOutLine' in str(s)]
    shapes.clear()
    if board_outline:
        shapes.extend(board_outline)
    
    # Calculate PCB dimensions
    pcb_width = (num_ports - 1) * LED_SPACING_X + 20  # Extra margin
    pcb_height = (num_ports - 1) * LED_SPACING_Y + 20  # Extra margin
    
    # Create board outline
    outline_x = START_X - 10
    outline_y = START_Y - 10
    outline = f"TRACK~1~10~~{outline_x} {outline_y} {outline_x + pcb_width} {outline_y} {outline_x + pcb_width} {outline_y + pcb_height} {outline_x} {outline_y + pcb_height} {outline_x} {outline_y}~{generate_unique_id('outline', 0)}~0"
    shapes.append(outline)
    
    # Create LEDs using information from schematic
    for led_info in led_info_list:
        led_num = led_info['led_num']
        anode_port = led_info['anode_port']  # 1-indexed (P1, P2, etc.)
        cathode_port = led_info['cathode_port']  # 1-indexed
        
        # Calculate position based on LED number
        # LEDs are arranged in columns by anode port
        col_idx = anode_port - 1  # 0-indexed for calculation
        led_in_col = (led_num - 1) % (num_ports - 1)
        
        col_x = START_X + (col_idx * LED_SPACING_X)
        led_y = START_Y + (led_in_col * LED_SPACING_Y)
        
        # Create LED component with proper coordinate offsetting
        led = create_led_component(col_x, led_y, led_num, anode_port, cathode_port, 
                                  template_led, template_x, template_y)
        shapes.append(led)
    
    # Update BBox
    output['BBox'] = {
        "x": outline_x,
        "y": outline_y,
        "width": pcb_width,
        "height": pcb_height
    }
    
    return output

def main():
    if len(sys.argv) != 4:
        print("Usage: python generate_charlieplex_pcb.py <schematic.json> <template_pcb.json> <output_pcb.json>")
        print("  schematic.json: The generated schematic with LED definitions")
        print("  template_pcb.json: PCB template with LED footprint")
        print("  output_pcb.json: Output PCB file")
        sys.exit(1)
    
    schematic_file = sys.argv[1]
    template_file = sys.argv[2]
    output_file = sys.argv[3]
    
    print(f"Loading schematic from: {schematic_file}")
    led_info_list = extract_leds_from_schematic(schematic_file)
    if not led_info_list:
        print("ERROR: No LEDs found in schematic!")
        sys.exit(1)
    print(f"  - Found {len(led_info_list)} LEDs in schematic")
    
    print(f"Loading PCB template from: {template_file}")
    with open(template_file, 'r', encoding='utf-8') as f:
        template = json.load(f)
    
    print("Generating 12x11 charlieplexed LED matrix PCB...")
    print(f"  - LED spacing: {LED_SPACING_X_MM}mm x {LED_SPACING_Y_MM}mm ({LED_SPACING_X:.3f} x {LED_SPACING_Y:.3f} PCB units)")
    print(f"  - Starting position: ({START_X}, {START_Y})")
    print(f"  - LED rotation: {LED_ROTATION} degrees")
    print(f"  - Component prefix: Z")
    print(f"  - No vias or traces generated")
    print(f"  - Unit conversion: 1mm = {MM_TO_PCB_UNITS:.3f} PCB units")
    
    pcb = generate_charlieplex_pcb(template, led_info_list, num_ports=12)
    
    print(f"Writing output to: {output_file}")
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(pcb, f, ensure_ascii=False, indent=2)
    
    print(f"Done! Generated {len(led_info_list)} LEDs")

if __name__ == '__main__':
    main()

