// GPS course of travel, not a magnetometer/device-orientation reading.
export type CourseFix = {
  timestamp: number;
  coords: { speed: number | null; heading: number | null; accuracy: number | null };
};

export function gpsCourse(fix: CourseFix, now = Date.now()): number | null {
  const { speed, heading, accuracy } = fix.coords;
  const age = now - fix.timestamp;
  if (age < -1000 || age > 5000 || speed === null || !Number.isFinite(speed) || speed < 1.5 ||
      accuracy === null || !Number.isFinite(accuracy) || accuracy < 0 || accuracy > 35 ||
      heading === null || !Number.isFinite(heading) || heading < 0 || heading > 360) return null;
  return heading % 360;
}

export function courseLabel(degrees: number | null): string {
  if (degrees === null || !Number.isFinite(degrees)) return '—';
  const angle = ((degrees % 360) + 360) % 360;
  const cardinal = ['N', 'NE', 'L', 'SE', 'S', 'SO', 'O', 'NO'][Math.floor((angle + 22.5) / 45) % 8];
  return `${Math.round(angle) % 360}° ${cardinal}`;
}

export function makeHeadingCommand(degrees: number | null, timestamp = Date.now()): string {
  const valid = degrees !== null && Number.isFinite(degrees) && degrees >= 0 && degrees <= 360;
  return JSON.stringify({
    type: 'heading', degrees: valid ? Math.round((degrees % 360) * 10) / 10 % 360 : null,
    valid, timestamp,
  });
}

export function formatRemainingDistance(meters: number | null): string {
  if (meters === null || !Number.isFinite(meters) || meters < 0) return '—';
  return meters < 1000 ? `${Math.round(meters)} m` : `${(meters / 1000).toLocaleString('pt-BR', { maximumFractionDigits: 1 })} km`;
}
