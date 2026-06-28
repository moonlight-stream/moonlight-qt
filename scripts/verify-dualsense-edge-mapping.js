#!/usr/bin/env node
/*
 * Verifies the DualSense Edge SDL controller mapping assumptions used by
 * app/streaming/input/gamepad.cpp without requiring physical hardware.
 */

const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(__dirname, '..');
const gamepadPath = path.join(repoRoot, 'app', 'streaming', 'input', 'gamepad.cpp');
const limelightPath = path.join(repoRoot, 'moonlight-common-c', 'moonlight-common-c', 'src', 'Limelight.h');
const inputHeaderPath = path.join(repoRoot, 'moonlight-common-c', 'moonlight-common-c', 'src', 'Input.h');
const inputStreamPath = path.join(repoRoot, 'moonlight-common-c', 'moonlight-common-c', 'src', 'InputStream.c');
const source = fs.readFileSync(gamepadPath, 'utf8');
const limelightSource = fs.readFileSync(limelightPath, 'utf8');
const inputHeaderSource = fs.readFileSync(inputHeaderPath, 'utf8');
const inputStreamSource = fs.readFileSync(inputStreamPath, 'utf8');

const paddleKeys = ['paddle1', 'paddle2', 'paddle3', 'paddle4'];
const sdl2PaddleButtons = [20, 19, 18, 17];
const sdl3CompatPaddleButtons = [16, 15, 14, 13];
const sdl2MinButtons = 21;
const sdl3CompatMinButtons = 17;

function fail(message) {
  console.error(`DualSense Edge mapping verification failed: ${message}`);
  process.exit(1);
}

function assert(condition, message) {
  if (!condition) {
    fail(message);
  }
}

function assertInSource(sourceText, pattern, message) {
  assert(pattern.test(sourceText), message);
}

function assertSource(pattern, message) {
  assertInSource(source, pattern, message);
}

function usesSdl3CompatMappings(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat) {
  if (hasSdl3VersionHint) {
    return true;
  }

  return runtimeLooksLikeSdl2Compat;
}

function minimumButtonCount(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat) {
  return usesSdl3CompatMappings(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat) ?
    sdl3CompatMinButtons :
    sdl2MinButtons;
}

function exposesPaddleButtons(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat, buttonCount) {
  return buttonCount >= minimumButtonCount(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat);
}

function sunshineButtonFlags2(buttonFlags) {
  return (buttonFlags >>> 16) & 0xffff;
}

function assertRuntimeSelection(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat, buttonCount, expectedUsesSdl3Compat, expectedExposesPaddles, message) {
  const actualUsesSdl3Compat = usesSdl3CompatMappings(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat);
  const actualExposesPaddles = exposesPaddleButtons(hasSdl3VersionHint, runtimeLooksLikeSdl2Compat, buttonCount);

  assert(
    actualUsesSdl3Compat === expectedUsesSdl3Compat,
    `${message}: raw mapping selection expected ${expectedUsesSdl3Compat ? 'SDL3/sdl2-compat' : 'SDL2'} but got ${actualUsesSdl3Compat ? 'SDL3/sdl2-compat' : 'SDL2'}`
  );
  assert(
    actualExposesPaddles === expectedExposesPaddles,
    `${message}: paddle exposure expected ${expectedExposesPaddles} but got ${actualExposesPaddles}`
  );
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

function escapeRegex(text) {
  return text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function bindingSummary(paddleButtons) {
  return paddleKeys.map((_, index) => `${paddleKeys[index]}=b${paddleButtons[index]}`).join(',');
}

function logMappingSummary(paddleButtons) {
  return paddleKeys.map((_, index) => paddleMappingEntry(index, paddleButtons)).join(',');
}

const moonlightEvidenceVariants = {
  sdl2: {
    label: 'native SDL2',
    minButtons: sdl2MinButtons,
    rawMappingName: 'SDL2',
    runtimeVersionPattern: /SDL \d+\.\d+\.\d+\)/,
    rawMappingSignalPattern: /^native SDL2 runtime$/,
    bindings: bindingSummary(sdl2PaddleButtons),
    logMappings: logMappingSummary(sdl2PaddleButtons),
  },
  sdl3Compat: {
    label: 'sdl2-compat/SDL3',
    minButtons: sdl3CompatMinButtons,
    rawMappingName: 'SDL3/sdl2-compat',
    runtimeVersionPattern: /^(?:SDL \d+\.\d+\.\d+, SDL3 \d+\.\d+\.\d+\)|SDL 2\.\d+\.(?:[5-9]\d|[1-9]\d{2,})\))$/,
    rawMappingSignalPattern: /^(SDL3_VERSION|sdl2-compat runtime version)$/,
    bindings: bindingSummary(sdl3CompatPaddleButtons),
    logMappings: logMappingSummary(sdl3CompatPaddleButtons),
  },
};

