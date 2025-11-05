import React from 'react';
import { TimeSettings } from '../types/layout';
import { LayoutMetadata } from '../hooks/useLayout';

interface TimeControlsProps {
  timeSettings: TimeSettings;
  onTimeChange: (settings: TimeSettings) => void;
  layoutMetadata?: LayoutMetadata | null;
}

const TimeControls: React.FC<TimeControlsProps> = ({ timeSettings, onTimeChange, layoutMetadata }) => {
  const handleHoursChange = (hours: number) => {
    onTimeChange({ ...timeSettings, hours });
  };

  const handleMinutesChange = (minutes: number) => {
    onTimeChange({ ...timeSettings, minutes });
  };

  const handleCurrentTimeToggle = (useCurrentTime: boolean) => {
    onTimeChange({ ...timeSettings, useCurrentTime });
  };

  // Generate hour options (0-23)
  const hourOptions = Array.from({ length: 24 }, (_, i) => i);
  
  // Generate minute options based on layout capabilities
  const isIndividualMinutes = layoutMetadata?.minuteGranularity === 'individual';
  const minuteOptions = isIndividualMinutes 
    ? Array.from({ length: 60 }, (_, i) => i) // 0-59 for individual minutes
    : Array.from({ length: 12 }, (_, i) => i * 5); // 0, 5, 10... for 5-minute intervals
  
  const minuteLabel = isIndividualMinutes ? "Minutes (individual)" : "Minutes (5-minute intervals)";

  return (
    <div className="bg-white p-3 rounded-lg shadow-md">
      {/* Header with selected time */}
      <div className="flex items-center justify-between mb-3">
        <h3 className="text-sm font-semibold text-gray-800">Time Settings</h3>
        <div className="font-mono text-lg font-bold text-blue-600">
          {timeSettings.hours.toString().padStart(2, '0')}:
          {timeSettings.minutes.toString().padStart(2, '0')}
        </div>
      </div>
      
      {/* Compact controls in a flex row that wraps */}
      <div className="flex flex-wrap gap-3 items-center">
        {/* Current Time Toggle */}
        <label className="flex items-center cursor-pointer">
          <input
            type="checkbox"
            checked={timeSettings.useCurrentTime}
            onChange={(e) => handleCurrentTimeToggle(e.target.checked)}
            className="w-4 h-4 text-blue-600 bg-gray-100 border-gray-300 rounded focus:ring-blue-500 focus:ring-2"
          />
          <span className="ml-2 text-xs font-medium text-gray-700">
            Current Time
          </span>
        </label>

        {/* Hours Control */}
        <div className={`flex items-center gap-1 ${timeSettings.useCurrentTime ? 'opacity-50 pointer-events-none' : ''}`}>
          <label className="text-xs text-gray-600 mr-1">Hours:</label>
          <button
            onClick={() => handleHoursChange(Math.max(0, timeSettings.hours - 1))}
            className="px-2 py-1 bg-gray-200 hover:bg-gray-300 rounded text-xs"
            disabled={timeSettings.useCurrentTime}
          >
            -
          </button>
          <select
            value={timeSettings.hours}
            onChange={(e) => handleHoursChange(parseInt(e.target.value))}
            disabled={timeSettings.useCurrentTime}
            className="px-2 py-1 text-xs border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
          >
            {hourOptions.map(hour => (
              <option key={hour} value={hour}>
                {hour.toString().padStart(2, '0')}
              </option>
            ))}
          </select>
          <button
            onClick={() => handleHoursChange(Math.min(23, timeSettings.hours + 1))}
            className="px-2 py-1 bg-gray-200 hover:bg-gray-300 rounded text-xs"
            disabled={timeSettings.useCurrentTime}
          >
            +
          </button>
        </div>

        {/* Minutes Control */}
        <div className={`flex items-center gap-1 ${timeSettings.useCurrentTime ? 'opacity-50 pointer-events-none' : ''}`}>
          <label className="text-xs text-gray-600 mr-1">
            Min{isIndividualMinutes ? '' : ' (5m)'}:
          </label>
          <button
            onClick={() => {
              const currentIndex = minuteOptions.indexOf(timeSettings.minutes);
              const newIndex = Math.max(0, currentIndex - 1);
              handleMinutesChange(minuteOptions[newIndex]);
            }}
            className="px-2 py-1 bg-gray-200 hover:bg-gray-300 rounded text-xs"
            disabled={timeSettings.useCurrentTime}
          >
            -
          </button>
          <select
            value={timeSettings.minutes}
            onChange={(e) => handleMinutesChange(parseInt(e.target.value))}
            disabled={timeSettings.useCurrentTime}
            className="px-2 py-1 text-xs border border-gray-300 rounded focus:ring-1 focus:ring-blue-500"
          >
            {minuteOptions.map(minute => (
              <option key={minute} value={minute}>
                {minute.toString().padStart(2, '0')}
              </option>
            ))}
          </select>
          <button
            onClick={() => {
              const currentIndex = minuteOptions.indexOf(timeSettings.minutes);
              const newIndex = Math.min(minuteOptions.length - 1, currentIndex + 1);
              handleMinutesChange(minuteOptions[newIndex]);
            }}
            className="px-2 py-1 bg-gray-200 hover:bg-gray-300 rounded text-xs"
            disabled={timeSettings.useCurrentTime}
          >
            +
          </button>
        </div>
      </div>
    </div>
  );
};

export default TimeControls;

