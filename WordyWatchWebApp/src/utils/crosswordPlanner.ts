// Advanced crossword planning utility with optimization algorithms

export interface WordIntersection {
  horizontalWord: string;
  verticalWord: string;
  letter: string;
  horizontalIndex: number;
  verticalIndex: number;
  score: number;
}

export interface PlacedWord {
  word: string;
  startRow: number;
  startCol: number;
  direction: 'horizontal' | 'vertical';
  category: string;
  intersections: WordIntersection[];
}

export interface CrosswordGrid {
  grid: string[][];
  placedWords: PlacedWord[];
  width: number;
  height: number;
  totalIntersections: number;
}

// Hour words (horizontal)
export const HOUR_WORDS = [
  'ZERO', 'ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE',
  'TEN', 'ELEVEN', 'TWELVE', 'THIRTEEN', 'FOURTEEN', 'FIFTEEN', 'SIXTEEN', 
  'SEVENTEEN', 'EIGHTEEN', 'NINETEEN', 'TWENTY', 'TWENTYONE', 'TWENTYTWO', 'TWENTYTHREE'
];

// Minute words (vertical)
export const MINUTE_WORDS = [
  'ZERO', 'FIVE', 'TEN', 'FIFTEEN', 'TWENTY', 'TWENTYFIVE', 
  'THIRTY', 'THIRTYFIVE', 'FORTY', 'FORTYFIVE', 'FIFTY', 'FIFTYFIVE'
];

export function findAllIntersections(): WordIntersection[] {
  const intersections: WordIntersection[] = [];
  
  for (const hWord of HOUR_WORDS) {
    for (const vWord of MINUTE_WORDS) {
      for (let i = 0; i < hWord.length; i++) {
        for (let j = 0; j < vWord.length; j++) {
          if (hWord[i] === vWord[j]) {
            // Score based on letter frequency and position
            const letterScore = getLetterScore(hWord[i]);
            const positionScore = getPositionScore(i, hWord.length, j, vWord.length);
            
            intersections.push({
              horizontalWord: hWord,
              verticalWord: vWord,
              letter: hWord[i],
              horizontalIndex: i,
              verticalIndex: j,
              score: letterScore + positionScore
            });
          }
        }
      }
    }
  }
  
  return intersections.sort((a, b) => b.score - a.score);
}

function getLetterScore(letter: string): number {
  // Common letters get higher scores for better connectivity
  const letterFreq: Record<string, number> = {
    'E': 10, 'T': 9, 'N': 8, 'R': 7, 'I': 6, 'O': 5, 'A': 4, 'S': 3, 'H': 2
  };
  return letterFreq[letter] || 1;
}

function getPositionScore(hIndex: number, hLength: number, vIndex: number, vLength: number): number {
  // Prefer intersections not at the very ends of words
  const hPosition = hIndex / (hLength - 1);
  const vPosition = vIndex / (vLength - 1);
  
  // Score is higher for positions away from ends
  const hScore = Math.sin(hPosition * Math.PI) * 5;
  const vScore = Math.sin(vPosition * Math.PI) * 5;
  
  return hScore + vScore;
}

