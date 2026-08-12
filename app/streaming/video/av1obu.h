#pragma once

#include <stdint.h>

// Repacks an AV1 temporal unit in place, merging an OBU_FRAME_HEADER and its
// OBU_TILE_GROUP into a single OBU_FRAME and hoisting any OBU_METADATA that sat
// between them ahead of the merged frame.
//
// macOS VideoToolbox rejects the split frame header + tile group layout that
// NVENC emits for AV1 HDR, failing every frame with kVTVideoDecoderMalfunctionErr
// (-12911). The same encoder uses the merged OBU_FRAME form for SDR, which decodes
// fine, so rewriting to that form is enough to make HDR decode too.
//
// Hoisting the metadata is a free side effect of the merge, and it happens to be
// what the HDR10+ AV1 Metadata Handling Specification requires: the metadata OBU
// must precede the frame header.
//
// Returns the new length, which is never larger than the original. If the temporal
// unit doesn't match the layout we rewrite, the buffer is left untouched and the
// original length is returned.
int repackAv1TemporalUnit(uint8_t* data, int length);
