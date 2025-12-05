// SVG Export utility for word clock grid
// Generates SVG with vector paths that match the UI display exactly

import { ClockLayout, FontSettings } from '../types/layout';
import { getFont, extractGlyphPath, createFallbackFont } from './fontPathExtractor';
import { pathToSVGNoFlip } from './svgPathConverter';

// Map CSS font families to font names (same as other exporters)
function mapFontFamilyToFontName(fontFamily: string): string {
  const fontMap: { [key: string]: string } = {
    'Monaco, Menlo, Ubuntu Mono, monospace': 'Monaco',
    'Arial, sans-serif': 'Arial',
    'Ruler Stencil Regular, Arial, sans-serif': 'Ruler Stencil Regular',
    'Ruler Stencil Bold, Arial, sans-serif': 'Ruler Stencil Bold',
    'Ruler Stencil Heavy, Arial, sans-serif': 'Ruler Stencil Heavy',
    'Ruler Stencil Light, Arial, sans-serif': 'Ruler Stencil Light',
    'Ruler Stencil Thin, Arial, sans-serif': 'Ruler Stencil Thin',
    'Warzone Stencil, Arial, sans-serif': 'Warzone Stencil',
    'Lazer Game Zone, Arial, sans-serif': 'Lazer Game Zone',
    'Octin Stencil Rg, Arial, sans-serif': 'Octin Stencil Rg',
    'Ombudsman Alternate, Arial, sans-serif': 'Ombudsman Alternate',
  };
  
  return fontMap[fontFamily] || 'Arial';
}