export function generateOptimalCrossword(): CrosswordGrid {
  const intersections = findAllIntersections();
  const placedWords: PlacedWord[] = [];
  const usedWords = new Set<string>();
  
  // Start with the best intersection
  if (intersections.length > 0) {
    const bestIntersection = intersections[0];
    
    // Place the horizontal word
    const hWord: PlacedWord = {
      word: bestIntersection.horizontalWord,
      startRow: 10, // Start in middle of grid
      startCol: 5,
      direction: 'horizontal',
      category: 'hour',
      intersections: [bestIntersection]
    };
    
    // Place the vertical word at intersection
    const vWord: PlacedWord = {
      word: bestIntersection.verticalWord,
      startRow: 10 - bestIntersection.verticalIndex,
      startCol: 5 + bestIntersection.horizontalIndex,
      direction: 'vertical',
      category: 'minute',
      intersections: [bestIntersection]
    };
    
    placedWords.push(hWord, vWord);
    usedWords.add(bestIntersection.horizontalWord);
    usedWords.add(bestIntersection.verticalWord);
  }
  
  // Find more intersections with already placed words
  for (const intersection of intersections.slice(1)) {
    if (usedWords.has(intersection.horizontalWord) && usedWords.has(intersection.verticalWord)) {
      continue; // Both words already placed
    }
    
    if (usedWords.has(intersection.horizontalWord)) {
      // Try to place the vertical word
      const existingHWord = placedWords.find(w => w.word === intersection.horizontalWord);
      if (existingHWord) {
        const newVWord: PlacedWord = {
          word: intersection.verticalWord,
          startRow: existingHWord.startRow - intersection.verticalIndex,
          startCol: existingHWord.startCol + intersection.horizontalIndex,
          direction: 'vertical',
          category: 'minute',
          intersections: [intersection]
        };
        
        if (canPlaceWord(newVWord, placedWords)) {
          placedWords.push(newVWord);
          usedWords.add(intersection.verticalWord);
        }
      }
    }
    
    if (usedWords.has(intersection.verticalWord)) {
      // Try to place the horizontal word
      const existingVWord = placedWords.find(w => w.word === intersection.verticalWord);
      if (existingVWord) {
        const newHWord: PlacedWord = {
          word: intersection.horizontalWord,
          startRow: existingVWord.startRow + intersection.verticalIndex,
          startCol: existingVWord.startCol - intersection.horizontalIndex,
          direction: 'horizontal',
          category: 'hour',
          intersections: [intersection]
        };
        
        if (canPlaceWord(newHWord, placedWords)) {
          placedWords.push(newHWord);
          usedWords.add(intersection.horizontalWord);
        }
      }
    }
  }
  
  // Fill remaining words in available spaces
  fillRemainingWords(placedWords, usedWords);
  
  // Calculate grid bounds
  const bounds = calculateGridBounds(placedWords);
  const grid = createGrid(placedWords, bounds);
  
  return {
    grid: grid.grid,
    placedWords: normalizeWordPositions(placedWords, bounds),
    width: grid.width,
    height: grid.height,
    totalIntersections: countIntersections(placedWords)
  };
}

function canPlaceWord(newWord: PlacedWord, existingWords: PlacedWord[]): boolean {
  // Check for conflicts with existing words
  for (const existing of existingWords) {
    if (wordsConflict(newWord, existing)) {
      return false;
    }
  }
  return true;
}

function wordsConflict(word1: PlacedWord, word2: PlacedWord): boolean {
  // Get all positions for both words
  const positions1 = getWordPositions(word1);
  const positions2 = getWordPositions(word2);
  
  // Check for overlapping positions with different letters
  for (const pos1 of positions1) {
    for (const pos2 of positions2) {
      if (pos1.row === pos2.row && pos1.col === pos2.col) {
        if (pos1.letter !== pos2.letter) {
          return true; // Conflict
        }
      }
    }
  }
  
  return false;
}

function getWordPositions(word: PlacedWord): Array<{row: number, col: number, letter: string}> {
  const positions = [];
  
  for (let i = 0; i < word.word.length; i++) {
    const row = word.direction === 'horizontal' ? word.startRow : word.startRow + i;
    const col = word.direction === 'horizontal' ? word.startCol + i : word.startCol;
    
    positions.push({
      row,
      col,
      letter: word.word[i]
    });
  }
  
  return positions;
}

