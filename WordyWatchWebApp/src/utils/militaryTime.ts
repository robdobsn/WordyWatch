import { LayoutMetadata } from '../hooks/useLayout';

export interface MilitaryTimeWord {
  word: string;
  category: 'hour' | 'minute' | 'military';
}

export interface MilitaryTimeWords {
  words: string[];
  wordsWithCategory: MilitaryTimeWord[];
  description: string;
}

const HOUR_WORDS: Record<number, string[]> = {
  0: ['ZERO'],
  1: ['ONE'],
  2: ['TWO'], 
  3: ['THREE'],
  4: ['FOUR'],
  5: ['FIVE'],
  6: ['SIX'],
  7: ['SEVEN'],
  8: ['EIGHT'],
  9: ['NINE'],
  10: ['TEN'],
  11: ['ELEVEN'],
  12: ['TWELVE'],
  13: ['THIRTEEN'],
  14: ['FOURTEEN'],
  15: ['FIFTEEN'],
  16: ['SIXTEEN'],
  17: ['SEVENTEEN'],
  18: ['EIGHTEEN'],
  19: ['NINETEEN'],
  20: ['TWENTY'],
  21: ['TWENTY', 'ONE'],
  22: ['TWENTY', 'TWO'],
  23: ['TWENTY', 'THREE']
};

const MINUTE_WORDS: Record<number, string[]> = {
  0: [], // No minutes for "hundred hours"
  5: ['FIVE'],
  10: ['TEN'],
  15: ['FIF', 'TEEN'],
  20: ['TWENTY'],
  25: ['TWENTY', 'FIVE'],
  30: ['THIRTY'],
  35: ['THIRTY', 'FIVE'],
  40: ['FORTY'],
  45: ['FORTY', 'FIVE'],
  50: ['FIFTY'],
  55: ['FIFTY', 'FIVE']
};

// Alternative minute words for crossword layout (using composite words)
const CROSSWORD_MINUTE_WORDS: Record<number, string[]> = {
  0: [], // No minutes for "hundred hours"
  5: ['FIVE'],
  10: ['TEN'],
  15: ['FIFTEEN'],
  20: ['TWENTY'],
  25: ['TWENTYFIVE'],
  30: ['THIRTY'],
  35: ['THIRTYFIVE'],
  40: ['FORTY'],
  45: ['FORTYFIVE'],
  50: ['FIFTY'],
  55: ['FIFTYFIVE']
};

// GraceGPT specific word mappings
const GRACEGPT_HOUR_WORDS: Record<number, string[]> = {
  0: ['ZERO'],
  1: ['ONE'],
  2: ['TWO'], 
  3: ['THREE'],
  4: ['FOUR'],
  5: ['FIVE'],
  6: ['SIX'],
  7: ['SEVEN'],
  8: ['EIGHT'],
  9: ['NINE'],
  10: ['TEN'],
  11: ['ELEVEN'],
  12: ['TWELVE'],
  13: ['THIR', 'TEEN'],
  14: ['FOUR', 'TEEN'],
  15: ['FIF', 'TEEN'],
  16: ['SIX', 'TEEN'],
  17: ['SEVEN', 'TEEN'],
  18: ['EIGHT', 'TEEN'],
  19: ['NINE', 'TEEN'],
  20: ['TWENTY'],
  21: ['TWENTY', 'ONE'],
  22: ['TWENTY', 'TWO'],
  23: ['TWENTY', 'THREE']
};

const GRACEGPT_MINUTE_WORDS: Record<number, string[]> = {
  0: ['HUNDRED'],
  5: ['FIVE'],
  10: ['TEN'],
  15: ['FIF', 'TEEN'],
  20: ['TWENTY'],
  25: ['TWENTY', 'FIVE'],
  30: ['THIRTY'],
  35: ['THIRTY', 'FIVE'],
  40: ['FORTY'],
  45: ['FORTY', 'FIVE'],
  50: ['FIFTY'],
  55: ['FIFTY', 'FIVE']
};

// Fragment-based word mappings for auto-generated layouts
const FRAGMENT_HOUR_WORDS: Record<number, string[]> = {
  0: ['ZERO'],
  1: ['ONE'],
  2: ['TWO'], 
  3: ['THREE'],
  4: ['FOUR'],
  5: ['FIVE'],
  6: ['SIX'],
  7: ['SEVEN'],
  8: ['EIGHT'],
  9: ['NINE'],
  10: ['TEN'],
  11: ['ELEVEN'],
  12: ['TWELVE'],
  13: ['THIR', 'TEEN'],
  14: ['FOUR', 'TEEN'],
  15: ['FIF', 'TEEN'],
  16: ['SIX', 'TEEN'],
  17: ['SEVEN', 'TEEN'],
  18: ['EIGHT', 'TEEN'],
  19: ['NINE', 'TEEN'],
  20: ['TWENTY'],
  21: ['TWENTY', 'ONE'],
  22: ['TWENTY', 'TWO'],
  23: ['TWENTY', 'THREE']
};

