#!/usr/bin/env node
/*
 * Verifies the DualSense Edge SDL2 mapping assumptions used by
 * app/streaming/input/gamepad.cpp without requiring physical hardware.
 */

const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(__dirname, '..');
const gamepadPath = path.join(repoRoot, 'app', 'streaming', 'input', 'gamepad.cpp');
const source = fs.readFileSync(gamepadPath, 'utf8');

const paddleKeys = ['paddle1', 'paddle2', 'paddle3', 'paddle4'];
const sdl2PaddleButtons = [20, 19, 18, 17];
const sdl3CompatPaddleButtons = [16, 15, 14, 13];

function fail(message) {
  console.error(`DualSense Edge mapping verification failed: ${message}`);
  process.exit(1);
}

function assert(condition, message) {
  if (!condition) {
    fail(message);
  }
}

function assertSource(pattern, message) {
  assert(pattern.test(source), message);
}

function paddleMappingEntry(index, paddleButtons) {
  return `${paddleKeys[index]}:b${paddleButtons[index]}`;
}

function paddleMappingIndex(mappingEntry) {
  for (let i = 0; i < paddleKeys.length; i++) {
    if (mappingEntry.startsWith(`${paddleKeys[i]}:`)) {
      return i;
    }
  }

  return -1;
}

function paddleRawButtonMappingIndex(mappingEntry, paddleButtons) {
  const separatorIndex = mappingEntry.indexOf(':');
  if (separatorIndex < 0) {
    return -1;
  }

  const binding = mappingEntry.slice(separatorIndex + 1);
  for (let i = 0; i < paddleButtons.length; i++) {
    if (binding === `b${paddleButtons[i]}`) {
      return i;
    }
  }

  return -1;
}

function isMappingMetadataEntry(mappingEntry) {
  return mappingEntry.startsWith('crc:') ||
         mappingEntry.startsWith('face:') ||
         mappingEntry.startsWith('hint:') ||
         mappingEntry.startsWith('platform:') ||
         mappingEntry.startsWith('sdk>=:') ||
         mappingEntry.startsWith('sdk<=:') ||
         mappingEntry.startsWith('type:');
}

function addPaddleMappingChange(changedMappings, mappingIndex, paddleButtons) {
  changedMappings.push(paddleMappingEntry(mappingIndex, paddleButtons));
}

function normalizePaddleMappings(mapping, paddleButtons) {
  const entries = mapping.split(',');
  const updatedEntries = [];
  const foundMappings = new Array(paddleKeys.length).fill(false);
  const changedMappingEntries = new Array(paddleKeys.length).fill(false);
  let hasChangedMappingEntries = false;

  for (const entry of entries) {
    let mappingIndex = paddleMappingIndex(entry);

    if (mappingIndex < 0) {
      mappingIndex = paddleRawButtonMappingIndex(entry, paddleButtons);
      if (mappingIndex >= 0) {
        changedMappingEntries[mappingIndex] = true;
        hasChangedMappingEntries = true;
        continue;
      }

      updatedEntries.push(entry);
      continue;
    }

    if (foundMappings[mappingIndex]) {
      changedMappingEntries[mappingIndex] = true;
      hasChangedMappingEntries = true;
      continue;
    }

    foundMappings[mappingIndex] = true;
    if (entry !== paddleMappingEntry(mappingIndex, paddleButtons)) {
      changedMappingEntries[mappingIndex] = true;
      hasChangedMappingEntries = true;
    }
  }

  let insertionIndex = updatedEntries.findIndex(isMappingMetadataEntry);
  if (insertionIndex < 0) {
    insertionIndex = updatedEntries.length;
    if (updatedEntries.length > 0 && updatedEntries[updatedEntries.length - 1] === '') {
      insertionIndex--;
    }
  }

  for (let i = 0; i < paddleKeys.length; i++) {
    if (!foundMappings[i]) {
      changedMappingEntries[i] = true;
      hasChangedMappingEntries = true;
    }
    updatedEntries.splice(insertionIndex++, 0, paddleMappingEntry(i, paddleButtons));
  }

  const updatedMapping = updatedEntries.join(',');
  if (updatedMapping !== mapping && !hasChangedMappingEntries) {
    for (let i = 0; i < paddleKeys.length; i++) {
      changedMappingEntries[i] = true;
    }
  }

  const changedMappings = [];
  for (let i = 0; i < paddleKeys.length; i++) {
    if (changedMappingEntries[i]) {
      addPaddleMappingChange(changedMappings, i, paddleButtons);
    }
  }

  return {
    mapping: updatedMapping,
    changedMappings: changedMappings.join(','),
  };
}

function assertNormalize(input, expectedMapping, expectedChangedMappings, paddleButtons, message) {
  const actual = normalizePaddleMappings(input, paddleButtons);
  assert(actual.mapping === expectedMapping, `${message}: mapping\nexpected: ${expectedMapping}\nactual:   ${actual.mapping}`);
  assert(actual.changedMappings === expectedChangedMappings, `${message}: changed mappings\nexpected: ${expectedChangedMappings}\nactual:   ${actual.changedMappings}`);
}