function hasDetectionEvidence(log, variant) {
  const detectionPattern = /DualSense Edge detected with (\d+) SDL joystick buttons \(need (\d+) for (SDL2|SDL3\/sdl2-compat) Edge raw mapping\) \(VID\/PID: 0x054c\/0x0df2\) \((SDL \d+\.\d+\.\d+(?:, SDL3 \d+\.\d+\.\d+)?)\) \(raw mapping signal: ([^)]+)\)/g;
  let match;

  while ((match = detectionPattern.exec(log)) !== null) {
    const buttonCount = Number(match[1]);
    const requiredButtons = Number(match[2]);
    const rawMappingName = match[3];
    const runtimeVersion = `${match[4]})`;
    const rawMappingSignal = match[5];
    if (buttonCount >= variant.minButtons &&
        requiredButtons === variant.minButtons &&
        rawMappingName === variant.rawMappingName &&
        variant.runtimeVersionPattern.test(runtimeVersion) &&
        variant.rawMappingSignalPattern.test(rawMappingSignal)) {
      return true;
    }
  }

  return false;
}

function hasMappingEvidence(log) {
  return log.includes('DualSense Edge paddle mappings already present') ||
         /Applied DualSense Edge paddle mappings \((added|updated)\): /.test(log);
}

function hasArrivalEvidence(log, variant) {
  const arrivalPattern = /DualSense Edge arrival support: buttons=(\d+) sdlType=\d+ arrivalType=0x([0-9a-fA-F]{2}) supportedButtonFlags=0x([0-9a-fA-F]{8}) paddle\/Fn=0x([0-9a-fA-F]{8}) capabilities=0x([0-9a-fA-F]{8}) bindings=([^\r\n]+)/g;
  let match;

  while ((match = arrivalPattern.exec(log)) !== null) {
    const buttonCount = Number(match[1]);
    const arrivalType = Number.parseInt(match[2], 16);
    const paddleFn = Number.parseInt(match[4], 16);
    const bindings = match[6];
    if (buttonCount >= variant.minButtons &&
        arrivalType === 0x02 &&
        paddleFn === 0x000f0000 &&
        bindings === variant.bindings) {
      return true;
    }
  }

  return false;
}

function hasPerButtonEvidence(log) {
  const expectedMasks = {
    PADDLE1: '00010000',
    PADDLE2: '00020000',
    PADDLE3: '00040000',
    PADDLE4: '00080000',
  };
  const singleMasks = new Set(['00010000', '00020000', '00040000', '00080000']);
  const eventPattern = /DualSense Edge (PADDLE[1-4]) (pressed|released) \(paddle\/Fn flags: 0x([0-9a-fA-F]{8})\)/g;
  const seen = {};
  let match;

  for (const name of Object.keys(expectedMasks)) {
    seen[`${name}:pressed`] = false;
    seen[`${name}:released`] = false;
  }

  while ((match = eventPattern.exec(log)) !== null) {
    const name = match[1];
    const action = match[2];
    const mask = match[3].toLowerCase();
    if (action === 'pressed') {
      if (mask !== expectedMasks[name]) {
        return false;
      }
      seen[`${name}:pressed`] = true;
    }
    else {
      if (mask !== '00000000') {
        return false;
      }
      seen[`${name}:released`] = true;
    }

    if (mask !== '00000000' && !singleMasks.has(mask)) {
      return false;
    }
  }

  return Object.values(seen).every(Boolean);
}

function hasNoMoonlightStopEvidence(log) {
  return !/DualSense Edge paddle mappings are present, but SDL did not expose/.test(log) &&
         !/DualSense Edge paddle mapping update did not expose/.test(log) &&
         !/DualSense Edge paddle mapping add did not expose/.test(log) &&
         !/Ignoring DualSense Edge PADDLE[1-4] event without verified paddle bindings/.test(log) &&
         !/Ignoring DualSense Edge controller button .* as stale Edge raw alias/.test(log) &&
         !/DualSense Edge controller button .* not advertising it as a normal button/.test(log);
}

function hasCompleteMoonlightLogEvidence(log, variant) {
  return hasDetectionEvidence(log, variant) &&
         hasMappingEvidence(log, variant) &&
         hasArrivalEvidence(log, variant) &&
         hasPerButtonEvidence(log) &&
         hasNoMoonlightStopEvidence(log);
}

