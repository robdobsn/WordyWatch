import React, { useState, useEffect } from 'react';
import { getFont, extractGlyphPath, createFallbackFont } from '../utils/fontPathExtractor';
import { pathToSVG } from '../utils/svgPathConverter';
import { FontSettings } from '../types/layout';

// Map CSS font families to font file names (same as DXFExport)
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

interface VectorLetterCellProps {
  letter: string;
  isActive: boolean;
  isEmpty: boolean;
  fontSettings: FontSettings;
  cellWidth: number; // mm
  cellHeight: number; // mm
  fontName?: string; // Optional mapped font name (will use fontSettings.family if not provided)
}

const VectorLetterCell: React.FC<VectorLetterCellProps> = ({
  letter,
  isActive,
  isEmpty,
  fontSettings,
  cellWidth,
  cellHeight,
  fontName: providedFontName
}) => {
  const fontName = providedFontName || mapFontFamilyToFontName(fontSettings.family);
  const [svgPath, setSvgPath] = useState<string>('');

  useEffect(() => {
    if (isEmpty || letter === ' ') {
      setSvgPath('');
      return;
    }

    async function loadAndConvert() {
      try {
        // Load font
        const font = await getFont(fontName);
        const workingFont = font || createFallbackFont();

        // Calculate target letter height (same logic as DXF export)
        const marginPct = Math.max(0, Math.min(0.49, fontSettings.letterPaddingPercent ?? 0.1));
        const targetHeight = cellHeight * (1 - 2 * marginPct);

        // Measure glyph at reference size to determine scale (same logic as DXF export)
        const measurePath = extractGlyphPath(workingFont, letter, 1000);
        if (measurePath) {
          try {
            const mbox = (measurePath as any).getBoundingBox();
            const mheight = Math.max(0.0001, (mbox.y2 - mbox.y1));
            const fontSizeForTarget = (targetHeight * 1000) / mheight;

            const path = extractGlyphPath(workingFont, letter, fontSizeForTarget);
            if (!path) throw new Error('Failed to build scaled glyph path');

            // Calculate glyph bounds to position it properly
            const bounds = (path as any).getBoundingBox();
            
            // Use cell center
            const cx = cellWidth / 2;
            const cy = cellHeight / 2;
            
            // Calculate effective horizontal stretch including W-specific stretch
            const baseStretch = fontSettings.horizontalStretch || 1.0;
            const wStretch = fontSettings.wStretch || 1.0;
            const isW = letter.toUpperCase() === 'W';
            const effectiveStretch = isW ? baseStretch * wStretch : baseStretch;
            
            // Horizontal positioning - center or left-align
            const glyphCenterX = (bounds.x1 + bounds.x2) / 2;
            
            let offsetX;
            if (fontSettings.centerHorizontally) {
              // Center horizontally
              // The SVG transform scale(effectiveStretch, -1) is applied to the path coordinates
              // After transform: x_final = (x + offsetX) * stretch
              // We want glyph center to be at cx: (glyphCenterX + offsetX) * stretch = cx
              // Therefore: offsetX = cx / stretch - glyphCenterX
              offsetX = cx / effectiveStretch - glyphCenterX;
            } else {
              // Left-align (align left edge of glyph to x=0)
              offsetX = -bounds.x1;
            }
            
            // Vertical positioning - always center
            // For SVG (Y increases downward, origin at top):
            // We need to position the glyph so its center is at cy from the top
            // OpenType Y increases upward, so we need to flip
            // After flipping: y_svg = -y_opentype + offsetY
            // We want: glyphCenter_svg = cy
            // So: -(bounds.y1 + bounds.y2)/2 + offsetY = cy
            // Therefore: offsetY = cy + (bounds.y1 + bounds.y2)/2
            const offsetY = cy + (bounds.y1 + bounds.y2) / 2;
            
            // Convert to SVG path with Y flipping
            const svgPathData = pathToSVG(path, offsetX, offsetY);
            
            // Debug: Log the calculations
            const glyphCenterY = (bounds.y1 + bounds.y2) / 2;
            console.log(`${letter}: centered=${fontSettings.centerHorizontally}, stretch=${effectiveStretch.toFixed(2)}, cx=${cx.toFixed(2)}, glyphCenterX=${glyphCenterX.toFixed(2)}, offsetX=${offsetX.toFixed(2)}, bounds.x=(${bounds.x1.toFixed(2)},${bounds.x2.toFixed(2)})`);
            
            setSvgPath(svgPathData);
          } catch (error) {
            console.warn(`Error processing glyph for '${letter}':`, error);
            setSvgPath('');
          }
        }
      } catch (error) {
        console.warn(`Error rendering letter '${letter}':`, error);
        setSvgPath('');
      }
    }

    loadAndConvert();
  }, [letter, isEmpty, fontName, cellWidth, cellHeight, fontSettings.letterPaddingPercent, fontSettings.centerHorizontally, fontSettings.horizontalStretch, fontSettings.wStretch]);

  if (isEmpty || letter === ' ') {
    return (
      <span
        style={{
          width: '100%',
          height: '100%',
          display: 'inline-block',
        }}
      />
    );
  }

  // Calculate total horizontal stretch including W-specific stretch
  const baseStretch = fontSettings.horizontalStretch || 1.0;
  const wStretch = fontSettings.wStretch || 1.0;
  const isW = letter.toUpperCase() === 'W';
  const horizontalStretch = isW ? baseStretch * wStretch : baseStretch;
  
  return (
    <svg
      viewBox={`0 ${-cellHeight} ${cellWidth} ${cellHeight}`}
      preserveAspectRatio="xMidYMid meet"
      style={{
        width: '100%',
        height: '100%',
        display: 'block',
        overflow: 'visible',
      }}
    >
      <g transform={`scale(${horizontalStretch}, -1)`}>
        <path
          d={svgPath}
          fill={isActive ? '#ffffff' : '#6b7280'}
          stroke="none"
          className="transition-all duration-300"
        />
      </g>
    </svg>
  );
};

export default VectorLetterCell;

