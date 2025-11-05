// Utility to generate and save optimized crossword layout
import { generateOptimalCrossword, PlacedWord } from './crosswordPlanner';

export function generateCrosswordLayout() {
  const crosswordData = generateOptimalCrossword();
  
  console.log('Generated crossword with:', {
    width: crosswordData.width,
    height: crosswordData.height,
    totalWords: crosswordData.placedWords.length,
    totalIntersections: crosswordData.totalIntersections
  });
  
  // Convert to JSON format
  const layoutJSON = {
    name: "Crossword Optimized",
    description: `Optimized crossword layout with ${crosswordData.totalIntersections} intersections`,
    gridWidth: crosswordData.width,
    gridHeight: crosswordData.height,
    words: crosswordData.placedWords.map((word: PlacedWord) => ({
      word: word.word,
      startRow: word.startRow,
      startCol: word.startCol,
      direction: word.direction,
      category: word.category
    }))
  };
  
  console.log('Generated layout JSON:', JSON.stringify(layoutJSON, null, 2));
  
  // Print the grid for visualization
  console.log('\nGenerated grid:');
  crosswordData.grid.forEach((row, i) => {
    console.log(`${i.toString().padStart(2)}: ${row.join('')}`);
  });
  
  return layoutJSON;
}

// Run the generator
if (typeof window === 'undefined') {
  // Only run in Node.js environment
  generateCrosswordLayout();
}
