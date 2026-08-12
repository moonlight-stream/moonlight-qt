#include "av1obu.h"

#include <string.h>

// OBU types we care about (AV1 spec 6.2.2)
#define OBU_TEMPORAL_DELIMITER 2
#define OBU_FRAME_HEADER       3
#define OBU_TILE_GROUP         4
#define OBU_METADATA           5
#define OBU_FRAME              6

// A temporal unit from a streaming host is a handful of OBUs. Anything longer is
// not the layout we rewrite, so we can bail out rather than grow this.
#define MAX_OBUS 16

// Metadata OBUs that need hoisting are HDR10+ ITU-T T.35, mastering display, and
// content light level payloads - all well under 100 bytes. They have to be stashed
// because they move backwards past the frame header while the frame header's own
// payload moves forwards past them.
#define MAX_HOISTED_METADATA 512

struct Obu {
    int type;
    int headerOffset;   // start of the OBU header
    int headerLength;   // header + leb128 size field
    int sizeBytes;      // length of the leb128 size field as actually encoded
    int payloadOffset;
    int payloadLength;
};

static int leb128Size(uint64_t value)
{
    int size = 0;
    do {
        size++;
        value >>= 7;
    } while (value != 0);
    return size;
}

static int writeLeb128(uint8_t* out, uint64_t value)
{
    int size = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        out[size++] = byte;
    } while (value != 0);
    return size;
}

// Parses the temporal unit into obus[]. Returns the OBU count, or -1 if the data
// is malformed or uses a form we can't safely rewrite (missing size fields).
static int parseObus(const uint8_t* data, int length, Obu* obus)
{
    int count = 0;
    int pos = 0;

    while (pos < length) {
        if (count == MAX_OBUS) {
            return -1;
        }

        uint8_t header = data[pos];
        if (header & 0x80) {
            // obu_forbidden_bit must be zero
            return -1;
        }

        int headerLength = 1;
        if (header & 0x04) {
            // obu_extension_flag: one more byte of temporal_id/spatial_id
            headerLength++;
        }
        if (!(header & 0x02)) {
            // No obu_size field, so the OBU runs to the end of the buffer and we
            // can't rewrite anything around it.
            return -1;
        }
        if (pos + headerLength >= length) {
            return -1;
        }

        // leb128 obu_size
        uint64_t size = 0;
        int sizeBytes = 0;
        for (;;) {
            if (sizeBytes == 8 || pos + headerLength + sizeBytes >= length) {
                return -1;
            }
            uint8_t byte = data[pos + headerLength + sizeBytes];
            size |= (uint64_t)(byte & 0x7F) << (7 * sizeBytes);
            sizeBytes++;
            if (!(byte & 0x80)) {
                break;
            }
        }
        headerLength += sizeBytes;

        if (size > (uint64_t)(length - (pos + headerLength))) {
            return -1;
        }

        obus[count].type = (header >> 3) & 0x0F;
        obus[count].headerOffset = pos;
        obus[count].headerLength = headerLength;
        obus[count].sizeBytes = sizeBytes;
        obus[count].payloadOffset = pos + headerLength;
        obus[count].payloadLength = (int)size;
        count++;

        pos += headerLength + (int)size;
    }

    return count;
}

