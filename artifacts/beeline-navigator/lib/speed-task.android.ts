import * as Location from 'expo-location';
import * as TaskManager from 'expo-task-manager';
import { BeeLineAndroid } from '@/modules/beeline-android';
import { makeSpeedCommand } from '@/lib/speed';

export const SPEED_TASK_NAME = 'beeline-speed-location-updates';

TaskManager.defineTask(SPEED_TASK_NAME, async ({ data, error }) => {
  if (error || !BeeLineAndroid.available) return;
  const locations = (data as { locations?: Location.LocationObject[] } | undefined)?.locations ?? [];
  const location = locations[locations.length - 1];
  if (!location) return;

  const isFresh = Date.now() - location.timestamp <= 10_000;
  const metersPerSecond = location.coords.speed;
  const speedKmh = isFresh && metersPerSecond !== null && metersPerSecond >= 0
    ? metersPerSecond * 3.6
    : null;
  try {
    await BeeLineAndroid.writeCommand(makeSpeedCommand(speedKmh, location.timestamp));
  } catch {
    // BLE may be temporarily disconnected; later location samples retry.
  }
});
