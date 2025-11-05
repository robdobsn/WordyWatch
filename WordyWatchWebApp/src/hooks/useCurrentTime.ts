import { useState, useEffect } from 'react';
import { getCurrentTime } from '../utils/militaryTime';

export function useCurrentTime(enabled: boolean) {
  const [currentTime, setCurrentTime] = useState(() => getCurrentTime());

  useEffect(() => {
    if (!enabled) return;

    const updateTime = () => {
      setCurrentTime(getCurrentTime());
    };

    // Update immediately
    updateTime();

    // Update every second
    const interval = setInterval(updateTime, 1000);

    return () => clearInterval(interval);
  }, [enabled]);

  return currentTime;
}