assertSource(/#define DUALSENSE_EDGE_SDL2_MIN_BUTTONS 21\b/, 'native SDL2 minimum Edge raw button count must stay at 21');
assertSource(/#define DUALSENSE_EDGE_SDL3_COMPAT_MIN_BUTTONS 17\b/, 'sdl2-compat/SDL3 minimum Edge raw button count must stay at 17');
assertSource(/#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE1 16\b/, 'PADDLE1 controller button index must stay at 16');
assertSource(/#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE4 19\b/, 'PADDLE4 controller button index must stay at 19');
assertSource(/"paddle1",\s*"paddle2",\s*"paddle3",\s*"paddle4"/, 'paddle mapping key order changed');
assertSource(/20,\s*19,\s*18,\s*17/, 'native SDL2 paddle raw button order changed');
assertSource(/16,\s*15,\s*14,\s*13/, 'sdl2-compat/SDL3 paddle raw button order changed');
assertSource(/MISC_FLAG,\s*PADDLE1_FLAG,\s*PADDLE2_FLAG,\s*PADDLE3_FLAG,\s*PADDLE4_FLAG,\s*TOUCHPAD_FLAG/, 'button map no longer keeps MISC, Edge paddles, and TOUCHPAD adjacent');
assertSource(/static_assert\(PADDLE1_FLAG == 0x010000/, 'PADDLE1 high-word assertion missing');
assertSource(/static_assert\(PADDLE4_FLAG == 0x080000/, 'PADDLE4 high-word assertion missing');
assertSource(/SDL_CONTROLLER_BUTTON_MISC1 == DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE1 - 1/, 'MISC/PADDLE adjacency assertion missing');
assertSource(/SDL_CONTROLLER_BUTTON_TOUCHPAD == DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE4 \+ 1/, 'PADDLE/TOUCHPAD adjacency assertion missing');
assertSource(/SDL_GetHint\("SDL3_VERSION"\)/, 'sdl2-compat/SDL3 runtime detection missing');
assertSource(/buttonCount >= DUALSENSE_EDGE_SDL3_COMPAT_MIN_BUTTONS &&\s*buttonCount < DUALSENSE_EDGE_SDL2_MIN_BUTTONS/, 'sdl2-compat/SDL3 raw count fallback missing');
assertSource(/dualSenseEdgeMinimumButtonCount\(controller\)/, 'runtime Edge minimum raw button count selection missing');
assertSource(/mappingEntry\.startsWith\("type:"\)/, 'SDL2 type metadata must stay treated as non-binding metadata');
assertSource(/mappingEntry\.startsWith\("face:"\)/, 'SDL3 face metadata must stay treated as non-binding metadata');

function allPaddles(paddleButtons) {
  return paddleKeys.map((_, index) => paddleMappingEntry(index, paddleButtons)).join(',');
}

function verifyMappingSet(paddleButtons, label) {
  const canonicalPaddles = allPaddles(paddleButtons);
  const allChanged = canonicalPaddles;

  assertNormalize(
    '030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,type:ps5,platform:Mac OS X,',
    `030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,${canonicalPaddles},type:ps5,platform:Mac OS X,`,
    allChanged,
    paddleButtons,
    `${label}: missing Edge paddle bindings are inserted before SDL metadata`
  );

  assertNormalize(
    `030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,${canonicalPaddles},type:ps5,platform:Mac OS X,`,
    `030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,${canonicalPaddles},type:ps5,platform:Mac OS X,`,
    '',
    paddleButtons,
    `${label}: already canonical Edge paddle block is unchanged`
  );

  assertNormalize(
    `030000004c050000f20d000000000000,DualSense Edge,a:b${paddleButtons[0]},paddle1:b${paddleButtons[3]},paddle1:b${paddleButtons[0]},paddle2:b${paddleButtons[2]},paddle3:b${paddleButtons[1]},paddle4:b${paddleButtons[0]},type:ps5,`,
    `030000004c050000f20d000000000000,DualSense Edge,${canonicalPaddles},type:ps5,`,
    allChanged,
    paddleButtons,
    `${label}: stale paddle mappings and duplicate raw aliases are canonicalized`
  );

  assertNormalize(
    `030000004c050000f20d000000000000,DualSense Edge,x:b${paddleButtons[3]},y:b${paddleButtons[2]},a:b${paddleButtons[0]},b:b${paddleButtons[1]},crc:1234,face:ABXY,hint:foo,sdk>=:0,sdk<=:999,type:ps5,`,
    `030000004c050000f20d000000000000,DualSense Edge,${canonicalPaddles},crc:1234,face:ABXY,hint:foo,sdk>=:0,sdk<=:999,type:ps5,`,
    allChanged,
    paddleButtons,
    `${label}: normal buttons bound to Edge-only raw buttons are removed before metadata`
  );
}

verifyMappingSet(sdl2PaddleButtons, 'native SDL2');
verifyMappingSet(sdl3CompatPaddleButtons, 'sdl2-compat/SDL3');

console.log('DualSense Edge mapping verification passed.');
