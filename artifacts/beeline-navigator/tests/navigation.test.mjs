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

test('forwards text guidance while keeping destination summary separate', () => {
  for (const fields of [
    { text: 'em direcao a R. Exemplo\n13min 4,3 km chegada 11:20' },
    { bigText: 'em direção a R. Exemplo 13min 4,3 km chegada 11:20' },
  ]) {
    const turn = parseMapsInstruction(notification(fields));
    assert.equal(turn?.maneuver, 'text');
    assert.equal(turn?.distanceMeters, null);
    assert.match(turn?.road ?? '', /^em dire[cç][aã]o a R\. Exemplo$/);
    const wire = JSON.parse(makeNavigationCommand(turn, 42));
    assert.equal(wire.maneuver, 'text');
    assert.equal(wire.road, 'em direcao a R. Exemplo');
    assert.ok(!Object.hasOwn(wire, 'distanceMeters'));
    assert.ok(!JSON.stringify(wire).includes('4,3'));
  }
});

test('contextual text shows only an explicitly associated distance', () => {
  for (const [fields, distanceMeters] of [
    [{ text: '250 m · em direção a R. Exemplo · 13 min · 4,3 km · Chegada 11:20' }, 250],
    [{ title: '0 m', text: 'em direção a R. Exemplo', subText: '13 min · 4,3 km · Chegada 11:20' }, 0],
    [{ text: '0,4 km em direção a R. Exemplo · 13 min · 4,3 km' }, 400],
    [{ textLines: '1,2 km · em direção a R. Exemplo\n13 min · 4,3 km' }, 1200],
    [{ title: '0 m', text: 'em direção a R. Exemplo', textLines: '100 m' }, 100],
    [{ text: '0 m · em direção a R. Exemplo · 100 m · 13 min · 4,3 km · Chegada 11:20' }, 100],
    [{ text: 'em direção a R. Exemplo · 100 m' }, 100],
  ]) {
    const turn = parseMapsInstruction(notification(fields));
    assert.equal(turn?.maneuver, 'text');
    assert.equal(turn?.distanceMeters, distanceMeters);
    const wire = JSON.parse(makeNavigationCommand(turn, 42));
    assert.equal(wire.distanceMeters, distanceMeters);
    assert.equal(wire.road, 'em direcao a R. Exemplo');
    assert.ok(!JSON.stringify(wire).includes('4,3'));
  }
  for (const fields of [
    { text: 'em direção a R. Exemplo · 13 min · 4,3 km · Chegada 11:20' },
    { text: 'em direção a R. Exemplo · 4,3 km · 13 min · Chegada 11:20' },
  ]) {
    assert.equal(parseMapsInstruction(notification(fields))?.distanceMeters, null);
  }
});

test('street names do not generate arrows; explicit maneuvers still take precedence', () => {
  assert.equal(parseMapsInstruction(notification({ text: 'em direção a Rua Direita' }))?.maneuver, 'text');
  assert.equal(parseMapsInstruction(notification({
    title: 'Vire à esquerda em 200 m',
    text: 'em direção a Rua Direita',
  }))?.maneuver, 'left');
  for (const [text, maneuver] of [
    ['Siga em frente', 'straight'], ['Mantenha-se à esquerda', 'slight_left'],
    ['Vire à esquerda', 'left'], ['Mantenha-se à direita', 'slight_right'],
    ['Vire à direita', 'right'], ['Faça o retorno', 'uturn'],
  ]) {
    assert.equal(parseMapsInstruction(notification({ text }))?.maneuver, maneuver);
  }
});

test('text guidance respects BLE bytes, handles accents and truncates with an ellipsis', () => {
  const turn = parseMapsInstruction(notification({
    text: 'em direção a Avenida ' + 'São João "Teste" '.repeat(30),
  }));
  const command = makeNavigationCommand(turn, 1761600000000);
  assert.ok(Buffer.byteLength(command) <= 180);
  assert.match(JSON.parse(command).road, /Sao Joao/);
  assert.match(JSON.parse(command).road, /\.\.\.$/);
  assert.equal(parseMapsInstruction(notification({ text: 'em direção a' })), null);
  assert.equal(parseMapsInstruction(notification({ text: '13min 4,3 km chegada 11:20' })), null);
});

test('distance and road without maneuver are sent, not discarded or replaced by destination distance', () => {
  for (const [distance, expected, destination] of [
    ['5,6 km', 5600, '7,7 km'],
    ['4,9 km', 4900, '7,0 km'],
    ['950 m', 950, '1,5 km'],
    ['0 m', 0, '1,5 km'],
  ]) {
    for (const fields of [
      { title: distance, text: 'Al. Exemplo', subText: `13min · ${destination} · Chegada: 12:58` },
      { text: `${distance} · Al. Exemplo · 13min · ${destination} · Chegada: 12:58` },
    ]) {
      const turn = parseMapsInstruction(notification(fields));
      assert.equal(turn?.maneuver, 'text');
      assert.equal(turn?.road, 'Al. Exemplo');
      assert.equal(turn?.distanceMeters, expected);
      const wire = JSON.parse(makeNavigationCommand(turn, 42));
      assert.equal(wire.distanceMeters, expected);
      assert.equal(wire.road, 'Al. Exemplo');
    }
  }
});

test('bare road names do not imply arrows or borrow journey distance', () => {
  assert.equal(parseMapsInstruction(notification({
    title: '100 m', text: 'Rua Direita', subText: '5min · 2 km · Chegada: 12:58',
  }))?.maneuver, 'text');
  assert.equal(parseMapsInstruction(notification({
    text: 'Al. Exemplo', subText: '5min · 2 km · Chegada: 12:58',
  }))?.distanceMeters, null);
  assert.equal(parseMapsInstruction(notification({
    text: '5min · 2 km · Chegada: 12:58',
  })), null);
  assert.equal(parseMapsInstruction(notification({
    title: '100 m', text: 'Praça Exemplo', subText: '5min · 2 km',
  }))?.distanceMeters, 100);
});