const FRAGMENT_MINUTE_WORDS: Record<number, string[]> = {
  0: ['HUNDRED'],
  5: ['FIVE'],
  10: ['TEN'],
  15: ['FIF', 'TEEN'],
  20: ['TWENTY'],
  25: ['TWENTY', 'FIVE'],
  30: ['THIRTY'],
  35: ['THIRTY', 'FIVE'],
  40: ['FORTY'],
  45: ['FORTY', 'FIVE'],
  50: ['FIFTY'],
  55: ['FIFTY', 'FIVE']
};

// Helper function to check if a layout has specific words
function layoutHasWord(layoutName: string, word: string): boolean {
  // This would need to be enhanced to actually check the layout data
  // For now, we'll use layout name patterns to determine capabilities
  if (layoutName === 'Gracegpt4' || layoutName === 'Gracegpt5' || layoutName === 'Gracegpt6' || layoutName === 'Graceopt1') {
    // gracegpt4 and gracegpt5 have OH but no ZERO
    return word === 'OH';
  }
  // Most other layouts have ZERO
  return word === 'ZERO';
}

// Helper function to get appropriate zero representation
function getZeroWords(layoutMetadata?: LayoutMetadata | null): { words: string[], category: string } {
  // For 00:00, prefer ZERO if available, otherwise use OH
  if (layoutMetadata?.hasZero) {
    return { words: ['ZERO'], category: 'hour' }; // ZERO HUNDRED
  } else if (layoutMetadata?.hasOH) {
    return { words: ['OH'], category: 'hour' }; // OH HUNDRED
  }
  // Default fallback to ZERO
  return { words: ['ZERO'], category: 'hour' };
}

// Helper function to get OH prefix for single-digit hours (01-09)
function getOHPrefix(layoutMetadata: LayoutMetadata | null | undefined, hour: number): { words: string[], category: string } | null {
  // For layouts with OH support, add OH prefix for 01-09:xx
  if (layoutMetadata?.hasOH && hour >= 1 && hour <= 9) {
    // For 01-09, prepend OH (e.g., 02:20 = OH TWO TWENTY)
    return { words: ['OH'], category: 'hour' };
  }
  return null;
}

// Helper function to get appropriate minute connector
function getMinuteConnector(layoutMetadata: LayoutMetadata | null | undefined, minute: number): { words: string[], category: string } | null {
  // For layouts with OH support, use OH connector for XX:05
  if (layoutMetadata?.hasOH && minute === 5) {
    // For xx:05 = OH FIVE (using vertical OH as connector)
    return { words: ['OH'], category: 'connector' };
  }
  return null;
}

