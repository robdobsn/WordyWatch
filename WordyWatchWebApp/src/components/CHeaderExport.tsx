import React, { useState } from 'react';
import { ClockLayout } from '../types/layout';
import { downloadCHeader } from '../utils/cHeaderExporter';
import { LayoutMetadata } from '../hooks/useLayout';

interface CHeaderExportProps {
  layout: ClockLayout;
  layoutMetadata?: LayoutMetadata | null;
}

const CHeaderExport: React.FC<CHeaderExportProps> = ({ layout, layoutMetadata }) => {
  const [filename, setFilename] = useState('wordclock_patterns');
  const [isExporting, setIsExporting] = useState(false);
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
      await downloadCHeader(layout, filename, layoutMetadata);
      showToast(`C header file "${filename}.h" downloaded`, 'success');
    } catch (error) {
      console.error('Export error:', error);
      showToast('Error exporting C header file. Please try again.', 'error');
    } finally {
      setIsExporting(false);
    }
  };

  const totalLEDs = layout.gridWidth * layout.gridHeight;
  const minuteGranularity = layoutMetadata?.minuteGranularity === 'individual' ? 1 : 5;
  const totalPatterns = 24 * (60 / minuteGranularity);

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
      <h3 className="text-sm font-semibold mb-2 text-gray-800">Export to C Header</h3>
      
      <div className="space-y-2">
        {/* Filename and Export in a row */}
        <div className="flex gap-2 items-end">
          <div className="flex-1">
            <label htmlFor="c-filename" className="block text-xs font-medium text-gray-700 mb-1">
              Filename
            </label>
            <input
              id="c-filename"
              type="text"
              value={filename}
              onChange={(e) => setFilename(e.target.value)}
              className="w-full px-2 py-1 text-xs border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
              placeholder="wordclock_patterns"
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

        {/* Compact info */}
        <p className="text-xs text-gray-500 leading-tight">
          {totalLEDs} LEDs • {minuteGranularity} min granularity • {totalPatterns} patterns
        </p>
      </div>
    </div>
  );
};

export default CHeaderExport;
