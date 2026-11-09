// Source contract checks, not Android execution. Device extraction remains a manual check.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const root = new URL('../', import.meta.url);
const kotlin = 'modules/beeline-android/android/src/main/java/expo/modules/beeline/';
const read = (file) => readFileSync(new URL(file, root), 'utf8');

test('diagnostics use public icon/layout APIs with bounded extraction', () => {
  const source = read(kotlin + 'MapsNotificationDiagnostic.kt');
  for (const value of ['smallIcon', 'getLargeIcon()', 'contentView', 'bigContentView',
    'headsUpContentView', 'contentDescription', 'EXTRA_TEXT_LINES', 'android.largeIcon.big']) {
    assert.ok(source.includes(value), value);
  }
  assert.match(source, /images\.length\(\) >= 8/);
  assert.match(source, /visited > 160/);
  assert.match(source, /depth > 12/);
  assert.match(source, /require\(sbn\.packageName == "com\.google\.android\.apps\.maps"\)/);
  assert.doesNotMatch(source, /getDeclaredField|AccessibilityService|MediaProjection|writeText|FileOutputStream/);
});

test('diagnostic capture is explicit and separate from navigation transport', () => {
  const module = read(kotlin + 'BeeLineAndroidModule.kt');
  assert.match(module, /AsyncFunction\("captureMapsDiagnostic"\)/);
  assert.match(read('modules/beeline-android/index.ts'), /captureMapsDiagnostic.*Promise<string>/);
  const ui = read('components/MapsDiagnostic.tsx');
  assert.match(ui, /onPress=\{\(\) => void capture\(\)\}/);
  assert.match(ui, /Share\.share/);
  assert.match(ui, /Compartilhar diagnóstico\?/);
  assert.match(ui, /setReport\(null\)/);
  assert.match(ui, /maps-diagnostic-clear/);
  assert.doesNotMatch(ui, /writeCommand|fetch\(|AsyncStorage|useEffect/);
});

test('only Maps navigation notifications feed guidance and arrow samples', () => {
  const service = read(kotlin + 'BeeLineNotificationListenerService.kt');
  assert.match(service, /CATEGORY_NAVIGATION/);
  assert.match(service, /packageName != MAPS_PACKAGE \|\| !isNavigation\(sbn\)\) return/);
  assert.match(service, /ArrowSamples\.record\(this, sbn\)/);
  assert.match(service, /\.filter \{ isNavigation\(it\) \}/);
  const summary = /\b\d+\s*(?:min|h)\b.*\b\d+(?:[.,]\d+)?\s*(?:km|m)\b/i;
  assert.ok(summary.test('16min · 3,4 km · Chegada: 20:21'));
  assert.ok(!summary.test('Fechado hoje'));
  assert.ok(!summary.test('Aberto hoje: 11:00–14:30'));
});

test('arrow samples are bounded, in memory and never transported over BLE', () => {
  const source = read(kotlin + 'ArrowSamples.kt');
  assert.match(source, /MAX_SAMPLES = 30/);
  assert.match(source, /getLargeIcon\(\)/);
  assert.doesNotMatch(source, /SharedPreferences|FileOutputStream|writeCommand/);
  const ui = read('components/MapsDiagnostic.tsx');
  assert.match(ui, /Compartilhar setas coletadas\?/);
  assert.match(ui, /clearArrowSamples/);
});