export function convertToMilitaryTime(hours: number, minutes: number, layoutName?: string, layoutMetadata?: LayoutMetadata | null): MilitaryTimeWords {
  // Round minutes based on layout capability
  let adjustedMinutes = minutes;
  
  if (layoutMetadata?.minuteGranularity === 'individual') {
    // Support individual minutes - no rounding needed
    adjustedMinutes = minutes;
  } else {
    // Default to 5-minute intervals
    adjustedMinutes = Math.round(minutes / 5) * 5;
  }
  
  // Handle hour rollover if minutes round up to 60
  let adjustedHours = hours;
  
  if (adjustedMinutes >= 60) {
    adjustedMinutes = 0;
    adjustedHours = (adjustedHours + 1) % 24;
  }
  
  const words: string[] = [];
  const wordsWithCategory: MilitaryTimeWord[] = [];
  let description = '';
  
  // Choose word mappings based on layout metadata
  let hourWordsMap = HOUR_WORDS;
  let minuteWordsMap = MINUTE_WORDS;
  
  if (layoutMetadata?.wordMappings === 'crossword') {
    minuteWordsMap = CROSSWORD_MINUTE_WORDS;
  } else if (layoutMetadata?.wordMappings === 'gracegpt') {
    hourWordsMap = GRACEGPT_HOUR_WORDS;
    minuteWordsMap = GRACEGPT_MINUTE_WORDS;
  } else if (layoutMetadata?.wordMappings === 'fragment') {
    // Use fragment-based mappings for auto-generated layouts
    hourWordsMap = FRAGMENT_HOUR_WORDS;
    minuteWordsMap = FRAGMENT_MINUTE_WORDS;
  }
  
  // Get hour words (can be multiple for certain hours)
  let hourWords = hourWordsMap[adjustedHours];
  let allHourWords: string[] = [];
  
  // Handle zero hour specially for layouts with OH
  if (adjustedHours === 0) {
    const zeroRepresentation = getZeroWords(layoutMetadata);
    allHourWords = zeroRepresentation.words;
    words.push(...allHourWords);
    allHourWords.forEach(word => {
      wordsWithCategory.push({ word, category: zeroRepresentation.category as 'hour' });
    });
  } else {
    // Check for OH prefix for single-digit hours (01-09)
    const ohPrefix = getOHPrefix(layoutMetadata, adjustedHours);
    if (ohPrefix) {
      words.push(...ohPrefix.words);
      allHourWords.push(...ohPrefix.words);
      ohPrefix.words.forEach(word => {
        wordsWithCategory.push({ word, category: ohPrefix.category as 'hour' });
      });
    }
    
    // Add the main hour words
    if (hourWords) {
      words.push(...hourWords);
      allHourWords.push(...hourWords);
      hourWords.forEach(word => {
        wordsWithCategory.push({ word, category: 'hour' });
      });
    }
  }
  
  // Handle minutes
  if (adjustedHours === 0) {
    // Special case: ALL 00:xx times use "ZERO HUNDRED" format
    // Always add HUNDRED for 00:xx times
    const zeroMinuteWords = minuteWordsMap[0];
    if (zeroMinuteWords && zeroMinuteWords.length > 0) {
      words.push(...zeroMinuteWords);
      zeroMinuteWords.forEach(word => {
        wordsWithCategory.push({ word, category: 'military' });
      });
    } else {
      words.push('HUNDRED');
      wordsWithCategory.push({ word: 'HUNDRED', category: 'military' });
    }
    
    // Add minute words if not 00:00
    if (adjustedMinutes > 0) {
      const minuteWords = minuteWordsMap[adjustedMinutes];
      if (minuteWords && minuteWords.length > 0) {
        words.push(...minuteWords);
        minuteWords.forEach(word => {
          wordsWithCategory.push({ word, category: 'minute' });
        });
      }
    }
    
    const minuteDesc = adjustedMinutes > 0 ? ` ${minuteWordsMap[adjustedMinutes]?.join(' ') || ''}` : '';
    description = `${allHourWords.join(' ')} hundred${minuteDesc}`;
  } else if (adjustedMinutes === 0) {
    // For other hours with 0 minutes, use "HUNDRED"
    const zeroMinuteWords = minuteWordsMap[0];
    if (zeroMinuteWords && zeroMinuteWords.length > 0) {
      words.push(...zeroMinuteWords);
      zeroMinuteWords.forEach(word => {
        wordsWithCategory.push({ word, category: 'military' });
      });
    } else {
      words.push('HUNDRED');
      wordsWithCategory.push({ word: 'HUNDRED', category: 'military' });
    }
    description = `${allHourWords.join(' ')} hundred`;
  } else {
    // For all other times (01:xx, 02:xx, etc. with minutes > 0)
    // Check for special minute connectors first
    const minuteConnector = getMinuteConnector(layoutMetadata, adjustedMinutes);
    if (minuteConnector) {
      words.push(...minuteConnector.words);
      minuteConnector.words.forEach(word => {
        wordsWithCategory.push({ word, category: minuteConnector.category as 'connector' });
      });
    }
    
    // "Eleven five", "Eleven fifteen", etc.
    const minuteWords = minuteWordsMap[adjustedMinutes];
    if (minuteWords && minuteWords.length > 0) {
      words.push(...minuteWords);
      minuteWords.forEach(word => {
        wordsWithCategory.push({ word, category: 'minute' });
      });
    }
    
    const hourDesc = allHourWords.join(' ');
    const connectorDesc = minuteConnector ? minuteConnector.words.join(' ') + ' ' : '';
    const minuteDesc = minuteWords?.join(' ') || '';
    description = `${hourDesc} ${connectorDesc}${minuteDesc}`;
  }
  
  return {
    words,
    wordsWithCategory,
    description
  };
}

export function getCurrentTime(): { hours: number; minutes: number } {
  const now = new Date();
  return {
    hours: now.getHours(),
    minutes: now.getMinutes()
  };
}