function fillRemainingWords(placedWords: PlacedWord[], usedWords: Set<string>) {
  // Add remaining hour words
  for (const hourWord of HOUR_WORDS) {
    if (!usedWords.has(hourWord)) {
      // Find a good spot for this word
      const placement = findAvailableSpot(hourWord, 'horizontal', placedWords);
      if (placement) {
        placedWords.push({
          word: hourWord,
          startRow: placement.row,
          startCol: placement.col,
          direction: 'horizontal',
          category: 'hour',
          intersections: []
        });
        usedWords.add(hourWord);
      }
    }
  }
  
  // Add remaining minute words
  for (const minuteWord of MINUTE_WORDS) {
    if (!usedWords.has(minuteWord)) {
      const placement = findAvailableSpot(minuteWord, 'vertical', placedWords);
      if (placement) {
        placedWords.push({
          word: minuteWord,
          startRow: placement.row,
          startCol: placement.col,
          direction: 'vertical',
          category: 'minute',
          intersections: []
        });
        usedWords.add(minuteWord);
      }
    }
  }
}

function findAvailableSpot(word: string, direction: 'horizontal' | 'vertical', placedWords: PlacedWord[]): {row: number, col: number} | null {
  // Try to find spots near existing words
  const maxAttempts = 100;
  
  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    const row = Math.floor(Math.random() * 20) + 5;
    const col = Math.floor(Math.random() * 20) + 5;
    
    const testWord: PlacedWord = {
      word,
      startRow: row,
      startCol: col,
      direction,
      category: direction === 'horizontal' ? 'hour' : 'minute',
      intersections: []
    };
    
    if (canPlaceWord(testWord, placedWords)) {
      return {row, col};
    }
  }
  
  return null;
}

function calculateGridBounds(words: PlacedWord[]): {minRow: number, maxRow: number, minCol: number, maxCol: number} {
  let minRow = Infinity, maxRow = -Infinity;
  let minCol = Infinity, maxCol = -Infinity;
  
  for (const word of words) {
    const endRow = word.direction === 'horizontal' ? word.startRow : word.startRow + word.word.length - 1;
    const endCol = word.direction === 'horizontal' ? word.startCol + word.word.length - 1 : word.startCol;
    
    minRow = Math.min(minRow, word.startRow);
    maxRow = Math.max(maxRow, endRow);
    minCol = Math.min(minCol, word.startCol);
    maxCol = Math.max(maxCol, endCol);
  }
  
  return {minRow, maxRow, minCol, maxCol};
}

function createGrid(words: PlacedWord[], bounds: {minRow: number, maxRow: number, minCol: number, maxCol: number}): {grid: string[][], width: number, height: number} {
  const width = bounds.maxCol - bounds.minCol + 1;
  const height = bounds.maxRow - bounds.minRow + 1;
  
  const grid: string[][] = Array(height).fill(null).map(() => Array(width).fill(' '));
  
  for (const word of words) {
    for (let i = 0; i < word.word.length; i++) {
      const row = word.direction === 'horizontal' ? word.startRow : word.startRow + i;
      const col = word.direction === 'horizontal' ? word.startCol + i : word.startCol;
      
      const gridRow = row - bounds.minRow;
      const gridCol = col - bounds.minCol;
      
      if (gridRow >= 0 && gridRow < height && gridCol >= 0 && gridCol < width) {
        grid[gridRow][gridCol] = word.word[i];
      }
    }
  }
  
  return {grid, width, height};
}

function normalizeWordPositions(words: PlacedWord[], bounds: {minRow: number, maxRow: number, minCol: number, maxCol: number}): PlacedWord[] {
  return words.map(word => ({
    ...word,
    startRow: word.startRow - bounds.minRow,
    startCol: word.startCol - bounds.minCol
  }));
}

function countIntersections(words: PlacedWord[]): number {
  let intersectionCount = 0;
  
  for (let i = 0; i < words.length; i++) {
    for (let j = i + 1; j < words.length; j++) {
      if (words[i].direction !== words[j].direction) {
        const positions1 = getWordPositions(words[i]);
        const positions2 = getWordPositions(words[j]);
        
        for (const pos1 of positions1) {
          for (const pos2 of positions2) {
            if (pos1.row === pos2.row && pos1.col === pos2.col && pos1.letter === pos2.letter) {
              intersectionCount++;
            }
          }
        }
      }
    }
  }
  
  return intersectionCount;
}
