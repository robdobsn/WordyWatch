// DXF Export utility for word clock grid
// Browser-safe: generates DXF as plain text and triggers download via Blob
// Uses vector font paths for precise rendering

import { ClockLayout } from '../types/layout';
import { getFont, extractGlyphPath, pathToDXFEntities, createFallbackFont, createDXFPolyline } from './fontPathExtractor';

// Create a letter grid from layout data (reusing logic from ClockDisplay.tsx)
function createLetterGrid(layout: ClockLayout): string[][] {
  const grid: string[][] = Array(layout.gridHeight)
    .fill(null)
    .map(() => Array(layout.gridWidth).fill(' '));
  
  // Place words in the grid
  layout.words.forEach((wordPos) => {
    const { word, startRow, startCol, direction } = wordPos as any;
    
    for (let i = 0; i < word.length; i++) {
      const row = direction === 'horizontal' ? startRow : startRow + i;
      const col = direction === 'horizontal' ? startCol + i : startCol;
      
      if (row >= 0 && row < layout.gridHeight && col >= 0 && col < layout.gridWidth) {
        grid[row][col] = word[i];
      }
    }
  });
  
  return grid;
}

// DXF export configuration
interface DXFConfig {
  letterSize: number;        // DEPRECATED in fit-to-cell mode
  gridSpacing: number;       // X spacing between grid cells (mm)
  gridSpacingY?: number;     // Y spacing between grid cells (mm)
  margin: number;            // Margin around the entire grid
  fontName: string;          // Font name for path extraction
  addBorder: boolean;        // Draw a border rectangle
  addGridLines: boolean;     // Draw grid lines
  useVectorPaths: boolean;   // Use vector font paths instead of text entities
  testMode: boolean;         // Test mode - output simple shapes only
  letterPaddingPercent?: number; // Per-side margin as % of cell height
  horizontalStretch?: number;    // Horizontal stretch factor (default 1.0)
  wStretch?: number;             // Additional stretch factor for W character (default 1.0)
  centerHorizontally?: boolean;  // Center letters horizontally in cells (default true)
}

const defaultConfig: DXFConfig = {
  letterSize: 10,
  gridSpacing: 2.5,
  gridSpacingY: 3,
  margin: 2,
  fontName: 'Ruler Stencil Regular',
  addBorder: true,
  addGridLines: false,
  useVectorPaths: true,
  testMode: false,
  letterPaddingPercent: 0.1,
  horizontalStretch: 1.55,
  wStretch: 0.9,
  centerHorizontally: true,
};

// Entities-only DXF (the variant that worked in Fusion 360)
function buildMinimalDXF(entities: string[]): string {
  return [
    '0',
    'SECTION',
    '2',
    'ENTITIES',
    ...entities,
    '0',
    'ENDSEC',
    '0',
    'EOF'
  ].join('\n');
}

// Full DXF builder for production - compatible with Fusion 360 and AutoCAD
function buildFullDXF(entities: string[], styles: string[] = []): string {
  const header = [
    '0','SECTION',
    '2','HEADER',
    '9','$ACADVER', '1','AC1027', // R2013
    '9','$HANDSEED', '5','FFFF', // Handle seed
    '9','$MEASUREMENT', '70','0', // English units
    '9','$INSUNITS', '70','4', // Millimeters
    '9','$DWGCODEPAGE', '3','ANSI_1252', // Code page
    '9','$EXTMIN', '10','0.0','20','0.0','30','0.0', // Extents minimum
    '9','$EXTMAX', '10','1000.0','20','1000.0','30','0.0', // Extents maximum
    '0','ENDSEC',
    '0','SECTION',
    '2','TABLES',
    // LAYER table
    '0','TABLE','2','LAYER','70','1',
    '0','LAYER','2','0','70','0','6','CONTINUOUS','62','7',
    '0','ENDTAB',
    // STYLE table
    '0','TABLE','2','STYLE','70','1',
    '0','STYLE','2','STANDARD','70','0','40','0','41','1','50','0','71','0','42','0','3','txt','4','',
    // Additional styles
    ...styles,
    '0','ENDTAB',
    // VIEW table (required by some CAD software)
    '0','TABLE','2','VIEW','70','0',
    '0','ENDTAB',
    // UCS table
    '0','TABLE','2','UCS','70','0',
    '0','ENDTAB',
    // APPID table
    '0','TABLE','2','APPID','70','1',
    '0','APPID','2','ACAD','70','0',
    '0','ENDTAB',
    '0','ENDSEC',
    '0','SECTION','2','ENTITIES'
  ].join('\n');

  const footer = [
    '0','ENDSEC',
    '0','SECTION','2','BLOCKS',
    '0','ENDSEC',
    '0','SECTION','2','OBJECTS',
    '0','DICTIONARY','2','ACAD_GROUP',
    '0','DICTIONARY','2','ACAD_MLINESTYLE',
    '0','ENDSEC',
    '0','EOF'
  ].join('\n');
  
  return `${header}\n${entities.join('\n')}\n${footer}`;
}