function makeCompleteMoonlightLog(variant) {
  const runtimeSummary = variant.rawMappingName === 'SDL2' ? 'SDL 2.30.9' : 'SDL 2.32.70, SDL3 3.4.11';
  const rawMappingSignal = variant.rawMappingName === 'SDL2' ? 'native SDL2 runtime' : 'SDL3_VERSION';

  return [
    `DualSense Edge detected with ${variant.minButtons} SDL joystick buttons (need ${variant.minButtons} for ${variant.rawMappingName} Edge raw mapping) (VID/PID: 0x054c/0x0df2) (${runtimeSummary}) (raw mapping signal: ${rawMappingSignal})`,
    `Applied DualSense Edge paddle mappings (updated): ${variant.logMappings}`,
    `DualSense Edge arrival support: buttons=${variant.minButtons} sdlType=4 arrivalType=0x02 supportedButtonFlags=0x000f0000 paddle/Fn=0x000f0000 capabilities=0x00000000 bindings=${variant.bindings}`,
    'DualSense Edge PADDLE1 pressed (paddle/Fn flags: 0x00010000)',
    'DualSense Edge PADDLE1 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00020000)',
    'DualSense Edge PADDLE2 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE3 pressed (paddle/Fn flags: 0x00040000)',
    'DualSense Edge PADDLE3 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE4 pressed (paddle/Fn flags: 0x00080000)',
    'DualSense Edge PADDLE4 released (paddle/Fn flags: 0x00000000)',
  ].join('\n');
}

function assertMoonlightLogEvidence(log, variant, expected, message) {
  const actual = hasCompleteMoonlightLogEvidence(log, variant);
  assert(actual === expected, `${message}: expected ${expected ? 'complete' : 'incomplete'} Moonlight log evidence`);
}

