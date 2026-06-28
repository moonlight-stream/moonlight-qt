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
const paddleButtons = [20, 19, 18, 17];

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

function paddleMappingEntry(index) {
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

function paddleRawButtonMappingIndex(mappingEntry) {
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

function addPaddleMappingChange(changedMappings, mappingIndex) {
  changedMappings.push(paddleMappingEntry(mappingIndex));
}

function normalizePaddleMappings(mapping) {
  const entries = mapping.split(',');
  const updatedEntries = [];
  const foundMappings = new Array(paddleKeys.length).fill(false);
  const changedMappingEntries = new Array(paddleKeys.length).fill(false);
  let hasChangedMappingEntries = false;

  for (const entry of entries) {
    let mappingIndex = paddleMappingIndex(entry);

    if (mappingIndex < 0) {
      mappingIndex = paddleRawButtonMappingIndex(entry);
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
    if (entry !== paddleMappingEntry(mappingIndex)) {
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
    updatedEntries.splice(insertionIndex++, 0, paddleMappingEntry(i));
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
      addPaddleMappingChange(changedMappings, i);
    }
  }

  return {
    mapping: updatedMapping,
    changedMappings: changedMappings.join(','),
  };
}

function assertNormalize(input, expectedMapping, expectedChangedMappings, message) {
  const actual = normalizePaddleMappings(input);
  assert(actual.mapping === expectedMapping, `${message}: mapping\nexpected: ${expectedMapping}\nactual:   ${actual.mapping}`);
  assert(actual.changedMappings === expectedChangedMappings, `${message}: changed mappings\nexpected: ${expectedChangedMappings}\nactual:   ${actual.changedMappings}`);
}

assertSource(/#define DUALSENSE_EDGE_MIN_BUTTONS 21\b/, 'minimum Edge raw button count must stay at 21');
assertSource(/#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE1 16\b/, 'PADDLE1 controller button index must stay at 16');
assertSource(/#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE4 19\b/, 'PADDLE4 controller button index must stay at 19');
assertSource(/"paddle1",\s*"paddle2",\s*"paddle3",\s*"paddle4"/, 'paddle mapping key order changed');
assertSource(/20,\s*19,\s*18,\s*17/, 'paddle raw button order changed');
assertSource(/MISC_FLAG,\s*PADDLE1_FLAG,\s*PADDLE2_FLAG,\s*PADDLE3_FLAG,\s*PADDLE4_FLAG,\s*TOUCHPAD_FLAG/, 'button map no longer keeps MISC, Edge paddles, and TOUCHPAD adjacent');
assertSource(/static_assert\(PADDLE1_FLAG == 0x010000/, 'PADDLE1 high-word assertion missing');
assertSource(/static_assert\(PADDLE4_FLAG == 0x080000/, 'PADDLE4 high-word assertion missing');
assertSource(/SDL_CONTROLLER_BUTTON_MISC1 == DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE1 - 1/, 'MISC/PADDLE adjacency assertion missing');
assertSource(/SDL_CONTROLLER_BUTTON_TOUCHPAD == DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE4 \+ 1/, 'PADDLE/TOUCHPAD adjacency assertion missing');
assertSource(/mappingEntry\.startsWith\("type:"\)/, 'SDL2 type metadata must stay treated as non-binding metadata');
assertSource(/mappingEntry\.startsWith\("face:"\)/, 'SDL3 face metadata must stay treated as non-binding metadata');

const allPaddles = 'paddle1:b20,paddle2:b19,paddle3:b18,paddle4:b17';
const allChanged = allPaddles;

assertNormalize(
  '030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,type:ps5,platform:Mac OS X,',
  `030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,${allPaddles},type:ps5,platform:Mac OS X,`,
  allChanged,
  'missing Edge paddle bindings are inserted before SDL metadata'
);

assertNormalize(
  `030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,${allPaddles},type:ps5,platform:Mac OS X,`,
  `030000004c050000f20d000000000000,DualSense Edge,a:b0,b:b1,${allPaddles},type:ps5,platform:Mac OS X,`,
  '',
  'already canonical Edge paddle block is unchanged'
);

assertNormalize(
  '030000004c050000f20d000000000000,DualSense Edge,a:b20,paddle1:b17,paddle1:b20,paddle2:b18,paddle3:b19,paddle4:b20,type:ps5,',
  `030000004c050000f20d000000000000,DualSense Edge,${allPaddles},type:ps5,`,
  allChanged,
  'stale paddle mappings and duplicate raw aliases are canonicalized'
);

assertNormalize(
  '030000004c050000f20d000000000000,DualSense Edge,x:b17,y:b18,a:b20,b:b19,crc:1234,face:ABXY,hint:foo,sdk>=:0,sdk<=:999,type:ps5,',
  `030000004c050000f20d000000000000,DualSense Edge,${allPaddles},crc:1234,face:ABXY,hint:foo,sdk>=:0,sdk<=:999,type:ps5,`,
  allChanged,
  'normal buttons bound to Edge-only raw buttons are removed before metadata'
);

console.log('DualSense Edge mapping verification passed.');