// Create a letter grid from layout data
function createLetterGrid(layout: ClockLayout): string[][] {
  if (layout.gridData && layout.gridData.length > 0) {
    return layout.gridData;
  }
  
  const grid: string[][] = Array(layout.gridHeight)
    .fill(null)
    .map(() => Array(layout.gridWidth).fill(' '));
  
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

// Mounting features options
export interface MountingOptions {
  outerWidth: number;
  outerHeight: number;
  holeSpacingWidth: number;
  holeSpacingHeight: number;
  holeDiameter: number;
  cornerRadius: number;
}

// Generate SVG content that matches the UI display using vector paths
export async function generateSVG(
  layout: ClockLayout,
  fontSettings: FontSettings,
  flipHorizontal: boolean = false,
  mountingOptions?: MountingOptions
): Promise<string> {
  const grid = createLetterGrid(layout);
  const fontName = mapFontFamilyToFontName(fontSettings.family);
  
  // Load the font for path extraction (same as DXF export)
  let font;
  try {
    font = await getFont(fontName);
    if (!font) {
      console.warn(`Font ${fontName} not available, using fallback font`);
      font = createFallbackFont();
    }
  } catch (error) {
    console.error('Error loading font, using fallback:', error);
    font = createFallbackFont();
  }
  
  // Calculate dimensions (in mm, matching the UI)
  const cellWidth = fontSettings.cellSpacingX;
  const cellHeight = fontSettings.cellSpacingY;
  const totalWidth = layout.gridWidth * cellWidth;
  const totalHeight = layout.gridHeight * cellHeight;
  const margin = fontSettings.margin || 2;
  
  // Base SVG dimensions (text area with margin)
  const textAreaWidth = totalWidth + 2 * margin;
  const textAreaHeight = totalHeight + 2 * margin;
  
  // Final SVG dimensions (with mounting features if specified)
  let svgWidth = textAreaWidth;
  let svgHeight = textAreaHeight;
  let borderOffsetX = 0;
  let borderOffsetY = 0;
  
  if (mountingOptions) {
    svgWidth = mountingOptions.outerWidth;
    svgHeight = mountingOptions.outerHeight;
    // Calculate equal borders on each side
    borderOffsetX = (svgWidth - textAreaWidth) / 2;
    borderOffsetY = (svgHeight - textAreaHeight) / 2;
  }
  
  // Calculate target letter height based on cell height and padding
  // This matches both DXF export and VectorLetterCell logic
  const marginPct = Math.max(0, Math.min(0.49, fontSettings.letterPaddingPercent ?? 0.1));
  const targetHeight = cellHeight * (1 - 2 * marginPct);
  
  let svg = '';
  
  // SVG header
  svg += `<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n`;
  svg += `<!-- Auto-generated from Wordy Watch UI -->\n`;
  svg += `<!-- Layout: ${layout.name} -->\n`;
  svg += `<!-- Grid: ${layout.gridWidth}x${layout.gridHeight}, Cell: ${cellWidth}x${cellHeight}mm -->\n`;
  if (mountingOptions) {
    svg += `<!-- Outer dimensions: ${svgWidth}x${svgHeight}mm -->\n`;
  }
  svg += `<svg\n`;
  svg += `  width="${svgWidth}mm"\n`;
  svg += `  height="${svgHeight}mm"\n`;
  svg += `  viewBox="0 0 ${svgWidth} ${svgHeight}"\n`;
  svg += `  xmlns="http://www.w3.org/2000/svg"\n`;
  svg += `  version="1.1">\n`;
  
  // Main group with optional horizontal flip
  const flipTransform = flipHorizontal ? `translate(${svgWidth}, 0) scale(-1, 1)` : '';
  svg += `  <g${flipTransform ? ` transform="${flipTransform}"` : ''}>\n`;
  
  // Background - solid black rounded rectangle
  if (mountingOptions) {
    svg += `    <rect x="0" y="0" width="${svgWidth}" height="${svgHeight}" rx="${mountingOptions.cornerRadius}" ry="${mountingOptions.cornerRadius}" fill="#000000" stroke="none"/>\n`;
    
    // Add mounting holes (white circles to cut through black background)
    const centerX = svgWidth / 2;
    const centerY = svgHeight / 2;
    const halfSpacingW = mountingOptions.holeSpacingWidth / 2;
    const halfSpacingH = mountingOptions.holeSpacingHeight / 2;
    const radius = mountingOptions.holeDiameter / 2;
    
    // Four corners of the hole spacing rectangle
    const holes = [
      { x: centerX - halfSpacingW, y: centerY - halfSpacingH }, // Top-left
      { x: centerX + halfSpacingW, y: centerY - halfSpacingH }, // Top-right
      { x: centerX - halfSpacingW, y: centerY + halfSpacingH }, // Bottom-left
      { x: centerX + halfSpacingW, y: centerY + halfSpacingH }, // Bottom-right
    ];
    
    holes.forEach((hole) => {
      svg += `    <circle cx="${hole.x}" cy="${hole.y}" r="${radius}" fill="#ffffff" stroke="none"/>\n`;
    });
  } else {
    svg += `    <rect x="0" y="0" width="${svgWidth}" height="${svgHeight}" fill="#000000" stroke="none"/>\n`;
  }
  
  // Grid lines (if enabled)
  if (fontSettings.addGridLines) {
    svg += `    <g id="gridlines" stroke="#444444" stroke-width="0.1" fill="none">\n`;
    
    // Vertical lines
    for (let col = 0; col <= layout.gridWidth; col++) {
      const x = borderOffsetX + margin + col * cellWidth;
      svg += `      <line x1="${x}" y1="${borderOffsetY + margin}" x2="${x}" y2="${borderOffsetY + margin + totalHeight}"/>\n`;
    }
    
    // Horizontal lines
    for (let row = 0; row <= layout.gridHeight; row++) {
      const y = borderOffsetY + margin + row * cellHeight;
      svg += `      <line x1="${borderOffsetX + margin}" y1="${y}" x2="${borderOffsetX + margin + totalWidth}" y2="${y}"/>\n`;
    }
    
    svg += `    </g>\n`;
  }
  
  // Define style for letters
  svg += `    <defs>\n`;
  svg += `      <style>\n`;
  svg += `        .letter { fill: #ffffff; stroke: none; }\n`;
  svg += `      </style>\n`;
  svg += `    </defs>\n`;
  
  // Build a set of positions that contain letters from actual words
  const wordPositions = new Set<string>();
  layout.words.forEach((wordPos: any) => {
    const { word, startRow, startCol, direction } = wordPos;
    for (let i = 0; i < word.length; i++) {
      const row = direction === 'horizontal' ? startRow : startRow + i;
      const col = direction === 'horizontal' ? startCol + i : startCol;
      wordPositions.add(`${row}-${col}`);
    }
  });
  
    // Letters group
    svg += `    <g id="letters">\n`;
  
  // Render each letter as vector path
  for (let rowIndex = 0; rowIndex < grid.length; rowIndex++) {
    const row = grid[rowIndex];
    for (let colIndex = 0; colIndex < row.length; colIndex++) {
      const letter = row[colIndex];
      const isEmpty = letter === ' ' || letter === '.';
      
      // Check if this letter should be displayed
      const displayAll = fontSettings.displayAllLetters ?? true;
      const isInWord = wordPositions.has(`${rowIndex}-${colIndex}`);
      
      if (!displayAll && !isInWord) continue;
      if (isEmpty) continue;
      
      // Calculate cell position (top-left corner) with border offset
      const cellX = borderOffsetX + margin + colIndex * cellWidth;
      const cellY = borderOffsetY + margin + rowIndex * cellHeight;
      
      // Calculate cell center
      const cx = cellX + cellWidth / 2;
      const cy = cellY + cellHeight / 2;
      
      // Calculate horizontal stretch (same as DXF and UI)
      const baseStretch = fontSettings.horizontalStretch || 1.0;
      const wStretch = fontSettings.wStretch || 1.0;
      const isW = letter.toUpperCase() === 'W';
      const effectiveStretch = isW ? baseStretch * wStretch : baseStretch;
      
      // Measure glyph at reference size to determine scale (same as DXF export)
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
          
          // Horizontal positioning (same logic as DXF export and VectorLetterCell)
          const glyphCenterX = (bounds.x1 + bounds.x2) / 2;
          
          let offsetX;
          if (fontSettings.centerHorizontally) {
            // Center horizontally
            offsetX = cx / effectiveStretch - glyphCenterX;
          } else {
            // Left-align
            offsetX = cellX / effectiveStretch - bounds.x1;
          }
          
          // Vertical positioning - center in cell (SVG Y increases downward)
          // For SVG without Y-flip, we keep OpenType coordinates and position accordingly
          const glyphCenterY = (bounds.y1 + bounds.y2) / 2;
          const offsetY = cy - glyphCenterY;
          
          // Convert to SVG path (no Y flip since we're using standard SVG coordinates)
          const svgPathData = pathToSVGNoFlip(path, offsetX, offsetY);
          
          if (svgPathData) {
            // Create a group with horizontal stretch transform
            svg += `      <g transform="scale(${effectiveStretch}, 1)">\n`;
            svg += `        <path class="letter" d="${svgPathData}"/>\n`;
            svg += `      </g>\n`;
          }
        } catch (error) {
          console.warn(`Error processing glyph for '${letter}':`, error);
        }
      }
    }
  }
  
    svg += `    </g>\n`;
    svg += `  </g>\n`;
  svg += `</svg>\n`;
  
  return svg;
}

// Download the SVG file
export async function downloadSVG(
  layout: ClockLayout,
  filename: string,
  fontSettings: FontSettings,
  flipHorizontal: boolean = false,
  mountingOptions?: MountingOptions
): Promise<void> {
  try {
    const svgContent = await generateSVG(layout, fontSettings, flipHorizontal, mountingOptions);
    
    // Create blob and download
    const blob = new Blob([svgContent], { type: 'image/svg+xml' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `${filename}.svg`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  } catch (error) {
    console.error('Error generating SVG:', error);
    throw error;
  }
}
