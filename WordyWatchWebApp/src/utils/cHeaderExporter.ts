import { ClockLayout } from '../types/layout';
import { convertToMilitaryTime } from './militaryTime';
import { LayoutMetadata } from '../hooks/useLayout';

interface LEDPattern {
  hour: number;
  minute: number;
  ledMask: number[];
}

// Convert grid position (row, col) to linear LED index (top-left to bottom-right, row by row)
function gridToLEDIndex(row: number, col: number, gridWidth: number): number {
  return row * gridWidth + col;
}

// Get all LED positions that should be lit for a given time
function getLEDsForTime(
  layout: ClockLayout,
  hours: number,
  minutes: number,
  layoutMetadata?: LayoutMetadata | null
): Set<number> {
  const activeLEDs = new Set<number>();
  
  // Get the military time words for this time
  const militaryTime = convertToMilitaryTime(hours, minutes, layout.name, layoutMetadata);
  
  // Helper function to find word positions
  const findWordWithCategoryPriority = (
    word: string,
    preferredCategory: 'hour' | 'minute' | 'military' | 'connector'
  ): Array<{row: number, col: number}> => {
    const wordInstances = layout.words.filter(w => w.word === word);
    
    if (wordInstances.length === 0) {
      return [];
    }
    
    if (preferredCategory === 'minute') {
      const minuteWords = wordInstances.filter(w => w.category === 'minute');
      if (minuteWords.length > 0) {
        return getPositionsFromWordInstance(minuteWords[0], word);
      }
      
      const fragmentWords = ['FIF', 'TEEN'];
      if (fragmentWords.includes(word)) {
        const verticalHourWords = wordInstances.filter(w => w.category === 'hour' && w.direction === 'vertical');
        if (verticalHourWords.length > 0) {
          return getPositionsFromWordInstance(verticalHourWords[0], word);
        }
      }
      
      return [];
    }
    
    if (preferredCategory === 'hour') {
      const hourWords = wordInstances.filter(w => w.category === 'hour');
      if (hourWords.length > 0) {
        return getPositionsFromWordInstance(hourWords[0], word);
      }
      return [];
    }
    
    if (preferredCategory === 'connector') {
      const verticalConnectorWords = wordInstances.filter(w => w.category === 'connector' && w.direction === 'vertical');
      if (verticalConnectorWords.length > 0) {
        return getPositionsFromWordInstance(verticalConnectorWords[0], word);
      }
      const connectorWords = wordInstances.filter(w => w.category === 'connector');
      if (connectorWords.length > 0) {
        return getPositionsFromWordInstance(connectorWords[0], word);
      }
      return [];
    }
    
    if (preferredCategory === 'military') {
      const militaryWords = wordInstances.filter(w => w.category === 'military');
      if (militaryWords.length > 0) {
        return getPositionsFromWordInstance(militaryWords[0], word);
      }
    }
    
    return [];
  };
  
  const getPositionsFromWordInstance = (
    wordInstance: any,
    word: string
  ): Array<{row: number, col: number}> => {
    const { startRow, startCol, direction } = wordInstance;
    const positions: Array<{row: number, col: number}> = [];
    
    for (let i = 0; i < word.length; i++) {
      const row = direction === 'horizontal' ? startRow : startRow + i;
      const col = direction === 'horizontal' ? startCol + i : startCol;
      positions.push({ row, col });
    }
    
    return positions;
  };
  
  const getLetterPositions = (
    word: string,
    preferredCategory?: 'hour' | 'minute' | 'military' | 'connector'
  ): Array<{row: number, col: number}> => {
    if (preferredCategory && layoutMetadata?.hasCategories) {
      return findWordWithCategoryPriority(word, preferredCategory);
    }
    
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
  };
  
  // Get positions for each active word
  militaryTime.wordsWithCategory.forEach(({ word, category }) => {
    const positions = getLetterPositions(word, category);
    positions.forEach(pos => {
      const ledIndex = gridToLEDIndex(pos.row, pos.col, layout.gridWidth);
      activeLEDs.add(ledIndex);
    });
  });
  
  return activeLEDs;
}

// Convert LED indices to bit array (packed into 32-bit integers)
function ledsToBitArray(leds: Set<number>, totalLEDs: number): number[] {
  const numInts = Math.ceil(totalLEDs / 32);
  const bitArray: number[] = new Array(numInts).fill(0);
  
  leds.forEach(ledIndex => {
    const intIndex = Math.floor(ledIndex / 32);
    const bitIndex = ledIndex % 32;
    bitArray[intIndex] |= (1 << bitIndex);
  });
  
  return bitArray;
}

// Format bit array as C array literal
function formatBitArray(bitArray: number[]): string {
  return bitArray.map(val => {
    // Ensure unsigned 32-bit integer formatting
    const unsigned = val >>> 0; // Convert to unsigned 32-bit
    return `0x${unsigned.toString(16).toUpperCase().padStart(8, '0')}`;
  }).join(', ');
}