function dxfText(x: number, y: number, height: number, text: string, handle: string): string {
  return [
    '0','TEXT',
    '5', handle,      // handle
    '8','0',          // layer 0
    '100','AcDbEntity', // subclass marker
    '100','AcDbText',   // subclass marker for TEXT
    '10', x.toString(), // insertion point X
    '20', y.toString(), // insertion point Y
    '30','0',         // insertion point Z
    '40', height.toString(), // text height
    '1', text,        // text content
    '7', 'STANDARD',  // Always use STANDARD style for compatibility
    '72','1',         // horizontal alignment: centered (1)
    '73','2',         // vertical alignment: middle (2)
    '11', x.toString(), // alignment point X (center point)
    '21', y.toString(), // alignment point Y (center point)
    '31','0',         // alignment point Z
    '50','0',         // rotation angle
  ].join('\n');
}

function dxfLine(x1: number, y1: number, x2: number, y2: number, handle: string): string {
  return [
    '0','LINE',
    '5', handle,      // handle
    '8','0',          // layer
    '100','AcDbEntity', // subclass marker
    '100','AcDbLine',   // subclass marker for LINE
    '10', x1.toString(), '20', y1.toString(), '30','0',
    '11', x2.toString(), '21', y2.toString(), '31','0'
  ].join('\n');
}

