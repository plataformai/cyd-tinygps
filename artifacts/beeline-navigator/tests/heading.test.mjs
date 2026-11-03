import test from 'node:test';
import assert from 'node:assert/strict';
import { gpsCourse, courseLabel, makeHeadingCommand, formatRemainingDistance } from '../lib/heading.ts';

const now = 100_000;
const fix = (heading = 45, changes = {}) => ({
  timestamp: now, coords: { speed: 4, accuracy: 10, heading, ...changes },
});
test('uses course of travel, never phone orientation; rejects stationary, inaccurate or stale fixes', () => {
  for (const heading of [0, 45, 90, 180, 270, 359]) assert.equal(gpsCourse(fix(heading), now), heading);
  assert.equal(gpsCourse(fix(360), now), 0);
  for (const changes of [{ speed: 0 }, { speed: 1.49 }, { speed: null }, { accuracy: 36 },
    { accuracy: null }, { accuracy: -1 }, { heading: null }, { heading: -1 }, { heading: 361 },
    { heading: Infinity }, { speed: NaN }]) assert.equal(gpsCourse(fix(45, changes), now), null);
  assert.equal(gpsCourse({ ...fix(), timestamp: now - 5001 }, now), null);
  assert.equal(gpsCourse({ ...fix(), timestamp: now + 1001 }, now), null);
  assert.equal(gpsCourse(fix(45, { speed: 1.5, accuracy: 35 }), now), 45);
});
test('formats all Portuguese cardinals and safely crosses true north', () => {
  for (const [degrees, label] of [[0, '0° N'], [45, '45° NE'], [90, '90° L'], [135, '135° SE'],
    [180, '180° S'], [225, '225° SO'], [270, '270° O'], [315, '315° NO'], [359, '359° N'], [360, '0° N']])
    assert.equal(courseLabel(degrees), label);
  assert.equal(courseLabel(null), '—');
  assert.equal(courseLabel(NaN), '—');
});
test('heading messages fit BLE, with explicit invalidation rather than false north', () => {
  for (const degrees of [0, 45, 359.99, 360, -1, 361, null, NaN, Infinity]) {
    const raw = makeHeadingCommand(degrees, now);
    assert.ok(Buffer.byteLength(raw) <= 180);
    const parsed = JSON.parse(raw);
    assert.equal(parsed.type, 'heading');
    assert.equal(parsed.timestamp, now);
    assert.ok(parsed.valid ? parsed.degrees >= 0 && parsed.degrees < 360 : parsed.degrees === null);
  }
});
test('remaining distance is distinct from maneuver distance, including zero and missing data', () => {
  assert.equal(formatRemainingDistance(null), '—');
  assert.equal(formatRemainingDistance(0), '0 m');
  assert.equal(formatRemainingDistance(900), '900 m');
  assert.equal(formatRemainingDistance(3900), '3,9 km');
});
