#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

/**
 * Automated layout build system
 * 1. Processes any .txt files in grids/ folder using gridAnalyzer
 * 2. Scans public/layouts/ for all JSON files
 * 3. Generates manifest with metadata
 */

const GRIDS_DIR = path.join(__dirname, '..', 'grids');
const LAYOUTS_DIR = path.join(__dirname, '..', 'public', 'layouts');
const OUTPUT_DIR = path.join(__dirname, '..', 'src', 'generated');
const OUTPUT_FILE = path.join(OUTPUT_DIR, 'layouts-manifest.json');
const GRID_ANALYZER = path.join(__dirname, '..', 'tools', 'gridAnalyzer.cjs');

function processGridFiles() {
  console.log('🔍 Scanning for new grid files...');
  
  if (!fs.existsSync(GRIDS_DIR)) {
    console.log('ℹ️  No grids directory found, skipping grid processing');
    return;
  }
  
  const gridFiles = fs.readdirSync(GRIDS_DIR)
    .filter(file => file.endsWith('.txt'))
    .sort();
  
  console.log(`📁 Found ${gridFiles.length} grid files:`, gridFiles);
  
  for (const gridFile of gridFiles) {
    const baseName = path.basename(gridFile, '.txt');
    const outputJsonFile = `${baseName}.json`;
    const outputPath = path.join(LAYOUTS_DIR, outputJsonFile);
    
    // Check if JSON already exists and is newer than the txt file
    const gridPath = path.join(GRIDS_DIR, gridFile);
    const gridStat = fs.statSync(gridPath);
    
    if (fs.existsSync(outputPath)) {
      const jsonStat = fs.statSync(outputPath);
      if (jsonStat.mtime > gridStat.mtime) {
        console.log(`  ✓ ${baseName}.json is up to date`);
        continue;
      }
    }
    
    console.log(`  🔄 Processing ${gridFile} -> ${outputJsonFile}`);
    
    try {
      // Run gridAnalyzer
      const { execSync } = require('child_process');
      execSync(`node "${GRID_ANALYZER}" "${gridPath}" "${outputJsonFile}"`, {
        stdio: 'pipe'
      });
      console.log(`  ✅ Generated ${outputJsonFile}`);
    } catch (error) {
      console.warn(`  ⚠️  Failed to process ${gridFile}:`, error.message);
    }
  }
}

function analyzeLayoutFeatures(layoutData) {
  const features = [];
  const name = layoutData.name;
  
  // Check if layout has OH words (indicates OH prefix support)
  const hasOH = layoutData.words?.some(w => w.word === 'OH');
  if (hasOH) {
    features.push('oh-prefix');
  }
  
  // Check if layout has ZERO words
  const hasZero = layoutData.words?.some(w => w.word === 'ZERO');
  if (hasZero) {
    features.push('zero-word');
  }
  
  // Check if layout has fragment words (THIR, TEEN, FIF, etc.)
  const fragmentWords = ['THIR', 'TEEN', 'FIF'];
  const hasFragments = fragmentWords.some(fragment => 
    layoutData.words?.some(w => w.word === fragment)
  );
  if (hasFragments) {
    features.push('fragment-words');
  }
  
  // Check if layout has category information
  const hasCategories = layoutData.words?.some(w => w.category);
  if (hasCategories) {
    features.push('category-based');
  }
  
  // Analyze minute granularity
  const minuteWords = layoutData.words?.filter(w => w.category === 'minute').map(w => w.word) || [];
  const fiveMinuteWords = ['FIVE', 'TEN', 'FIFTEEN', 'TWENTY', 'THIRTY', 'FORTY', 'FIFTY'];
  const individualMinuteWords = ['ONE', 'TWO', 'THREE', 'FOUR', 'SIX', 'SEVEN', 'EIGHT', 'NINE', 'ELEVEN', 'TWELVE'];
  const compositeMinuteWords = ['TWENTYFIVE', 'THIRTYFIVE', 'FORTYFIVE', 'FIFTYFIVE'];
  
  let minuteGranularity = 'five-minute'; // default
  
  // Check for composite minute words (like TWENTYFIVE)
  const hasCompositeWords = compositeMinuteWords.some(word => minuteWords.includes(word));
  if (hasCompositeWords) {
    features.push('composite-minutes');
  }
  
  // Check for individual minute words
  const hasIndividualWords = individualMinuteWords.some(word => minuteWords.includes(word));
  if (hasIndividualWords) {
    minuteGranularity = 'individual';
    features.push('individual-minutes');
  }
  
  // Determine word mapping type
  let wordMappings = 'standard';
  if (hasFragments) {
    wordMappings = 'fragment';
  } else if (name?.includes('GraceGPT')) {
    wordMappings = 'gracegpt';
  } else if (name?.includes('Crossword')) {
    wordMappings = 'crossword';
  }
  
  // Determine grid generation type
  let gridGeneration = 'hardcoded';
  if (hasCategories && layoutData.words?.length > 0) {
    gridGeneration = 'json-based';
  }
  
  return {
    features,
    wordMappings,
    gridGeneration,
    hasOH,
    hasZero,
    hasFragments,
    hasCategories,
    minuteGranularity,
    minuteWords: minuteWords
  };
}

