import { useState, useEffect } from 'react';
import { ClockLayout } from '../types/layout';
import layoutsManifest from '../generated/layouts-manifest.json';

// Interface for layout metadata
export interface LayoutMetadata {
  name: string;
  displayName: string;
  description: string;
  gridWidth: number;
  gridHeight: number;
  wordCount: number;
  features: string[];
  wordMappings: 'standard' | 'fragment' | 'gracegpt' | 'crossword';
  gridGeneration: 'hardcoded' | 'json-based';
  hasOH: boolean;
  hasZero: boolean;
  hasFragments: boolean;
  hasCategories: boolean;
  minuteGranularity: 'five-minute' | 'individual';
  minuteWords: string[];
}

// Function to discover available layouts from generated manifest
async function discoverLayouts(): Promise<{ layouts: string[], metadata: Record<string, LayoutMetadata> }> {
  try {
    console.log(`📊 Loading ${layoutsManifest.layouts.length} layouts from manifest (generated: ${layoutsManifest.generated})`);
    
    const availableLayouts: string[] = [];
    const metadata: Record<string, LayoutMetadata> = {};
    
    // Check which layouts are actually available by trying to fetch them
    for (const layoutInfo of layoutsManifest.layouts) {
      try {
        const response = await fetch(`/layouts/${layoutInfo.name}.json`);
        if (response.ok) {
          availableLayouts.push(layoutInfo.name);
          metadata[layoutInfo.name] = layoutInfo as LayoutMetadata;
        }
      } catch (error) {
        console.debug(`Layout ${layoutInfo.name} not accessible:`, error);
      }
    }
    
    console.log(`✅ Found ${availableLayouts.length} available layouts:`, availableLayouts);
    return { layouts: availableLayouts, metadata };
    
  } catch (error) {
    console.error('Error loading layouts manifest:', error);
    
    // Fallback to basic manual discovery
    const fallbackLayouts = ['military-standard', 'compact-vertical', 'military-condensed', 'crossword-one'];
    const fallbackMetadata: Record<string, LayoutMetadata> = {};
    
    const availableLayouts: string[] = [];
    for (const layoutName of fallbackLayouts) {
      try {
        const response = await fetch(`/layouts/${layoutName}.json`);
        if (response.ok) {
          availableLayouts.push(layoutName);
          // Create basic metadata for fallback
          fallbackMetadata[layoutName] = {
            name: layoutName,
            displayName: layoutName.split('-').map(word => 
              word.charAt(0).toUpperCase() + word.slice(1)
            ).join(' '),
            description: 'Legacy layout',
            gridWidth: 0,
            gridHeight: 0,
            wordCount: 0,
            features: ['zero-word'],
            wordMappings: 'standard',
            gridGeneration: 'hardcoded',
            hasOH: false,
            hasZero: true,
            hasFragments: false,
            hasCategories: false,
            minuteGranularity: 'five-minute',
            minuteWords: []
          };
        }
      } catch {
        // Skip unavailable layouts
      }
    }
    
    return { layouts: availableLayouts, metadata: fallbackMetadata };
  }
}

export function useLayout() {
  const [layout, setLayout] = useState<ClockLayout | null>(null);
  const [availableLayouts, setAvailableLayouts] = useState<string[]>([]);
  const [layoutsMetadata, setLayoutsMetadata] = useState<Record<string, LayoutMetadata>>({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadLayout = async (layoutName: string) => {
    try {
      setLoading(true);
      setError(null);
      
      const response = await fetch(`/layouts/${layoutName}.json`);
      if (!response.ok) {
        throw new Error(`Failed to load layout: ${response.statusText}`);
      }
      
      const layoutData: ClockLayout = await response.json();
      setLayout(layoutData);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load layout');
      console.error('Error loading layout:', err);
    } finally {
      setLoading(false);
    }
  };

  const getLayoutMetadata = (layoutName: string): LayoutMetadata | null => {
    return layoutsMetadata[layoutName] || null;
  };

  useEffect(() => {
    // Discover available layouts dynamically from manifest
    const initializeLayouts = async () => {
      try {
        const { layouts, metadata } = await discoverLayouts();
        setAvailableLayouts(layouts);
        setLayoutsMetadata(metadata);
        
        // Load default layout
        if (layouts.length > 0) {
          const preferredLayout = 'gracegpt8';
          const defaultLayout = layouts.includes(preferredLayout) ? preferredLayout : layouts[0];
          loadLayout(defaultLayout);
        }
      } catch (error) {
        console.error('Failed to initialize layouts:', error);
        // Fallbacks 
        const fallbackLayouts = ['gracegpt8', 'military-standard'];
        let loadedFallback = false;
        
        for (const fallbackLayout of fallbackLayouts) {
          try {
            const response = await fetch(`/layouts/${fallbackLayout}.json`);
            if (response.ok) {
              setAvailableLayouts([fallbackLayout]);
              setLayoutsMetadata({});
              loadLayout(fallbackLayout);
              loadedFallback = true;
              break;
            }
          } catch {
            // Continue to next fallback
          }
        }
        
        if (!loadedFallback) {
          setAvailableLayouts(['military-standard']);
          setLayoutsMetadata({});
          loadLayout('military-standard');
        }
      }
    };
    
    initializeLayouts();
  }, []);

  return {
    layout,
    availableLayouts,
    layoutsMetadata,
    loading,
    error,
    loadLayout,
    getLayoutMetadata
  };
}
