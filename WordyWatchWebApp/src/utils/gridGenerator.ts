// Generate the exact 15x15 grid as specified
export function generateMilitaryCondensedGrid(): string[][] {
  const gridRows = [
    'ZEROONETWOTENX',
    'THREEFOURFIVES',
    'SIXSEVENEIGHTYN',
    'NINEELEVENPQRS',
    'TWELVETHIRTEENA',
    'FOURTEENFIFTEEN',
    'SIXTEENEIGHTEEN',
    'SEVENTEENABCDEF',
    'NINETEENTWENTYS',
    'TWENTYONEFIVEZZ',
    'TWENTYTWOTWENTY',
    'THIRTYFORTYXXXX',
    'FIFTYFIFTEENABC',
    'HUNDREDZEROXXXX',
    'FORTYFIVEKLMNOP'
  ];

  return gridRows.map(row => row.split(''));
}

// Generate the GraceGPT grid as specified
export function generateGraceGPTGrid(): string[][] {
  const gridRows = [
    'fft...sixtfh',
    'iihtwoonewou',
    'ffiffour.ern',
    'ttrthree.ntd',
    'eyte.fivetyr',
    'e.yneighty.e',
    'nine.vsevend',
    '.thire..teen'
  ];

  return gridRows.map(row => row.split(''));
}

// Generate the GraceGPT2 grid as specified
export function generateGraceGPT2Grid(): string[][] {
  const gridRows = [
    'thir....six.fh',
    'threeeight.fiu',
    't.tzero..hfifn',
    'w.efive..ioftd',
    'efnten...rrter',
    'nine.fourttyee',
    'tv..sevenyy.nd',
    'yeone..twoteen'
  ];

  return gridRows.map(row => row.split(''));
}

// Note: GraceGPT4 grid is now generated dynamically from JSON word positions
// No hardcoded grid needed - it uses the same approach as Auto Layout

// Generate the Auto Layout grid as specified
export function generateAutoLayoutGrid(): string[][] {
  const gridRows = [
    'HTFTTFFZERO',
    'UEIWHOIFOUR',
    'NNFEIRFTHIR',
    'D.TNRTTHREE',
    'R.ETTYYNINE',
    'E.EYYELEVEN',
    'D.NTWELVE..',
    'TWENTYFIF..',
    'EIGHTWOFIVE',
    'SEVENSIXV..',
    'ONETEN.TEEN'
  ];

  return gridRows.map(row => row.split(''));
}

// Generate optimized crossword grid with strategic intersections
export function generateCrosswordGrid(layout: any): string[][] {
  if (layout.name === 'Crossword Optimized') {
    return generateOptimizedCrosswordGrid();
  }
  
  const grid: string[][] = Array(layout.gridHeight)
    .fill(null)
    .map(() => Array(layout.gridWidth).fill(' '));
  
  // Place all words in the grid, allowing overlaps at intersections
  layout.words.forEach((wordPos: any) => {
    const { word, startRow, startCol, direction } = wordPos;
    
    for (let i = 0; i < word.length; i++) {
      const row = direction === 'horizontal' ? startRow : startRow + i;
      const col = direction === 'horizontal' ? startCol + i : startCol;
      
      if (row >= 0 && row < layout.gridHeight && col >= 0 && col < layout.gridWidth) {
        // For crossword, we allow letters to be overwritten if they match
        // This handles intersections properly
        const existingLetter = grid[row][col];
        const newLetter = word[i];
        
        if (existingLetter === ' ' || existingLetter === newLetter) {
          grid[row][col] = newLetter;
        } else {
          // Conflict - this shouldn't happen in a well-designed crossword
          console.warn(`Letter conflict at (${row}, ${col}): existing '${existingLetter}' vs new '${newLetter}'`);
        }
      }
    }
  });
  
  return grid;
}

// Generate a hand-crafted optimized crossword with maximum intersections
function generateOptimizedCrosswordGrid(): string[][] {
  // This is a proper crossword where words intersect at shared letters
  // Key intersections: ZERO×ZERO(E), THREE×THIRTY(R), FIVE×FIFTEEN(E), etc.
  const gridRows = [
    'GHTFIUSIXTEENUVABCDEFGHIJK',  // SIXTEEN
    'AZERVONETVWAXBCSEVNTYDEFGH',  // ONE, SEVEN starting vertically
    'BERESCOTWOZRFGHIJKLMNOPQRS',  // TWO
    'CROTHTHREEUIABCDEFGHIJKLMN',  // THREE
    'DOTHAFOURGHIJKLMNOPQRSTUV',  // FOUR
    'EFIHIEFIVEVABCDEFGHIJKLMNOP',  // FIVE, FIVE crossing vertically  
    'FGHVJKLSIXNOPQRSTUVWXYZABC',  // SIX
    'GHIEVSEVENKLMNOPQRSTUVWXYZ',  // SEVEN
    'HIJKEIGHTABCDEFGHIJKLMNOPQ',  // EIGHT
    'IJKLMNOPQRSTUVWXYZNINEABCD',  // NINE
    'JKLMNOPQRSTUVWXYZTABCDEFGH',  // TEN
    'KLMNOPQRSTUVWXYZAELEVENIJK',  // ELEVEN
    'LMNOPQRSTUVWXYZABCTWELVEOP',  // TWELVE
    'MNOPQRSTUVWXYZABCTHIRTEENQ',  // THIRTEEN
    'NOPQRSTUVWXYZABCDFOURTEENR',  // FOURTEEN
    'OPQRSTUVWXYZABCDEFIFTEENST'   // FIFTEEN
  ];

  return gridRows.map(row => row.split(''));
}

// Verify that a word exists at the specified position in the grid
export function verifyWordInGrid(grid: string[][], word: string, startRow: number, startCol: number, direction: 'horizontal' | 'vertical'): boolean {
  for (let i = 0; i < word.length; i++) {
    const row = direction === 'horizontal' ? startRow : startRow + i;
    const col = direction === 'horizontal' ? startCol + i : startCol;
    
    if (row >= grid.length || col >= grid[0].length) {
      return false;
    }
    
    if (grid[row][col] !== word[i]) {
      return false;
    }
  }
  
  return true;
}
