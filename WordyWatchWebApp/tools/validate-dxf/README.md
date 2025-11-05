# DXF Validation Tool

A Python tool for validating DXF files and providing detailed error reporting to help debug DXF generation issues.

## Installation

1. Install Python 3.7 or higher
2. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

### Basic validation:
```bash
python validate_dxf.py file.dxf
```

### Verbose output:
```bash
python validate_dxf.py -v file.dxf
```

### Save detailed report:
```bash
python validate_dxf.py --output report.txt file.dxf
```

## What it validates

- **File structure**: Checks if file exists and is readable
- **DXF format**: Validates DXF structure and version
- **Sections**: Verifies presence of required sections (HEADER, TABLES, BLOCKS, ENTITIES, OBJECTS)
- **Tables**: Checks for standard tables (layers, styles, views, etc.)
- **Entities**: Validates TEXT, LINE, and LWPOLYLINE entities
- **Handles**: Ensures all entity handles are unique and valid
- **Geometry**: Validates entity properties and geometry

## Output

The tool provides:
- ✅ Success indicators for valid components
- ❌ Error messages for critical issues
- ⚠️ Warning messages for potential problems
- Detailed entity counts and structure analysis
- Optional detailed report file

## Common DXF Issues

- **Missing sections**: DXF files need HEADER, TABLES, BLOCKS, ENTITIES, and OBJECTS sections
- **Invalid handles**: Each entity must have a unique, valid handle
- **Malformed entities**: TEXT entities need proper text content and positioning
- **Unclosed polylines**: LWPOLYLINE entities marked as closed must actually be closed
- **Missing tables**: Some CAD software requires specific tables to be present

## Examples

### Valid DXF:
```
✓ Successfully loaded DXF version: R2013
✓ No errors found
✓ No warnings found
```

### Invalid DXF:
```
✗ DXF Structure Error: Missing HEADER section
❌ ERRORS (1):
  1. DXF Structure Error: Missing HEADER section
```

