# Grid Analyzer Tool

This utility automatically analyzes letter grids and generates JSON layout files for the Word Clock Grace application.

## Usage

```bash
node tools/gridAnalyzer.cjs <input-grid-file> <output-json-name>
```

### Example

```bash
node tools/gridAnalyzer.cjs my-grid.txt custom-layout.json
```

## Input Format

Create a text file with your letter grid, one row per line:

```
HTFTTFFZERO
UEIWHOIFOUR
NNFEIRFTHIR
D.TNRTTHREE
R.ETTYYNINE
E.EYYELEVEN
D.NTWELVE..
TWENTYFIF..
EIGHTWOFIVE
SEVENSIXV..
ONETEN.TEEN
```

## Features

- **Automatic Word Detection**: Finds all time-related words (hours, minutes, military terms)
- **Bidirectional Search**: Detects words both horizontally and vertically
- **Smart Categorization**: Automatically categorizes words as hour, minute, or military terms
- **Duplicate Removal**: Eliminates duplicate word detections
- **JSON Generation**: Creates properly formatted layout files

## Word Lists

The analyzer searches for specific word fragments designed for composite time display:

### Horizontal Words (Hour fragments):
`THIR`, `SIX`, `THREE`, `EIGHT`, `ZERO`, `FIF`, `FIVE`, `TEN`, `NINE`, `FOUR`, `SEVEN`, `ONE`, `TWO`, `TEEN`

### Vertical Words (Minutes):
`TWENTY`, `FIVE`, `THIRTY`, `FORTY`, `FIFTY`, `FIFTEEN`, `HUNDRED`

### Composite Time Examples:
- **14:00** → "FOUR" + "TEEN" + "HUNDRED" 
- **15:00** → "FIF" + "TEEN" + "HUNDRED"
- **16:30** → "SIX" + "TEEN" + "THIRTY"
- **17:45** → "SEVEN" + "TEEN" + "FORTY" + "FIVE"

This approach allows for flexible time composition using word fragments rather than requiring complete words like "FOURTEEN".

## Output

The tool generates:
1. Console analysis showing found words and positions
2. JSON layout file in `public/layouts/` directory
3. Automatic integration with the Word Clock application

## Dynamic Layout Discovery

The application now automatically discovers available layout files in the `public/layouts/` folder, so new layouts created with this tool are immediately available in the layout dropdown.

## Example Output

```
=== GRID ANALYSIS ===
Grid size: 11 x 11

Horizontal Words:
  ZERO at (0, 7) - hour
  FOUR at (1, 7) - hour
  TWENTY at (7, 0) - minute

Vertical Words:
  HUNDRED at (0, 0) - military
  TEN at (0, 1) - minute

✅ Layout saved to: public/layouts/auto-layout.json
📊 Found 25 words total
```
