import React, { useState } from 'react';
import { ClockLayout, FontSettings } from '../types/layout';
import { downloadDXF } from '../utils/dxfExporter';

interface DXFExportProps {
  layout: ClockLayout;
  fontSettings: FontSettings;
}

// Map CSS font families to font file names
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

const DXFExport: React.FC<DXFExportProps> = ({ layout, fontSettings }) => {
  const [filename, setFilename] = useState('wordclock-grid');
  const [isExporting, setIsExporting] = useState(false);
  const [testMode, setTestMode] = useState(false);
  const [toast, setToast] = useState<{ message: string; type: 'success' | 'error' } | null>(null);

  const showToast = (message: string, type: 'success' | 'error' = 'success') => {
    setToast({ message, type });
    window.clearTimeout((showToast as any)._t);
    (showToast as any)._t = window.setTimeout(() => setToast(null), 3000);
  };

  const handleExport = async () => {
    if (!filename.trim()) {
      showToast('Please enter a filename', 'error');
      return;
    }

    setIsExporting(true);
    
    try {
      // Convert fontSettings to DXF config format
      const config = {
        letterSize: fontSettings.size,
        gridSpacing: fontSettings.cellSpacingX, // X (mm)
        gridSpacingY: fontSettings.cellSpacingY, // Y (mm)
        margin: fontSettings.margin,
        letterPaddingPercent: fontSettings.letterPaddingPercent,
        horizontalStretch: fontSettings.horizontalStretch,
        wStretch: fontSettings.wStretch,
        centerHorizontally: fontSettings.centerHorizontally,
        fontName: mapFontFamilyToFontName(fontSettings.family),
        useVectorPaths: fontSettings.useVectorPaths,
        addBorder: fontSettings.addBorder,
        addGridLines: fontSettings.addGridLines,
        testMode: testMode
      };

      await downloadDXF(layout, filename, config);
      // Show success message (non-blocking toast)
      showToast(`DXF file "${filename}.dxf" downloaded`,'success');
    } catch (error) {
      console.error('Export error:', error);
      showToast('Error exporting DXF file. Please try again.','error');
    } finally {
      setIsExporting(false);
    }
  };

  return (
    <div className="bg-white p-3 rounded-lg shadow-md border relative">
      {toast && (
        <div
          role="status"
          className={`absolute right-2 top-2 px-2 py-1 rounded text-xs shadow transition-opacity z-10 ${
            toast.type === 'success' ? 'bg-green-600 text-white' : 'bg-red-600 text-white'
          }`}
        >
          {toast.message}
        </div>
      )}
      <h3 className="text-sm font-semibold mb-2 text-gray-800">Export to DXF</h3>
      
      <div className="space-y-2">
        {/* Filename and Export in a row */}
        <div className="flex gap-2 items-end">
          <div className="flex-1">
            <label htmlFor="filename" className="block text-xs font-medium text-gray-700 mb-1">
              Filename
            </label>
            <input
              id="filename"
              type="text"
              value={filename}
              onChange={(e) => setFilename(e.target.value)}
              className="w-full px-2 py-1 text-xs border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
              placeholder="wordclock-grid"
            />
          </div>
          
          <button
            onClick={handleExport}
            disabled={isExporting || !filename.trim()}
            className={`px-3 py-1 text-xs rounded font-medium transition-colors whitespace-nowrap ${
              isExporting || !filename.trim()
                ? 'bg-gray-300 text-gray-500 cursor-not-allowed'
                : 'bg-blue-600 text-white hover:bg-blue-700'
            }`}
          >
            {isExporting ? 'Exporting...' : 'Export'}
          </button>
        </div>

        {/* Test Mode Toggle - Compact */}
        <label className="flex items-center gap-2">
          <input
            type="checkbox"
            checked={testMode}
            onChange={(e) => setTestMode(e.target.checked)}
            className="w-3 h-3 rounded"
          />
          <span className="text-xs text-gray-700">
            Test Mode (single letter)
          </span>
        </label>

        {/* Compact info */}
        <p className="text-xs text-gray-500 leading-tight">
          {mapFontFamilyToFontName(fontSettings.family)} • {fontSettings.cellSpacingX}×{fontSettings.cellSpacingY}mm
          {fontSettings.useVectorPaths && ' • Vector'}
          {fontSettings.addBorder && ' • Border'}
          {fontSettings.addGridLines && ' • Grid'}
        </p>
      </div>
    </div>
  );
};

export default DXFExport;
