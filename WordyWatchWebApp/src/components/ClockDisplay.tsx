import React from 'react';
import { ClockLayout, FontSettings, WordPosition } from '../types/layout';
import { convertToMilitaryTime } from '../utils/militaryTime';
import { generateMilitaryCondensedGrid, generateCrosswordGrid, generateGraceGPTGrid, generateGraceGPT2Grid, generateAutoLayoutGrid } from '../utils/gridGenerator';
import { LayoutMetadata } from '../hooks/useLayout';
import VectorLetterCell from './VectorLetterCell';

interface ClockDisplayProps {
  layout: ClockLayout;
  hours: number;
  minutes: number;
  fontSettings: FontSettings;
  layoutMetadata?: LayoutMetadata | null;
}

interface LetterGridProps {
  layout: ClockLayout;
  activeWords: string[];
  fontSettings: FontSettings;
}

interface CategorizedLetterGridProps {
  layout: ClockLayout;
  activeWordsWithCategory: Array<{word: string, category: 'hour' | 'minute' | 'military'}>;
  fontSettings: FontSettings;
  layoutMetadata?: LayoutMetadata | null;
}

function createLetterGrid(layout: ClockLayout, layoutMetadata?: LayoutMetadata | null): string[][] {
  // Use pre-generated grid for military-condensed layout
  if (layout.name === 'Military Condensed') {
    return generateMilitaryCondensedGrid();
  }
  
  // Use crossword grid generator for crossword layouts
  if (layout.name === 'Crossword One') {
    return generateCrosswordGrid(layout);
  }
  
  // Use GraceGPT grid generator for GraceGPT layout
  if (layout.name === 'GraceGPT') {
    return generateGraceGPTGrid();
  }
  
  // Use GraceGPT2 grid generator for GraceGPT2 layout
  if (layout.name === 'GraceGPT2') {
    return generateGraceGPT2Grid();
  }
  
  // Use JSON-based grid generation for layouts with metadata indicating this capability
  if (layoutMetadata?.gridGeneration === 'json-based' && layout.words.length > 0) {
    // Use the word positions from the JSON layout
    const grid: string[][] = Array(layout.gridHeight)
      .fill(null)
      .map(() => Array(layout.gridWidth).fill(' '));
    
    // Place words in the grid using the JSON layout data
    layout.words.forEach((wordPos: any) => {
      const { word, startRow, startCol, direction } = wordPos;
      
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
  
  // Fallback for layouts without JSON data
  if (layout.name === 'Auto Layout' || layout.name === 'Updated Layout') {
    return generateAutoLayoutGrid();
  }
  
  // Emergency fallback - create empty grid if no other option works
  if (layoutMetadata?.gridGeneration === 'json-based') {
    return Array(layout.gridHeight || 12)
      .fill(null)
      .map(() => Array(layout.gridWidth || 12).fill(' '));
  }
  
  // Initialize grid with empty spaces for other layouts
  const grid: string[][] = Array(layout.gridHeight)
    .fill(null)
    .map(() => Array(layout.gridWidth).fill(' '));
  
  // Place words in the grid
  layout.words.forEach((wordPos: WordPosition) => {
    const { word, startRow, startCol, direction } = wordPos;
    
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

// Helper function to find words using category-based priority
function findWordWithCategoryPriority(layout: ClockLayout, word: string, preferredCategory: 'hour' | 'minute' | 'military' | 'connector'): Array<{row: number, col: number}> {
  // Find all instances of the word in the layout
  const wordInstances = layout.words.filter(w => w.word === word);
  
  if (wordInstances.length === 0) {
    return [];
  }
  
  // For minute category, find words categorized as 'minute' (which should be vertical due to gridAnalyzer logic)
  if (preferredCategory === 'minute') {
    const minuteWords = wordInstances.filter(w => w.category === 'minute');
    if (minuteWords.length > 0) {
      return getPositionsFromWordInstance(minuteWords[0], word);
    }
    
    // Fallback for fragment words: if no minute category found, check hour category BUT only vertical direction
    // This handles cases like FIF/TEEN which might be categorized as hour but used for minutes
    // STRICT RULE: Minutes must ALWAYS be vertical
    const fragmentWords = ['FIF', 'TEEN'];
    if (fragmentWords.includes(word)) {
      const verticalHourWords = wordInstances.filter(w => w.category === 'hour' && w.direction === 'vertical');
      if (verticalHourWords.length > 0) {
        return getPositionsFromWordInstance(verticalHourWords[0], word);
      }
    }
    
    // If no suitable word found, return empty
    return [];
  }
  
  // For hour category, find words categorized as 'hour' (which should be horizontal due to gridAnalyzer logic)
  if (preferredCategory === 'hour') {
    const hourWords = wordInstances.filter(w => w.category === 'hour');
    if (hourWords.length > 0) {
      return getPositionsFromWordInstance(hourWords[0], word);
    }
    // If no hour word found, return empty
    return [];
  }
  
  // For connector category, prefer vertical direction
  if (preferredCategory === 'connector') {
    const verticalConnectorWords = wordInstances.filter(w => w.category === 'connector' && w.direction === 'vertical');
    if (verticalConnectorWords.length > 0) {
      return getPositionsFromWordInstance(verticalConnectorWords[0], word);
    }
    // Fallback to any connector if no vertical found
    const connectorWords = wordInstances.filter(w => w.category === 'connector');
    if (connectorWords.length > 0) {
      return getPositionsFromWordInstance(connectorWords[0], word);
    }
    return [];
  }
  
  // For military category, use any direction
  if (preferredCategory === 'military') {
    const militaryWords = wordInstances.filter(w => w.category === 'military');
    if (militaryWords.length > 0) {
      return getPositionsFromWordInstance(militaryWords[0], word);
    }
  }
  
  // No valid word found for the preferred category and direction constraints
  return [];
}

// Helper function to get positions from a word instance
function getPositionsFromWordInstance(wordInstance: any, word: string): Array<{row: number, col: number}> {
  const { startRow, startCol, direction } = wordInstance;
  const positions: Array<{row: number, col: number}> = [];
  
  for (let i = 0; i < word.length; i++) {
    const row = direction === 'horizontal' ? startRow : startRow + i;
    const col = direction === 'horizontal' ? startCol + i : startCol;
    positions.push({ row, col });
  }
  
  return positions;
}

function getLetterPositions(layout: ClockLayout, word: string, preferredCategory?: 'hour' | 'minute' | 'military' | 'connector', layoutMetadata?: LayoutMetadata | null): Array<{row: number, col: number}> {
  // For layouts that support categories, use category-based priority
  if (preferredCategory && layoutMetadata?.hasCategories) {
    return findWordWithCategoryPriority(layout, word, preferredCategory);
  }
  
  // Fallback to first match for other layouts or when no category specified
  const wordPos = layout.words.find(w => w.word === word);
  if (!wordPos) return [];
  
  const { startRow, startCol, direction } = wordPos;
  const positions: Array<{row: number, col: number}> = [];
  
  for (let i = 0; i < word.length; i++) {
    const row = direction === 'horizontal' ? startRow : startRow + i;
    const col = direction === 'horizontal' ? startCol + i : startCol;
    positions.push({ row, col });
  }
  
  return positions;
}

const CategorizedLetterGrid: React.FC<CategorizedLetterGridProps> = ({ layout, activeWordsWithCategory, fontSettings, layoutMetadata }) => {
  const grid = createLetterGrid(layout, layoutMetadata);
  
  // Get all active letter positions using category information
  const activePositions = new Set<string>();
  activeWordsWithCategory.forEach(({ word, category }) => {
    const positions = getLetterPositions(layout, word, category, layoutMetadata);
    positions.forEach(pos => {
      activePositions.add(`${pos.row}-${pos.col}`);
    });
  });
  
  // Calculate aspect ratio based on actual cell dimensions
  const totalWidth = layout.gridWidth * fontSettings.cellSpacingX;
  const totalHeight = layout.gridHeight * fontSettings.cellSpacingY;
  const aspectRatio = totalWidth / totalHeight;
  
  // Calculate border and grid line styles
  const borderClass = fontSettings.addBorder ? 'border-2 border-gray-300' : 'border-0';
  const cellBorderStyle = fontSettings.addGridLines 
    ? { borderRight: '1px solid #444', borderBottom: '1px solid #444' }
    : {};
  
  return (
    <div 
      className={`inline-block ${borderClass} p-2 sm:p-4 bg-black w-full max-w-full`}
      style={{
        aspectRatio: aspectRatio.toString(),
      }}
    >
      {grid.map((row, rowIndex) => (
        <div key={rowIndex} className="flex" style={{ gap: '0', marginBottom: '0', height: `${100 / layout.gridHeight}%` }}>
          {row.map((letter, colIndex) => {
            const isActive = activePositions.has(`${rowIndex}-${colIndex}`);
            const isEmpty = letter === ' ';
            
            return (
              <div
                key={`${rowIndex}-${colIndex}`}
                style={{
                  flex: '1 1 0',
                  height: '100%',
                  ...cellBorderStyle,
                }}
              >
                <VectorLetterCell
                  letter={letter}
                  isActive={isActive}
                  isEmpty={isEmpty}
                  fontSettings={fontSettings}
                  cellWidth={fontSettings.cellSpacingX}
                  cellHeight={fontSettings.cellSpacingY}
                />
              </div>
            );
          })}
        </div>
      ))}
    </div>
  );
};

const LetterGrid: React.FC<LetterGridProps> = ({ layout, activeWords, fontSettings }) => {
  const grid = createLetterGrid(layout);
  
  // Get all active letter positions
  const activePositions = new Set<string>();
  activeWords.forEach(word => {
    const positions = getLetterPositions(layout, word);
    positions.forEach(pos => {
      activePositions.add(`${pos.row}-${pos.col}`);
    });
  });
  
  // Calculate aspect ratio based on actual cell dimensions
  const totalWidth = layout.gridWidth * fontSettings.cellSpacingX;
  const totalHeight = layout.gridHeight * fontSettings.cellSpacingY;
  const aspectRatio = totalWidth / totalHeight;
  
  // Calculate border and grid line styles
  const borderClass = fontSettings.addBorder ? 'border-2 border-gray-300' : 'border-0';
  const cellBorderStyle = fontSettings.addGridLines 
    ? { borderRight: '1px solid #444', borderBottom: '1px solid #444' }
    : {};
  
  return (
    <div 
      className={`inline-block ${borderClass} p-2 sm:p-4 bg-black w-full max-w-full`}
      style={{
        aspectRatio: aspectRatio.toString(),
      }}
    >
      {grid.map((row, rowIndex) => (
        <div key={rowIndex} className="flex" style={{ gap: '0', marginBottom: '0', height: `${100 / layout.gridHeight}%` }}>
          {row.map((letter, colIndex) => {
            const isActive = activePositions.has(`${rowIndex}-${colIndex}`);
            const isEmpty = letter === ' ';
            
            return (
              <div
                key={`${rowIndex}-${colIndex}`}
                style={{
                  flex: '1 1 0',
                  height: '100%',
                  ...cellBorderStyle,
                }}
              >
                <VectorLetterCell
                  letter={letter}
                  isActive={isActive}
                  isEmpty={isEmpty}
                  fontSettings={fontSettings}
                  cellWidth={fontSettings.cellSpacingX}
                  cellHeight={fontSettings.cellSpacingY}
                />
              </div>
            );
          })}
        </div>
      ))}
    </div>
  );
};

const ClockDisplay: React.FC<ClockDisplayProps> = ({ 
  layout, 
  hours, 
  minutes, 
  fontSettings,
  layoutMetadata 
}) => {
  const militaryTime = convertToMilitaryTime(hours, minutes, layout.name, layoutMetadata);
  
  return (
    <div className="flex flex-col items-center w-full">
      {/* Compact header */}
      <div className="text-center mb-2 w-full">
        <h2 className="text-lg font-bold text-gray-800 mb-1">
          {layout.name}
        </h2>
        <p className="text-sm text-gray-600">
          {militaryTime.description}
        </p>
      </div>
      
      {/* Clock face - scales to fill width */}
      <div className="w-full flex justify-center">
        {layoutMetadata?.hasCategories ? (
          <CategorizedLetterGrid 
            layout={layout}
            activeWordsWithCategory={militaryTime.wordsWithCategory}
            fontSettings={fontSettings}
            layoutMetadata={layoutMetadata}
          />
        ) : (
          <LetterGrid 
            layout={layout}
            activeWords={militaryTime.words}
            fontSettings={fontSettings}
          />
        )}
      </div>
      
      <div className="text-xs text-gray-500 mt-2">
        Active: {militaryTime.words.join(', ')}
      </div>
    </div>
  );
};

export default ClockDisplay;