// Generate the C header file content
export function generateCHeader(
  layout: ClockLayout,
  layoutMetadata?: LayoutMetadata | null
): string {
  const totalLEDs = layout.gridWidth * layout.gridHeight;
  const minuteGranularity = layoutMetadata?.minuteGranularity === 'individual' ? 1 : 5;
  
  // Generate patterns for all valid times
  const patterns: LEDPattern[] = [];
  
  for (let hour = 0; hour < 24; hour++) {
    for (let minute = 0; minute < 60; minute += minuteGranularity) {
      const leds = getLEDsForTime(layout, hour, minute, layoutMetadata);
      const ledMask = ledsToBitArray(leds, totalLEDs);
      patterns.push({ hour, minute, ledMask });
    }
  }
  
  // Build the header file content
  let header = '';
  
  // Header comment and include guards
  const guardName = `WORDY_WATCH_LED_PATTERNS_H`;
  header += `/*\n`;
  header += ` * Auto-generated from Wordy Watch UI\n`;
  header += ` * Layout: ${layout.name}\n`;
  header += ` * Grid Size: ${layout.gridWidth} x ${layout.gridHeight} (${totalLEDs} LEDs)\n`;
  header += ` * Minute Granularity: ${minuteGranularity} minute${minuteGranularity > 1 ? 's' : ''}\n`;
  header += ` * Generated: ${new Date().toISOString()}\n`;
  header += ` *\n`;
  header += ` * LED Ordering: Top-left to bottom-right, row by row\n`;
  header += ` * Bit Packing: 32-bit integers, LSB = LED 0\n`;
  header += ` */\n\n`;
  header += `#ifndef ${guardName}\n`;
  header += `#define ${guardName}\n\n`;
  
  // Constants
  header += `#include <stdint.h>\n\n`;
  header += `// Layout configuration\n`;
  header += `#define LED_GRID_WIDTH ${layout.gridWidth}\n`;
  header += `#define LED_GRID_HEIGHT ${layout.gridHeight}\n`;
  header += `#define LED_TOTAL_COUNT ${totalLEDs}\n`;
  header += `#define LED_MINUTE_GRANULARITY ${minuteGranularity}\n`;
  header += `#define LED_PATTERNS_COUNT ${patterns.length}\n`;
  header += `#define LED_MASK_WORDS ${Math.ceil(totalLEDs / 32)}\n\n`;
  
  // Pattern structure
  header += `// LED pattern structure\n`;
  header += `typedef struct {\n`;
  header += `    uint8_t hour;    // 0-23\n`;
  header += `    uint8_t minute;  // 0-59 (in ${minuteGranularity}-minute increments)\n`;
  header += `    uint32_t led_mask[${Math.ceil(totalLEDs / 32)}];  // Bit mask for LEDs\n`;
  header += `} led_pattern_t;\n\n`;
  
  // Pattern data array
  header += `// LED patterns lookup table\n`;
  header += `static const led_pattern_t led_patterns[${patterns.length}] = {\n`;
  
  patterns.forEach((pattern, index) => {
    const isLast = index === patterns.length - 1;
    header += `    { ${pattern.hour}, ${pattern.minute}, { ${formatBitArray(pattern.ledMask)} } }`;
    header += isLast ? '\n' : ',\n';
  });
  
  header += `};\n\n`;
  
  // Lookup function implementation
  header += `// Get LED pattern for specified time (with rounding to nearest valid minute)\n`;
  header += `static inline const led_pattern_t* get_led_pattern(uint8_t hour, uint8_t minute) {\n`;
  header += `    // Normalize inputs\n`;
  header += `    hour = hour % 24;\n`;
  header += `    \n`;
  header += `    // Round minute to nearest granularity\n`;
  header += `    minute = ((minute + ${minuteGranularity / 2}) / ${minuteGranularity}) * ${minuteGranularity};\n`;
  header += `    if (minute >= 60) {\n`;
  header += `        minute = 0;\n`;
  header += `        hour = (hour + 1) % 24;\n`;
  header += `    }\n`;
  header += `    \n`;
  header += `    // Calculate index into lookup table\n`;
  header += `    uint16_t index = (hour * ${60 / minuteGranularity}) + (minute / ${minuteGranularity});\n`;
  header += `    \n`;
  header += `    // Bounds check\n`;
  header += `    if (index >= LED_PATTERNS_COUNT) {\n`;
  header += `        return NULL;\n`;
  header += `    }\n`;
  header += `    \n`;
  header += `    return &led_patterns[index];\n`;
  header += `}\n\n`;
  
  // Helper function to check if specific LED is on
  header += `// Check if a specific LED should be on for the given pattern\n`;
  header += `static inline uint8_t is_led_on(const led_pattern_t* pattern, uint16_t led_index) {\n`;
  header += `    if (led_index >= LED_TOTAL_COUNT || pattern == NULL) {\n`;
  header += `        return 0;\n`;
  header += `    }\n`;
  header += `    \n`;
  header += `    uint16_t word_index = led_index / 32;\n`;
  header += `    uint8_t bit_index = led_index % 32;\n`;
  header += `    \n`;
  header += `    return (pattern->led_mask[word_index] & (1U << bit_index)) ? 1 : 0;\n`;
  header += `}\n\n`;
  
  // Close include guard
  header += `#endif // ${guardName}\n`;
  
  return header;
}

// Download the C header file
export async function downloadCHeader(
  layout: ClockLayout,
  filename: string,
  layoutMetadata?: LayoutMetadata | null
): Promise<void> {
  try {
    const headerContent = generateCHeader(layout, layoutMetadata);
    
    // Create blob and download
    const blob = new Blob([headerContent], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `${filename}.h`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  } catch (error) {
    console.error('Error generating C header:', error);
    throw error;
  }
}
