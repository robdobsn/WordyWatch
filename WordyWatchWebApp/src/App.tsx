import React, { useState, useEffect } from 'react';
import ClockDisplay from './components/ClockDisplay';
import TimeControls from './components/TimeControls';
import FontControls from './components/FontControls';
import DXFExport from './components/DXFExport';
import { useLayout } from './hooks/useLayout';
import { useCurrentTime } from './hooks/useCurrentTime';
import { FontSettings, TimeSettings } from './types/layout';

function App() {
  const { layout, availableLayouts, layoutsMetadata, loading, error, loadLayout, getLayoutMetadata } = useLayout();
  
  const [timeSettings, setTimeSettings] = useState<TimeSettings>({
    hours: 11,
    minutes: 0,
    useCurrentTime: false
  });

  const [fontSettings, setFontSettings] = useState<FontSettings>({
    family: 'Ruler Stencil Regular, Arial, sans-serif',
    weight: '700',
    size: 2,
    cellSpacingX: 2.5,
    cellSpacingY: 3,
    margin: 2,
    letterPaddingPercent: 0.1,
    horizontalStretch: 1.55,
    wStretch: 0.9,
    centerHorizontally: true,
    useVectorPaths: true,
    addBorder: true,
    addGridLines: false
  });

  const currentTime = useCurrentTime(timeSettings.useCurrentTime);

  // Update time settings when current time changes
  useEffect(() => {
    if (timeSettings.useCurrentTime) {
      setTimeSettings(prev => ({
        ...prev,
        hours: currentTime.hours,
        minutes: currentTime.minutes
      }));
    }
  }, [currentTime, timeSettings.useCurrentTime]);

  if (loading) {
    return (
      <div className="min-h-screen bg-gray-100 flex items-center justify-center">
        <div className="text-center">
          <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
          <p className="text-gray-600">Loading word clock...</p>
        </div>
      </div>
    );
  }

  if (error || !layout) {
    return (
      <div className="min-h-screen bg-gray-100 flex items-center justify-center">
        <div className="text-center">
          <div className="text-red-600 text-xl mb-4">⚠️ Error</div>
          <p className="text-gray-600">{error || 'Failed to load layout'}</p>
          <button 
            onClick={() => loadLayout('military-standard')}
            className="mt-4 px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700"
          >
            Retry
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-100 flex flex-col">
      {/* Compact Header */}
      <header className="bg-white shadow-sm border-b">
        <div className="max-w-7xl mx-auto px-3 sm:px-4 py-2">
          <div className="flex items-center justify-between gap-3">
            <h1 className="text-xl sm:text-2xl font-bold text-gray-900">
              Word Clock Grace
            </h1>
            <div className="flex items-center gap-2">
              <label className="text-xs sm:text-sm font-medium text-gray-700 hidden sm:inline">Layout:</label>
              <select
                value={layout.name.toLowerCase().replace(/\s+/g, '-')}
                onChange={(e) => loadLayout(e.target.value)}
                className="px-2 py-1 text-xs sm:text-sm border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
              >
                {availableLayouts.map(layoutName => (
                  <option key={layoutName} value={layoutName}>
                    {layoutName.split('-').map(word => 
                      word.charAt(0).toUpperCase() + word.slice(1)
                    ).join(' ')}
                  </option>
                ))}
              </select>
            </div>
          </div>
        </div>
      </header>

      {/* Main Content - Flex grow to use available space */}
      <main className="flex-1 max-w-7xl mx-auto w-full px-3 sm:px-4 py-3">
        <div className="flex flex-col lg:flex-row gap-3 h-full">
          {/* Clock Display - Full width up to 600px */}
          <div className="flex-1 flex justify-center items-start min-w-0">
            <div className="w-full max-w-[600px]">
              <ClockDisplay 
                layout={layout}
                hours={timeSettings.hours}
                minutes={timeSettings.minutes}
                fontSettings={fontSettings}
                layoutMetadata={getLayoutMetadata(layout.name.toLowerCase().replace(/\s+/g, '-'))}
              />
            </div>
          </div>

          {/* Controls - Compact on the right */}
          <div className="w-full lg:w-80 xl:w-96 space-y-2 flex-shrink-0">
            <TimeControls 
              timeSettings={timeSettings}
              onTimeChange={setTimeSettings}
              layoutMetadata={getLayoutMetadata(layout.name.toLowerCase().replace(/\s+/g, '-'))}
            />
            
            <FontControls
              fontSettings={fontSettings}
              onFontChange={setFontSettings}
            />
            
            <DXFExport layout={layout} fontSettings={fontSettings} />
            
            {/* Layout Info - Inline with controls */}
            <div className="bg-white p-2 rounded-lg shadow-sm border">
              <div className="flex flex-wrap gap-x-3 gap-y-1 text-xs text-gray-600">
                <div><strong>Grid:</strong> {layout.gridWidth}×{layout.gridHeight}</div>
                <div><strong>Words:</strong> {layout.words.length}</div>
              </div>
            </div>
          </div>
        </div>
      </main>

      {/* Compact Footer */}
      <footer className="bg-white border-t py-2">
        <div className="max-w-7xl mx-auto px-3 sm:px-4">
          <div className="text-center text-xs text-gray-500">
            Word Clock Grace • Inspired by{' '}
            <a 
              href="https://www.qlocktwo.com/en-us" 
              target="_blank" 
              rel="noopener noreferrer"
              className="text-blue-600 hover:text-blue-800"
            >
              QlockTwo
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
}

export default App;

