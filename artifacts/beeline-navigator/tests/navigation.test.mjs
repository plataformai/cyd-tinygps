// Synthetic notification formats, not captures from a user's route.
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  getMapsNotificationText,
  makeNavigationCommand,
  parseMapsInstruction,
} from '../lib/navigation.ts';

const notification = (fields = {}) => ({
  packageName: 'com.google.android.apps.maps',
  title: '',
  text: '',
  bigText: '',
  subText: '',
  textLines: '',
  postedAt: 1,
  ...fields,
});

test('recognizes a Portuguese initial heading instruction', () => {
  const turn = parseMapsInstruction(notification({
    title: '200 m',
    text: 'Siga na direção sul na Rua Exemplo',
  }));
  assert.equal(turn?.maneuver, 'straight');
  assert.equal(turn?.distanceMeters, 200);
  assert.equal(turn?.road, 'Rua Exemplo');
});

test('recognizes English compound compass headings', () => {
  assert.equal(parseMapsInstruction(notification({
    title: 'Head northwest on Example Road',
  }))?.maneuver, 'straight');
});

test('preserves supported turns, distances and text-lines extraction', () => {
  for (const [text, maneuver] of [
    ['Em 1,2 km, vire à direita na Rua Exemplo', 'right'],
    ['Mantenha-se à esquerda em 300 m', 'slight_left'],
    ['Faça o retorno em 50 m', 'uturn'],
    ['Siga em frente por 400 m', 'straight'],
  ]) {
    assert.equal(parseMapsInstruction(notification({ textLines: text }))?.maneuver, maneuver);
  }
  assert.equal(parseMapsInstruction(notification({
    text: 'Em 1,2 km, vire à direita na Rua Exemplo',
  }))?.distanceMeters, 1200);
  assert.match(getMapsNotificationText(notification({ subText: 'Campo adicional' })), /Campo adicional/);
});

test('does not invent a maneuver for missing or incompatible notifications', () => {
  assert.equal(parseMapsInstruction(notification()), null);
  assert.equal(parseMapsInstruction(notification({ text: 'Chegada às 15:30 · 12 min' })), null);
  assert.equal(parseMapsInstruction(notification({
    packageName: 'another.app',
    text: 'Vire à direita',
  })), null);
});

test('keeps the firmware navigation JSON contract and byte limit', () => {
  const turn = parseMapsInstruction(notification({ text: 'Vire à direita em 100 m' }));
  assert.ok(turn);
  const command = makeNavigationCommand(turn, 42);
  assert.deepEqual(JSON.parse(command), {
    type: 'navigation', maneuver: 'right', distanceMeters: 100, road: '', timestamp: 42,
  });
  assert.ok(Buffer.byteLength(command, 'utf8') <= 180);
});
