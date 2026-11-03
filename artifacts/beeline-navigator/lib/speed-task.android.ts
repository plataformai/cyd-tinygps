import * as Location from 'expo-location';
import * as TaskManager from 'expo-task-manager';
import { BeeLineAndroid } from '@/modules/beeline-android';
import { makeSpeedCommand } from '@/lib/speed';
import { gpsCourse, makeHeadingCommand } from '@/lib/heading';

export const SPEED_TASK_NAME = 'beeline-speed-location-updates';

TaskManager.defineTask(SPEED_TASK_NAME, async ({ data, error }) => {
  if (!BeeLineAndroid.available) return;
  if (error) {
    // Explicitly invalidate course; firmware also has an independent watchdog.
    await BeeLineAndroid.writeCommand(makeHeadingCommand(null)).catch(() => undefined);
    return;
  }
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
    await BeeLineAndroid.writeCommand(makeHeadingCommand(gpsCourse(location), location.timestamp));
  } catch {
    // BLE may be temporarily disconnected; later location samples retry.
  }
});