function generateManifest() {
  console.log('📊 Generating layouts manifest...');
  
  // Ensure output directory exists
  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  }
  
  // Ensure layouts directory exists
  if (!fs.existsSync(LAYOUTS_DIR)) {
    console.warn('⚠️  No layouts directory found!');
    return;
  }
  
  // Read all JSON files from layouts directory
  const files = fs.readdirSync(LAYOUTS_DIR)
    .filter(file => file.endsWith('.json'))
    .sort();
  
  console.log(`📁 Found ${files.length} layout files:`, files.map(f => f.replace('.json', '')));
  
  const layouts = [];
  
  for (const file of files) {
    const layoutPath = path.join(LAYOUTS_DIR, file);
    const layoutName = file.replace('.json', '');
    
    try {
      const layoutContent = fs.readFileSync(layoutPath, 'utf-8');
      const layoutData = JSON.parse(layoutContent);
      
      // Analyze layout features
      const analysis = analyzeLayoutFeatures(layoutData);
      
      const layoutInfo = {
        name: layoutName,
        displayName: layoutData.name || layoutName,
        description: layoutData.description || '',
        gridWidth: layoutData.gridWidth || 0,
        gridHeight: layoutData.gridHeight || 0,
        wordCount: layoutData.words?.length || 0,
        ...analysis
      };
      
      layouts.push(layoutInfo);
      
      console.log(`  ✅ ${layoutName}: ${analysis.features.join(', ')}`);
      
    } catch (error) {
      console.warn(`  ⚠️  Failed to parse ${file}:`, error.message);
    }
  }
  
  const manifest = {
    generated: new Date().toISOString(),
    layouts: layouts,
    summary: {
      total: layouts.length,
      withOH: layouts.filter(l => l.hasOH).length,
      withZero: layouts.filter(l => l.hasZero).length,
      withFragments: layouts.filter(l => l.hasFragments).length,
      withCategories: layouts.filter(l => l.hasCategories).length
    }
  };
  
  // Write manifest file
  fs.writeFileSync(OUTPUT_FILE, JSON.stringify(manifest, null, 2));
  
  console.log('\n🎉 Build completed successfully!');
  console.log(`📄 Manifest: ${OUTPUT_FILE}`);
  console.log(`📊 Summary:`, manifest.summary);
  
  return manifest;
}

function buildLayouts() {
  try {
    console.log('🚀 Starting automated layout build...\n');
    
    // Step 1: Process any new/updated grid files
    processGridFiles();
    
    // Step 2: Generate manifest from all JSON files
    generateManifest();
    
  } catch (error) {
    console.error('❌ Build failed:', error.message);
    process.exit(1);
  }
}

// Run if called directly
if (require.main === module) {
  buildLayouts();
}

module.exports = { buildLayouts };