int repackAv1TemporalUnit(uint8_t* data, int length)
{
    Obu obus[MAX_OBUS];
    int count = parseObus(data, length, obus);
    if (count <= 0) {
        return length;
    }

    // Find the split frame we're here to merge. We only handle the exact layout
    // NVENC produces: a single frame header, optional metadata, then a single tile
    // group that ends the temporal unit.
    int frameHeaderIdx = -1;
    int tileGroupIdx = -1;
    for (int i = 0; i < count; i++) {
        switch (obus[i].type) {
        case OBU_FRAME:
            // Already in the merged form the decoder wants
            return length;
        case OBU_FRAME_HEADER:
            if (frameHeaderIdx >= 0) {
                return length;
            }
            frameHeaderIdx = i;
            break;
        case OBU_TILE_GROUP:
            if (tileGroupIdx >= 0) {
                return length;
            }
            tileGroupIdx = i;
            break;
        default:
            break;
        }
    }

    if (frameHeaderIdx < 0 || tileGroupIdx != count - 1 || tileGroupIdx < frameHeaderIdx) {
        return length;
    }

    // Everything between the frame header and the tile group gets hoisted ahead of
    // the merged frame. Only metadata belongs there.
    int metadataLength = 0;
    for (int i = frameHeaderIdx + 1; i < tileGroupIdx; i++) {
        if (obus[i].type != OBU_METADATA) {
            return length;
        }
        metadataLength += obus[i].headerLength + obus[i].payloadLength;
    }
    if (metadataLength > MAX_HOISTED_METADATA) {
        return length;
    }

    const Obu& frameHeader = obus[frameHeaderIdx];
    const Obu& tileGroup = obus[tileGroupIdx];

    // The merged OBU_FRAME reuses the frame header's OBU header, so the tile group's
    // header has to agree with it. If they disagree we'd silently move the tile group
    // into a different temporal/spatial layer, so leave the whole thing alone.
    uint8_t fhHeader = data[frameHeader.headerOffset];
    uint8_t tgHeader = data[tileGroup.headerOffset];
    if ((fhHeader & 0x04) != (tgHeader & 0x04)) {
        return length;
    }
    if ((fhHeader & 0x04) &&
        data[frameHeader.headerOffset + 1] != data[tileGroup.headerOffset + 1]) {
        return length;
    }

    // An OBU_FRAME_HEADER payload ends with trailing_bits(obu_size * 8 - payloadBits):
    // a one bit, then zeroes all the way to the end of obu_size. An encoder that
    // over-declares obu_size therefore leaves whole zero bytes at the end, which is
    // legal.
    //
    // Inside an OBU_FRAME the frame header is followed by byte_alignment() instead,
    // and that only pads to the next byte boundary - it can never span a whole byte.
    // So two things have to happen here: the stop bit (the lowest set bit of the last
    // non-zero byte) goes away, and any whole zero bytes after it get dropped.
    // Keeping them would shift them into tile_group_obu()'s payload and corrupt the
    // frame.
    int frameHeaderLength = frameHeader.payloadLength;
    while (frameHeaderLength > 0 && data[frameHeader.payloadOffset + frameHeaderLength - 1] == 0) {
        frameHeaderLength--;
    }
    if (frameHeaderLength == 0) {
        // Malformed: trailing_bits() always writes a stop bit somewhere.
        return length;
    }
    uint8_t origLastByte = data[frameHeader.payloadOffset + frameHeaderLength - 1];
    uint8_t lastByte = origLastByte & (origLastByte - 1);
    // Exactly 0x80 means the stop bit was the first bit of the byte, i.e.
    // frame_header_obu() ended on the previous byte boundary and this byte carries no
    // payload bits at all. byte_alignment() contributes nothing there, so the byte has
    // to disappear rather than become a zero - same reason as the zero bytes above.
    // Any other single-bit value (0x20, 0x40, ...) still holds real payload bits ahead
    // of the stop bit, so it stays as 0x00 and byte_alignment() pads out the rest.
    bool dropLastByte = (origLastByte == 0x80);
    if (dropLastByte) {
        frameHeaderLength--;
        if (frameHeaderLength == 0) {
            return length;
        }
    }

    uint64_t mergedPayloadLength = (uint64_t)frameHeaderLength + tileGroup.payloadLength;
    // Recover the OBU header bytes without its size field. Use the size field's actual
    // encoded width, not leb128Size(): AV1 permits non-minimal leb128, so an encoder may
    // have written e.g. 81 00 for a payload of 1.
    int mergedHeaderLength = (frameHeader.headerLength - frameHeader.sizeBytes) +
                             leb128Size(mergedPayloadLength);

    int preambleLength = frameHeader.headerOffset;

    uint8_t metadata[MAX_HOISTED_METADATA];
    if (metadataLength > 0) {
        memcpy(metadata, data + obus[frameHeaderIdx + 1].headerOffset, metadataLength);
    }

    // Lay out [preamble][metadata][OBU_FRAME header][frame header][tile group].
    // The preamble doesn't move. Everything else is written back to front so no
    // move clobbers bytes a later move still needs to read.
    int mergedHeaderOffset = preambleLength + metadataLength;
    int frameHeaderDest = mergedHeaderOffset + mergedHeaderLength;
    int tileGroupDest = frameHeaderDest + frameHeaderLength;

    memmove(data + tileGroupDest, data + tileGroup.payloadOffset, tileGroup.payloadLength);
    memmove(data + frameHeaderDest, data + frameHeader.payloadOffset, frameHeaderLength);
    if (!dropLastByte) {
        data[frameHeaderDest + frameHeaderLength - 1] = lastByte;
    }

    // Reuse the frame header's OBU header so temporal_id/spatial_id survive, but
    // retype it to OBU_FRAME.
    data[mergedHeaderOffset] = (data[frameHeader.headerOffset] & ~0x78) | (OBU_FRAME << 3);
    int written = 1;
    if (data[mergedHeaderOffset] & 0x04) {
        data[mergedHeaderOffset + 1] = data[frameHeader.headerOffset + 1];
        written++;
    }
    written += writeLeb128(data + mergedHeaderOffset + written, mergedPayloadLength);

    if (metadataLength > 0) {
        memcpy(data + preambleLength, metadata, metadataLength);
    }

    return tileGroupDest + tileGroup.payloadLength;
}
