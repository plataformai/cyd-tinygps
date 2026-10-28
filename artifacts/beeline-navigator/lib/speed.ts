export type SpeedCommand = {
  type: 'speed';
  speedKmh: number | null;
  valid: boolean;
  timestamp: number;
};

export function makeSpeedCommand(speedKmh: number | null, timestamp = Date.now()): string {
  const isValid = speedKmh !== null && Number.isFinite(speedKmh) && speedKmh >= 0;
  const command: SpeedCommand = {
    type: 'speed',
    speedKmh: isValid ? Math.round(speedKmh * 10) / 10 : null,
    valid: isValid,
    timestamp,
  };
  const serialized = JSON.stringify(command);
  // The speed-only protocol is ASCII, so JS character count equals UTF-8 bytes.
  const utf8Bytes = serialized.length;
  if (utf8Bytes > 180) {
    throw new Error('A atualização de velocidade excede o limite BLE de 180 bytes.');
  }
  return serialized;
}
