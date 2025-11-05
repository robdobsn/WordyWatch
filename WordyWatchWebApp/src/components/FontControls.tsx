import React from 'react';
import { FontSettings } from '../types/layout';

interface FontControlsProps {
  fontSettings: FontSettings;
  onFontChange: (settings: FontSettings) => void;
}

const FontControls: React.FC<FontControlsProps> = ({ fontSettings, onFontChange }) => {
  const fontFamilies = [
    { value: 'Monaco, Menlo, Ubuntu Mono, monospace', label: 'Monaco (Monospace)' },
    { value: 'Arial, sans-serif', label: 'Arial (Sans-serif)' },
    { value: 'Ruler Stencil Regular, Arial, sans-serif', label: 'Ruler Stencil Regular' },
    { value: 'Ruler Stencil Bold, Arial, sans-serif', label: 'Ruler Stencil Bold' },
    { value: 'Ruler Stencil Heavy, Arial, sans-serif', label: 'Ruler Stencil Heavy' },
    { value: 'Ruler Stencil Light, Arial, sans-serif', label: 'Ruler Stencil Light' },
    { value: 'Ruler Stencil Thin, Arial, sans-serif', label: 'Ruler Stencil Thin' },
    { value: 'Warzone Stencil, Arial, sans-serif', label: 'Warzone Stencil' },
    { value: 'Lazer Game Zone, Arial, sans-serif', label: 'Lazer Game Zone' },
    { value: 'Octin Stencil Rg, Arial, sans-serif', label: 'Octin Stencil Rg' },
    { value: 'Ombudsman Alternate, Arial, sans-serif', label: 'Ombudsman Alternate' },
  ];

  const fontWeights = [
    { value: '100', label: 'Thin' },
    { value: '200', label: 'Extra Light' },
    { value: '300', label: 'Light' },
    { value: '400', label: 'Normal' },
    { value: '500', label: 'Medium' },
    { value: '600', label: 'Semi Bold' },
    { value: '700', label: 'Bold' },
    { value: '800', label: 'Extra Bold' },
    { value: '900', label: 'Black' },
  ];

  const handleChange = (property: keyof FontSettings, value: string | number | boolean) => {
    onFontChange({
      ...fontSettings,
      [property]: value
    });
  };

  return (
    <div className="bg-white p-3 rounded-lg shadow-md">
      <h3 className="text-sm font-semibold text-gray-800 mb-3">Font & Layout Settings</h3>
      
      <div className="space-y-3">
        {/* Font Settings - Compact Grid */}
        <div className="grid grid-cols-2 gap-2">
          {/* Font Family */}
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Font
            </label>
            <select
              value={fontSettings.family}
              onChange={(e) => handleChange('family', e.target.value)}
              className="w-full px-2 py-1 text-xs border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
            >
              {fontFamilies.map(font => (
                <option key={font.value} value={font.value}>
                  {font.label}
                </option>
              ))}
            </select>
          </div>

          {/* Font Weight */}
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Weight
            </label>
            <select
              value={fontSettings.weight}
              onChange={(e) => handleChange('weight', e.target.value)}
              className="w-full px-2 py-1 text-xs border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
            >
              {fontWeights.map(weight => (
                <option key={weight.value} value={weight.value}>
                  {weight.label}
                </option>
              ))}
            </select>
          </div>
        </div>

        {/* Letter Margin - Compact */}
        <div>
          <label className="block text-xs font-medium text-gray-700 mb-1">
            Letter Margin: {(fontSettings.letterPaddingPercent * 100).toFixed(1)}%
          </label>
          <div className="flex items-center gap-2">
            <input
              type="range"
              min="0"
              max="40"
              step="0.5"
              value={fontSettings.letterPaddingPercent * 100}
              onChange={(e) => handleChange('letterPaddingPercent', parseFloat(e.target.value) / 100)}
              className="flex-1 h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
            />
            <input
              type="number"
              min={0}
              max={40}
              step={0.5}
              value={fontSettings.letterPaddingPercent * 100}
              onChange={(e) => {
                const val = parseFloat(e.target.value);
                if (!isNaN(val)) handleChange('letterPaddingPercent', val / 100);
              }}
              onBlur={(e) => {
                const val = parseFloat(e.target.value);
                if (isNaN(val)) handleChange('letterPaddingPercent', 0.1);
                else handleChange('letterPaddingPercent', Math.max(0, Math.min(40, val)) / 100);
              }}
              className="w-14 px-1 py-1 text-xs border border-gray-300 rounded"
            />
          </div>
        </div>

        {/* Horizontal Stretch and W Stretch - Compact Grid */}
        <div className="grid grid-cols-2 gap-2">
          {/* Horizontal Stretch */}
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">
              H. Stretch: {fontSettings.horizontalStretch.toFixed(2)}x
            </label>
            <div className="flex items-center gap-2">
              <input
                type="range"
                min="0.5"
                max="2.0"
                step="0.05"
                value={fontSettings.horizontalStretch}
                onChange={(e) => handleChange('horizontalStretch', parseFloat(e.target.value))}
                className="flex-1 h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
              />
              <input
                type="number"
                min={0.5}
                max={2.0}
                step={0.05}
                value={fontSettings.horizontalStretch}
                onChange={(e) => {
                  const val = parseFloat(e.target.value);
                  if (!isNaN(val)) handleChange('horizontalStretch', val);
                }}
                onBlur={(e) => {
                  const val = parseFloat(e.target.value);
                  if (isNaN(val)) handleChange('horizontalStretch', 1.0);
                  else handleChange('horizontalStretch', Math.max(0.5, Math.min(2.0, val)));
                }}
                className="w-14 px-1 py-1 text-xs border border-gray-300 rounded"
              />
            </div>
          </div>

          {/* W Stretch */}
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">
              W Stretch: {fontSettings.wStretch.toFixed(2)}x
            </label>
            <input
              type="number"
              min={0.5}
              max={1.5}
              step={0.05}
              value={fontSettings.wStretch}
              onChange={(e) => {
                const val = parseFloat(e.target.value);
                if (!isNaN(val)) handleChange('wStretch', val);
              }}
              onBlur={(e) => {
                const val = parseFloat(e.target.value);
                if (isNaN(val)) handleChange('wStretch', 1.0);
                else handleChange('wStretch', Math.max(0.5, Math.min(1.5, val)));
              }}
              className="w-full px-2 py-1 text-xs border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
            />
          </div>
        </div>

        {/* Cell Spacing - Compact Grid */}
        <div className="grid grid-cols-2 gap-2">
          {/* Cell Spacing X (mm) */}
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Cell X: {fontSettings.cellSpacingX.toFixed(1)} mm
            </label>
            <div className="flex items-center gap-1">
              <input
                type="range"
                min="2"
                max="5"
                step="0.1"
                value={fontSettings.cellSpacingX}
                onChange={(e) => handleChange('cellSpacingX', parseFloat(e.target.value))}
                className="flex-1 h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
              />
              <input
                type="number"
                min={2}
                max={5}
                step={0.1}
                value={fontSettings.cellSpacingX}
                onChange={(e) => {
                  const val = parseFloat(e.target.value);
                  if (!isNaN(val)) handleChange('cellSpacingX', val);
                }}
                onBlur={(e) => {
                  const val = parseFloat(e.target.value);
                  if (isNaN(val)) handleChange('cellSpacingX', 3);
                  else handleChange('cellSpacingX', Math.max(2, Math.min(5, val)));
                }}
                className="w-12 px-1 py-1 text-xs border border-gray-300 rounded"
              />
            </div>
          </div>

          {/* Cell Spacing Y (mm) */}
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Cell Y: {fontSettings.cellSpacingY.toFixed(1)} mm
            </label>
            <div className="flex items-center gap-1">
              <input
                type="range"
                min="2"
                max="5"
                step="0.1"
                value={fontSettings.cellSpacingY}
                onChange={(e) => handleChange('cellSpacingY', parseFloat(e.target.value))}
                className="flex-1 h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
              />
              <input
                type="number"
                min={2}
                max={5}
                step={0.1}
                value={fontSettings.cellSpacingY}
                onChange={(e) => {
                  const val = parseFloat(e.target.value);
                  if (!isNaN(val)) handleChange('cellSpacingY', val);
                }}
                onBlur={(e) => {
                  const val = parseFloat(e.target.value);
                  if (isNaN(val)) handleChange('cellSpacingY', 3);
                  else handleChange('cellSpacingY', Math.max(2, Math.min(5, val)));
                }}
                className="w-12 px-1 py-1 text-xs border border-gray-300 rounded"
              />
            </div>
          </div>
        </div>

        {/* Margin - Compact */}
        <div>
          <label className="block text-xs font-medium text-gray-700 mb-1">
            Margin: {fontSettings.margin.toFixed(1)} mm
          </label>
          <div className="flex items-center gap-2">
            <input
              type="range"
              min="0"
              max="5"
              step="0.1"
              value={fontSettings.margin}
              onChange={(e) => handleChange('margin', parseFloat(e.target.value))}
              className="flex-1 h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
            />
            <input
              type="number"
              min={0}
              max={5}
              step={0.1}
              value={fontSettings.margin}
              onChange={(e) => {
                const val = parseFloat(e.target.value);
                if (!isNaN(val)) handleChange('margin', val);
              }}
              onBlur={(e) => {
                const val = parseFloat(e.target.value);
                if (isNaN(val)) handleChange('margin', 2);
                else handleChange('margin', Math.max(0, Math.min(5, val)));
              }}
              className="w-14 px-1 py-1 text-xs border border-gray-300 rounded"
            />
          </div>
        </div>

        {/* Display Options - Compact Checkboxes */}
        <div className="flex flex-wrap gap-x-4 gap-y-1 pt-1">
          <label className="flex items-center cursor-pointer">
            <input
              type="checkbox"
              checked={fontSettings.centerHorizontally}
              onChange={(e) => handleChange('centerHorizontally', e.target.checked)}
              className="w-3 h-3 text-blue-600 bg-gray-100 border-gray-300 rounded focus:ring-blue-500 focus:ring-1"
            />
            <span className="ml-1 text-xs text-gray-700">
              Center Horizontally
            </span>
          </label>
          
          <label className="flex items-center cursor-pointer">
            <input
              type="checkbox"
              checked={fontSettings.addBorder}
              onChange={(e) => handleChange('addBorder', e.target.checked)}
              className="w-3 h-3 text-blue-600 bg-gray-100 border-gray-300 rounded focus:ring-blue-500 focus:ring-1"
            />
            <span className="ml-1 text-xs text-gray-700">
              Border
            </span>
          </label>
          
          <label className="flex items-center cursor-pointer">
            <input
              type="checkbox"
              checked={fontSettings.addGridLines}
              onChange={(e) => handleChange('addGridLines', e.target.checked)}
              className="w-3 h-3 text-blue-600 bg-gray-100 border-gray-300 rounded focus:ring-blue-500 focus:ring-1"
            />
            <span className="ml-1 text-xs text-gray-700">
              Grid Lines
            </span>
          </label>
        </div>
      </div>
    </div>
  );
};

export default FontControls;