export async function exportGridToDXF(layout: ClockLayout, filename: string, config: Partial<DXFConfig> = {}): Promise<void> {
  try {
    console.log('Starting DXF export...');
    const finalConfig = { ...defaultConfig, ...config };
    console.log('Final config:', finalConfig);
    const grid = createLetterGrid(layout);
    console.log('Grid created, size:', grid.length, 'x', grid[0]?.length);

  const spacingX = finalConfig.gridSpacing;
  const spacingY = finalConfig.gridSpacingY ?? finalConfig.gridSpacing;
  const totalWidth = (layout.gridWidth * spacingX) + (2 * finalConfig.margin);
  const totalHeight = (layout.gridHeight * spacingY) + (2 * finalConfig.margin);

  const entities: string[] = [];
  const handleCounter = { value: 10 }; // Use object to pass by reference

  // Optional grid lines
  if (finalConfig.addGridLines) {
    // Vertical lines
    for (let col = 0; col <= layout.gridWidth; col++) {
      const x = finalConfig.margin + (col * spacingX);
      const handle = handleCounter.value.toString(16).toUpperCase().padStart(4, '0');
      entities.push(dxfLine(x, finalConfig.margin, x, totalHeight - finalConfig.margin, handle));
      handleCounter.value++;
    }
    // Horizontal lines
    for (let row = 0; row <= layout.gridHeight; row++) {
      const y = finalConfig.margin + (row * spacingY);
      const handle = handleCounter.value.toString(16).toUpperCase().padStart(4, '0');
      entities.push(dxfLine(finalConfig.margin, y, totalWidth - finalConfig.margin, y, handle));
      handleCounter.value++;
    }
  }

  // Test mode - create simple shapes
  if (finalConfig.testMode) {
    console.log('TEST MODE: Creating simple test shapes');
    await addTestShapes(entities, finalConfig, totalHeight, handleCounter);
  }
  // Letters - use vector paths or text entities
  else if (finalConfig.useVectorPaths) {
    try {
      console.log(`Attempting to load font: ${finalConfig.fontName}`);
      // Load the font for path extraction
      const font = await getFont(finalConfig.fontName);
      if (!font) {
        console.warn(`Font ${finalConfig.fontName} not available, using fallback font for vector paths`);
        console.log('Font loaded successfully');
        // Use fallback font for vector paths instead of text entities
        const fallbackFont = createFallbackFont();
        await addVectorPaths(entities, grid, layout, finalConfig, totalHeight, fallbackFont, handleCounter);
      } else {
        console.log(`Successfully loaded font: ${finalConfig.fontName}`);
        // Use vector paths
        await addVectorPaths(entities, grid, layout, finalConfig, totalHeight, font, handleCounter);
      }
    } catch (error) {
      console.error('Error with vector paths, using fallback font:', error);
      const fallbackFont = createFallbackFont();
      await addVectorPaths(entities, grid, layout, finalConfig, totalHeight, fallbackFont, handleCounter);
    }
  } else {
    console.log('Vector paths disabled - no entities will be created');
    // Skip letter generation if vector paths are disabled
  }

  // Border - draw rectangle around the grid area
  if (finalConfig.addBorder) {
    // Border should align with the outer edges of the grid cells
    // Grid spans from margin to (margin + gridWidth*spacingX) in X
    // and from margin to (margin + gridHeight*spacingY) in Y
    const x = finalConfig.margin;
    const y = finalConfig.margin;
    const w = layout.gridWidth * spacingX;
    const h = layout.gridHeight * spacingY;
    
    const handle1 = handleCounter.value.toString(16).toUpperCase().padStart(4, '0');
    handleCounter.value++;
    const handle2 = handleCounter.value.toString(16).toUpperCase().padStart(4, '0');
    handleCounter.value++;
    const handle3 = handleCounter.value.toString(16).toUpperCase().padStart(4, '0');
    handleCounter.value++;
    const handle4 = handleCounter.value.toString(16).toUpperCase().padStart(4, '0');
    handleCounter.value++;
    
    // Draw border rectangle: bottom, right, top, left
    entities.push(dxfLine(x, y, x + w, y, handle1));         // Bottom edge
    entities.push(dxfLine(x + w, y, x + w, y + h, handle2)); // Right edge  
    entities.push(dxfLine(x + w, y + h, x, y + h, handle3)); // Top edge
    entities.push(dxfLine(x, y + h, x, y, handle4));         // Left edge
  }

  console.log('Building DXF with', entities.length, 'entities');
  // Use entities-only DXF for both modes to match Fusion-accepted format
  const dxfContent = buildMinimalDXF(entities);
  console.log('DXF content length:', dxfContent.length);
  console.log('Using entities-only DXF format');

  // Trigger download in browser
  const blob = new Blob([dxfContent], { type: 'application/dxf' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename.endsWith('.dxf') ? filename : `${filename}.dxf`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
  console.log('DXF export completed successfully');
  } catch (error) {
    console.error('DXF export failed:', error);
    throw error;
  }
}

// Add letters as vector paths
async function addVectorPaths(
  entities: string[], 
  grid: string[][], 
  layout: ClockLayout, 
  config: DXFConfig, 
  totalHeight: number, 
  font: any,
  handleCounter: { value: number }
): Promise<void> {
  for (let row = 0; row < layout.gridHeight; row++) {
    for (let col = 0; col < layout.gridWidth; col++) {
      const letter = grid[row][col];
      if (letter === ' ') continue;

      // Calculate position (centered in cell)
      // DXF uses bottom-left origin, so we need to flip Y to maintain correct row order
      const spacingX = config.gridSpacing;
      const spacingY = config.gridSpacingY ?? config.gridSpacing;
      const cx = config.margin + (col + 0.5) * spacingX;
      const cy = totalHeight - config.margin - (row + 0.5) * spacingY;

      // Fit glyph height to cell height with per-side margin percent
      const marginPct = Math.max(0, Math.min(0.49, config.letterPaddingPercent ?? 0.1));
      const targetHeight = spacingY * (1 - 2 * marginPct);

      // Measure glyph at reference size and compute fontSize that yields targetHeight
      const measurePath = extractGlyphPath(font, letter, 1000);
      if (measurePath) {
        try {
          const mbox = (measurePath as any).getBoundingBox();
          const mheight = Math.max(0.0001, (mbox.y2 - mbox.y1));
          const fontSizeForTarget = (targetHeight * 1000) / mheight;

          const path = extractGlyphPath(font, letter, fontSizeForTarget);
          if (!path) throw new Error('Failed to build scaled glyph path');

          // Calculate glyph bounds to position it properly
          const bounds = (path as any).getBoundingBox();
          
          // Apply horizontal stretch including W-specific stretch
          const baseStretch = config.horizontalStretch ?? 1.0;
          const wStretch = config.wStretch ?? 1.0;
          const isW = letter.toUpperCase() === 'W';
          const horizontalStretch = isW ? baseStretch * wStretch : baseStretch;
          
          // Account for horizontal stretch in bounds calculation
          // Since we apply stretch to coordinates, we need to adjust bounds accordingly
          const stretchedBounds = {
            x1: bounds.x1 * horizontalStretch,
            x2: bounds.x2 * horizontalStretch,
            y1: bounds.y1,
            y2: bounds.y2
          };
          
          // Horizontal positioning - center or left-align
          const centerHorizontally = config.centerHorizontally ?? true;
          const offsetX = centerHorizontally
            ? cx - (stretchedBounds.x1 + stretchedBounds.x2) / 2  // Center horizontally
            : config.margin + col * spacingX - stretchedBounds.x1;  // Left-align to cell left edge
          
          // Vertical positioning - always center
          // Note: We negate Y coordinates in pathToDXFEntities, so we need to add (not subtract) the bounds center
          const offsetY = cy + (bounds.y1 + bounds.y2) / 2;
          
          // Convert path to DXF entities with positioning and stretch
          const result = pathToDXFEntities(path, offsetX, offsetY, handleCounter.value, horizontalStretch);
          const lineEntities = convertPolylinesToLines(result.entities, handleCounter);
          entities.push(...lineEntities);
          handleCounter.value = result.nextHandle;
        } catch (error) {
          console.warn(`Error processing glyph for '${letter}':`, error);
          // Skip this letter if vector path generation fails
          continue;
        }
      } else {
        // Skip this letter if path extraction fails
        console.warn(`No path found for letter '${letter}', skipping`);
        continue;
      }
    }
  }
}

// Add simple test shapes for debugging - single letter as vector
async function addTestShapes(
  entities: string[], 
  config: DXFConfig, 
  totalHeight: number,
  handleCounter: { value: number }
): Promise<void> {
  console.log('TEST MODE: Creating single letter "A" as vector path');
  
  // First, let's test with simple LINE entities to see if Fusion 360 accepts those
  console.log('Creating simple test lines first...');
  createSimpleTestLines(entities, handleCounter);
  
  try {
    // Load the font for path extraction
    const font = await getFont(config.fontName);
    if (!font) {
      console.warn(`Font ${config.fontName} not available, using fallback font`);
      const fallbackFont = createFallbackFont();
      await addSingleLetter(entities, 'A', config, totalHeight, handleCounter, fallbackFont);
    } else {
      console.log(`Successfully loaded font: ${config.fontName}`);
      await addSingleLetter(entities, 'A', config, totalHeight, handleCounter, font);
    }
  } catch (error) {
    console.error('Error loading font, using fallback:', error);
    const fallbackFont = createFallbackFont();
    await addSingleLetter(entities, 'A', config, totalHeight, handleCounter, fallbackFont);
  }
}

// Create simple test lines to verify basic DXF structure
function createSimpleTestLines(entities: string[], handleCounter: { value: number }): void {
  // Create a simple cross pattern using basic LINE entities
  const centerX = 50;
  const centerY = 50;
  const size = 20;
  
  // Horizontal line
  entities.push([
    '0','LINE',
    '5', (handleCounter.value++).toString(16).toUpperCase(),
    '8','0',
    '100','AcDbEntity',
    '100','AcDbLine',
    '10', (centerX - size).toString(), '20', centerY.toString(), '30','0',
    '11', (centerX + size).toString(), '21', centerY.toString(), '31','0'
  ].join('\n'));
  
  // Vertical line
  entities.push([
    '0','LINE',
    '5', (handleCounter.value++).toString(16).toUpperCase(),
    '8','0',
    '100','AcDbEntity',
    '100','AcDbLine',
    '10', centerX.toString(), '20', (centerY - size).toString(), '30','0',
    '11', centerX.toString(), '21', (centerY + size).toString(), '31','0'
  ].join('\n'));
  
  // Diagonal line
  entities.push([
    '0','LINE',
    '5', (handleCounter.value++).toString(16).toUpperCase(),
    '8','0',
    '100','AcDbEntity',
    '100','AcDbLine',
    '10', (centerX - size).toString(), '20', (centerY - size).toString(), '30','0',
    '11', (centerX + size).toString(), '21', (centerY + size).toString(), '31','0'
  ].join('\n'));
  
  console.log(`Created simple test lines at (${centerX}, ${centerY})`);
}

// Add a single letter as vector path for testing
async function addSingleLetter(
  entities: string[], 
  letter: string,
  config: DXFConfig, 
  totalHeight: number,
  handleCounter: { value: number },
  font: any
): Promise<void> {
  // Position the letter in the center
  const centerX = config.margin + (config.gridSpacing * 5); // Center of a 10x10 grid
  const centerY = totalHeight - config.margin - (config.gridSpacing * 5);
  
  console.log(`Creating letter "${letter}" at (${centerX}, ${centerY})`);
  
  // Extract glyph path
  const path = extractGlyphPath(font, letter, config.letterSize);
  if (path) {
    try {
      // Calculate glyph bounds to center it properly
      const bounds = path.getBoundingBox();
      
      // Center the glyph within the cell
      const offsetX = centerX - (bounds.x1 + bounds.x2) / 2;
      const offsetY = centerY - (bounds.y1 + bounds.y2) / 2;
      
      // Convert path to DXF entities with proper centering
      const result = pathToDXFEntities(path, offsetX, offsetY, handleCounter.value);
      
      // For testing, convert LWPOLYLINE to simple LINE entities
      // This makes it more compatible with Fusion 360
      const lineEntities = convertPolylinesToLines(result.entities, handleCounter);
      entities.push(...lineEntities);
      handleCounter.value = result.nextHandle;
      
      console.log(`Successfully created vector letter "${letter}" with ${result.entities.length} entities`);
    } catch (error) {
      console.warn(`Error processing glyph for '${letter}':`, error);
      // Create a simple fallback square
      createFallbackSquare(entities, centerX, centerY, config.letterSize, handleCounter);
    }
  } else {
    console.warn(`No path found for letter '${letter}', creating fallback square`);
    createFallbackSquare(entities, centerX, centerY, config.letterSize, handleCounter);
  }
}

// Create a simple fallback square if letter path fails
function createFallbackSquare(
  entities: string[], 
  centerX: number, 
  centerY: number, 
  size: number,
  handleCounter: { value: number }
): void {
  const halfSize = size / 2;
  
  // Create square using 4 lines
  const points = [
    { x: centerX - halfSize, y: centerY - halfSize },
    { x: centerX + halfSize, y: centerY - halfSize },
    { x: centerX + halfSize, y: centerY + halfSize },
    { x: centerX - halfSize, y: centerY + halfSize }
  ];
  
  // Create each side as a separate line
  for (let i = 0; i < points.length; i++) {
    const start = points[i];
    const end = points[(i + 1) % points.length];
    
    entities.push([
      '0','LINE',
      '5', (handleCounter.value++).toString(16).toUpperCase(),
      '8','0',
      '100','AcDbEntity',
      '100','AcDbLine',
      '10', start.x.toString(), '20', start.y.toString(), '30','0',
      '11', end.x.toString(), '21', end.y.toString(), '31','0'
    ].join('\n'));
  }
  
  console.log(`Created fallback square at (${centerX}, ${centerY}) with size ${size}`);
}

// Convert LWPOLYLINE entities to simple LINE entities for better Fusion 360 compatibility
function convertPolylinesToLines(polylineEntities: string[], handleCounter: { value: number }): string[] {
  const lineEntities: string[] = [];
  
  for (const entity of polylineEntities) {
    // Skip empty or invalid entities
    if (!entity || entity.trim().length === 0) {
      continue;
    }
    
    if (entity.includes('LWPOLYLINE')) {
      // Parse the LWPOLYLINE to extract points
      const lines = entity.split('\n');
      const points: Array<{x: number, y: number}> = [];
      
      // Find where vertex data starts (after the vertex count '90' code)
      let startIndex = 0;
      for (let i = 0; i < lines.length; i++) {
        if (lines[i].trim() === '90') {
          // Skip past the vertex count value and constant width (43, 0)
          startIndex = i + 2; // Start after '90' and its value
          break;
        }
      }
      
      for (let i = startIndex; i < lines.length; i++) {
        const line = lines[i].trim();
        
        // Look for X coordinate group code '10' followed by a value, then '20' for Y
        if (line === '10' && i + 3 < lines.length && lines[i + 2].trim() === '20') {
          const x = parseFloat(lines[i + 1]);
          const y = parseFloat(lines[i + 3]);
          
          // Validate the coordinates
          if (!isNaN(x) && !isNaN(y) && isFinite(x) && isFinite(y)) {
            points.push({ x, y });
          }
          i += 3; // Skip the next 3 lines (x, 20, y)
        }
      }
      
      // Convert points to LINE entities (only if we have valid points)
      if (points.length >= 2) {
        for (let i = 0; i < points.length; i++) {
          const start = points[i];
          const end = points[(i + 1) % points.length];
          
          // Validate both start and end points
          if (!isNaN(start.x) && !isNaN(start.y) && !isNaN(end.x) && !isNaN(end.y)) {
            lineEntities.push([
              '0','LINE',
              '5', (handleCounter.value++).toString(16).toUpperCase(),
              '8','0',
              '100','AcDbEntity',
              '100','AcDbLine',
              '10', start.x.toFixed(6), '20', start.y.toFixed(6), '30','0',
              '11', end.x.toFixed(6), '21', end.y.toFixed(6), '31','0'
            ].join('\n'));
          }
        }
      }
    } else {
      // Keep non-LWPOLYLINE entities as-is (if they're valid)
      if (entity.trim().length > 0) {
        lineEntities.push(entity);
      }
    }
  }
  
  return lineEntities;
}

// Add letters as text entities (fallback)
async function addTextEntities(
  entities: string[], 
  grid: string[][], 
  layout: ClockLayout, 
  config: DXFConfig, 
  totalHeight: number,
  handleCounter: { value: number }
): Promise<void> {
  for (let row = 0; row < layout.gridHeight; row++) {
    for (let col = 0; col < layout.gridWidth; col++) {
      const letter = grid[row][col];
      if (letter === ' ') continue;
      const cx = config.margin + (col + 0.5) * config.gridSpacing;
      const cy = totalHeight - config.margin - (row + 0.5) * config.gridSpacing;
      const handle = handleCounter.value.toString(16).toUpperCase().padStart(4, '0');
      entities.push(dxfText(cx, cy, config.letterSize, letter, handle));
      handleCounter.value++;
    }
  }
}

// Export function that can be called from React components
export async function downloadDXF(layout: ClockLayout, filename: string, config?: Partial<DXFConfig>): Promise<void> {
  const finalFilename = filename.endsWith('.dxf') ? filename : `${filename}.dxf`;
  await exportGridToDXF(layout, finalFilename, config);
}