function withoutLogLine(log, line) {
  return log.replace(new RegExp(`^${escapeRegex(line)}\\n?`, 'm'), '');
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
assertSource(/version\.major == 2 && version\.patch >= 50/, 'sdl2-compat/SDL3 runtime-version fallback missing');
assertSource(/SDL_GameControllerHasButton\(controller, button\)/, 'Edge paddle support must probe the SDL controller-button layer');
assertSource(/SDL_GameControllerGetBindForButton\(controller, button\)/, 'Edge paddle support must verify SDL controller-button bindings');
assertSource(/dualSenseEdgeMinimumButtonCount\(controller\)/, 'runtime Edge minimum raw button count selection missing');
assertSource(/raw mapping signal: %s/, 'Edge detection log must include raw mapping selection signal');
assertSource(/SDL3.*QString::fromUtf8\(sdl3Version\)/s, 'Edge detection log must include underlying SDL3 runtime version when available');
assertSource(/mappingEntry\.startsWith\("type:"\)/, 'SDL2 type metadata must stay treated as non-binding metadata');
assertSource(/mappingEntry\.startsWith\("face:"\)/, 'SDL3 face metadata must stay treated as non-binding metadata');
assertSource(/isDualSenseEdge && type != LI_CTYPE_PS[\s\S]*type = LI_CTYPE_PS;/, 'DualSense Edge must advertise PlayStation arrival type by VID/PID even if SDL type is generic');
assertSource(/supportedButtonFlags &= ~DUALSENSE_EDGE_PADDLE_FLAGS[\s\S]*supportedButtonFlags \|= DUALSENSE_EDGE_PADDLE_FLAGS/, 'DualSense Edge paddle/Fn support must only be advertised after verified controller-button bindings');
assertSource(/DualSense Edge arrival support: buttons=%d sdlType=%d arrivalType=0x%02x supportedButtonFlags=0x%08x paddle\/Fn=0x%08x capabilities=0x%08x bindings=%s/, 'DualSense Edge arrival evidence log format changed');
assertSource(/DualSense Edge %s %s \(paddle\/Fn flags: 0x%08x\)/, 'DualSense Edge per-button evidence log format changed');
assertSource(/without verified paddle bindings/, 'DualSense Edge unverified paddle-event diagnostic missing');
assertSource(/as stale Edge raw alias/, 'DualSense Edge stale raw-alias diagnostic missing');
assertSource(/not advertising it as a normal button/, 'DualSense Edge capability alias diagnostic missing');
assertInSource(limelightSource, /#define PADDLE1_FLAG\s+0x010000\b/, 'Moonlight common PADDLE1 flag must stay in the Sunshine high word');
assertInSource(limelightSource, /#define PADDLE2_FLAG\s+0x020000\b/, 'Moonlight common PADDLE2 flag must stay in the Sunshine high word');
assertInSource(limelightSource, /#define PADDLE3_FLAG\s+0x040000\b/, 'Moonlight common PADDLE3 flag must stay in the Sunshine high word');
assertInSource(limelightSource, /#define PADDLE4_FLAG\s+0x080000\b/, 'Moonlight common PADDLE4 flag must stay in the Sunshine high word');
assertInSource(inputHeaderSource, /short buttonFlags2; \/\/ Sunshine protocol extension \(always 0 for GFE\)/, 'multi-controller packet must retain the Sunshine buttonFlags2 extension');
assertInSource(inputStreamSource, /buttonFlags2 != \(IS_SUNSHINE\(\) \? LE16\(\(short\)\(buttonFlags >> 16\)\) : 0\)/, 'batched controller comparison must include Sunshine high-word buttonFlags2');
assertInSource(inputStreamSource, /buttonFlags2 = IS_SUNSHINE\(\) \? LE16\(\(short\)\(buttonFlags >> 16\)\) : 0;/, 'controller packets must carry high-word Edge buttons in Sunshine buttonFlags2');
assertInSource(inputStreamSource, /controllerArrival\.supportedButtonFlags = LE32\(supportedButtonFlags\);/, 'arrival packets must advertise the full 32-bit supported button mask');

assertRuntimeSelection(false, false, 21, false, true, 'native SDL2 exposes the 21-button Edge shape');
assertRuntimeSelection(false, false, 20, false, false, 'unknown 20-button Edge shape fails closed without an SDL3 hint');
assertRuntimeSelection(false, false, 18, false, false, 'unknown 18-button Edge shape fails closed without an SDL3 hint');
assertRuntimeSelection(false, false, 17, false, false, 'ambiguous 17-button Edge shape fails closed without an SDL3/sdl2-compat runtime signal');
assertRuntimeSelection(false, true, 17, true, true, 'sdl2-compat runtime fallback accepts current 17-button Edge shape');
assertRuntimeSelection(false, true, 16, true, false, 'sdl2-compat runtime fallback still requires enough raw buttons');
assertRuntimeSelection(true, false, 21, true, true, 'SDL3 hint selects sdl2-compat mapping even if future SDL3 exposes more raw buttons');
assertRuntimeSelection(true, false, 17, true, true, 'SDL3 hint accepts current sdl2-compat Edge shape');
assertRuntimeSelection(true, false, 16, true, false, 'SDL3 hint still requires enough raw buttons');

[
  [0x010000, 0x0001, 'PADDLE1'],
  [0x020000, 0x0002, 'PADDLE2'],
  [0x040000, 0x0004, 'PADDLE3'],
  [0x080000, 0x0008, 'PADDLE4'],
].forEach(([buttonFlag, expectedButtonFlags2, name]) => {
  assert(
    sunshineButtonFlags2(buttonFlag) === expectedButtonFlags2,
    `${name} must serialize to Sunshine buttonFlags2 bit 0x${expectedButtonFlags2.toString(16).padStart(4, '0')}`
  );
  assert(
    (buttonFlag & 0xffff) === 0,
    `${name} must not collide with the legacy 16-bit buttonFlags field`
  );
});
assert(sunshineButtonFlags2(0x0f0000) === 0x000f, 'all Edge paddle/Fn buttons must serialize as buttonFlags2 mask 0x000f');
assert(sunshineButtonFlags2(0x2f0000) === 0x002f, 'Edge paddle/Fn, touchpad, and misc buttons must all survive in Sunshine buttonFlags2');

const completeSdl2MoonlightLog = makeCompleteMoonlightLog(moonlightEvidenceVariants.sdl2);
const completeSdl3MoonlightLog = makeCompleteMoonlightLog(moonlightEvidenceVariants.sdl3Compat);
assertMoonlightLogEvidence(completeSdl2MoonlightLog, moonlightEvidenceVariants.sdl2, true, 'native SDL2 same-log evidence passes');
assertMoonlightLogEvidence(completeSdl3MoonlightLog, moonlightEvidenceVariants.sdl3Compat, true, 'sdl2-compat/SDL3 same-log evidence passes');
assertMoonlightLogEvidence(
  completeSdl3MoonlightLog.replace(
    '(SDL 2.32.70, SDL3 3.4.11) (raw mapping signal: SDL3_VERSION)',
    '(SDL 2.32.70) (raw mapping signal: sdl2-compat runtime version)'
  ),
  moonlightEvidenceVariants.sdl3Compat,
  true,
  'sdl2-compat runtime-version signal satisfies SDL3-compatible log evidence without SDL3_VERSION'
);
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.replace(
    'DualSense Edge detected with 21 SDL joystick buttons',
    'DualSense Edge detected with 20 SDL joystick buttons'
  ),
  moonlightEvidenceVariants.sdl2,
  false,
  'native SDL2 raw-button undercount fails Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl3MoonlightLog.replace(
    'DualSense Edge detected with 17 SDL joystick buttons',
    'DualSense Edge detected with 16 SDL joystick buttons'
  ),
  moonlightEvidenceVariants.sdl3Compat,
  false,
  'sdl2-compat/SDL3 raw-button undercount fails Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl3MoonlightLog.replace(
    '(SDL 2.32.70, SDL3 3.4.11) (raw mapping signal: SDL3_VERSION)',
    '(SDL 2.32.10) (raw mapping signal: sdl2-compat runtime version)'
  ),
  moonlightEvidenceVariants.sdl3Compat,
  false,
  'ambiguous low-patch sdl2-compat claim fails Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.replace('VID/PID: 0x054c/0x0df2', 'VID/PID: 0x054c/0x0ce6'),
  moonlightEvidenceVariants.sdl2,
  false,
  'wrong Edge VID/PID fails Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.replace(' (raw mapping signal: native SDL2 runtime)', ''),
  moonlightEvidenceVariants.sdl2,
  false,
  'missing raw mapping signal fails Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl3MoonlightLog.replace('raw mapping signal: SDL3_VERSION', 'raw mapping signal: native SDL2 runtime'),
  moonlightEvidenceVariants.sdl3Compat,
  false,
  'wrong raw mapping signal fails sdl2-compat/SDL3 log evidence'
);
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.replace(
    `Applied DualSense Edge paddle mappings (updated): ${moonlightEvidenceVariants.sdl2.logMappings}`,
    'DualSense Edge paddle mappings already present'
  ),
  moonlightEvidenceVariants.sdl2,
  true,
  'already-present mapping line satisfies Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.replace(
    `Applied DualSense Edge paddle mappings (updated): ${moonlightEvidenceVariants.sdl2.logMappings}`,
    'Applied DualSense Edge paddle mappings (updated): paddle2:b19,paddle4:b17'
  ),
  moonlightEvidenceVariants.sdl2,
  true,
  'partial mapping-change line can satisfy Moonlight log evidence when arrival bindings are complete'
);
assertMoonlightLogEvidence(
  withoutLogLine(completeSdl2MoonlightLog, 'DualSense Edge PADDLE4 released (paddle/Fn flags: 0x00000000)'),
  moonlightEvidenceVariants.sdl2,
  false,
  'missing release-to-neutral line fails Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.replace(
    `bindings=${moonlightEvidenceVariants.sdl2.bindings}`,
    `bindings=${moonlightEvidenceVariants.sdl3Compat.bindings}`
  ),
  moonlightEvidenceVariants.sdl2,
  false,
  'mismatched SDL binding variant fails Moonlight log evidence'
);
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.replace(
    'DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00020000)',
    'DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00030000)'
  ),
  moonlightEvidenceVariants.sdl2,
  false,
  'combined one-at-a-time paddle/Fn mask fails Moonlight log evidence'
);
[
  'DualSense Edge paddle mappings are present, but SDL did not expose the expected paddle controller bindings (actual: paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17)',
  'DualSense Edge paddle mapping update did not expose the expected paddle controller bindings (actual: paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17)',
  'DualSense Edge paddle mapping add did not expose the expected paddle controller bindings (actual: paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17)',
  'Ignoring DualSense Edge PADDLE1 event without verified paddle bindings',
  'Ignoring DualSense Edge controller button a bound to raw paddle1:b20 as stale Edge raw alias',
  'DualSense Edge controller button a is bound to raw paddle1:b20; not advertising it as a normal button',
].forEach((stopLine) => {
  assertMoonlightLogEvidence(
    `${completeSdl2MoonlightLog}\n${stopLine}`,
    moonlightEvidenceVariants.sdl2,
    false,
    `${stopLine} fails Moonlight log evidence`
  );
});
assertMoonlightLogEvidence(
  completeSdl2MoonlightLog.split('\n').slice(0, 2).join('\n'),
  moonlightEvidenceVariants.sdl2,
  false,
  'partial copied Moonlight log cannot satisfy complete evidence'
);

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
