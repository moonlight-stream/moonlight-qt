# AV1 + HDR 在 VideoToolbox 上黑屏：起因与移植到 iOS 的做法

> 这份文档是给 **moonlight-ios**（以及任何直接用 VideoToolbox 解码的 Apple 平台客户端）看的。
> moonlight-qt 侧的修复见 [#168](https://github.com/qiin2333/moonlight-qt/pull/168)，
> 实现文件是 `app/streaming/video/av1obu.{h,cpp}`。

## TL;DR

NVENC 在 **AV1 HDR** 路径下会把一帧拆成 `OBU_FRAME_HEADER(3)` + `OBU_TILE_GROUP(4)` 两个 OBU，
而 SDR 路径下发的是合并的 `OBU_FRAME(6)`。**VideoToolbox 吞不下拆开的那种**，每一帧都返回
`kVTVideoDecoderMalfunctionErr (-12911)`，一帧都解不出来 → 黑屏。

修法：**在送进 VideoToolbox 之前，把这两个 OBU 合并回单个 `OBU_FRAME`**。纯字节搬运，
不需要解析熵编码内容，唯一的位级操作是清掉一个停止位。本文附完整可直接使用的 C 实现。

这跟 HDR10+ 没关系。触发条件是**编码器选了拆开的 OBU 打包**，目前已知 NVENC 在 AV1 HDR
路径下会这么发（AMF 不会，NVENC 的 AV1 SDR 也不会）。只要你的客户端支持 AV1 且用
VideoToolbox 解码，连 NVENC 主机开 HDR 就会中招；换个别的编码器如果也这么打包，同样会中。

---

## 症状

macOS 上表现为（iOS 上的日志文本会不同，但错误码一样）：

```text
vt decoder cb: output image buffer is null: -12911
HW accel end frame fail.
avcodec_send_packet() failed
```

`-12911` = `kVTVideoDecoderMalfunctionErr`。特征是**每一帧都失败，成功率为零**，
不是偶发花屏或丢帧。客户端如果有「解码失败 → 重启解码器 → 请求 IDR」的逻辑，
就会陷入死循环，用户看到的就是纯黑屏 + 偶尔闪一下。

HEVC（包括 HEVC HDR / HDR10+）在同一台主机上完全正常，所以很容易被误判成
「HDR 元数据的问题」或「主机的问题」。都不是。

## 根因

同一台 M4、同一台 NVENC 主机 `212333.monster`，对比首帧的 OBU 序列：

| 场景 | 首帧 OBU 序列 | 结果 |
|---|---|---|
| AV1 8-bit **SDR**（NVENC） | `TD(2), SeqHdr(1), FRAME(6)` | 正常，连跑数小时 |
| AV1 10-bit **HDR**（NVENC） | `TD(2), SeqHdr(1), FRAME_HEADER(3), TILE_GROUP(4)` | **每帧 -12911** |
| AV1 10-bit HDR（**AMF** 主机） | `TD(2), SeqHdr(1), METADATA(5), FRAME(6)` | 正常 |
| HEVC HDR10+（NVENC） | — | 正常 |

唯一的变量就是 **frame header 和 tile group 有没有合并成一个 `OBU_FRAME`**。
分辨率、tile 数、码率、丢包都排除了：4K 和 1080p 都复现，0% 丢包也复现。

按 AV1 spec，`OBU_FRAME` 只是 `OBU_FRAME_HEADER` + `OBU_TILE_GROUP` 的等价打包形式
（spec 5.10 `frame_obu()`），两者语义完全相同，合法解码器**应该**都支持。
VideoToolbox 显然只实现了 `OBU_FRAME` 这条路。

### 排除掉的几个嫌疑

**HDR10+ 元数据不是元凶。** 最初我以为是 metadata OBU 夹在 frame header 和 tile group
中间违反了 HDR10+ AV1 Metadata Handling Specification 的顺序要求。但 1080p 那次会话的
**第一个 IDR 完全没有 metadata OBU**（序列就是 `2,1,3,4`），照样 -12911。
元数据的存在与位置都不是触发条件。

**FFmpeg 无辜**（如果你的客户端也走 FFmpeg 的 VT hwaccel）。
`videotoolbox_av1.c` 的 `end_frame` 按 `start_unit..nb_unit` 把整段 OBU 原样拼给 VT，
`av1dec.c` 里 `s->nb_unit = i + 1`（`i` 为 tile group 下标）把 tile group 算进了范围，
一个 OBU 都没漏。iOS 如果是自己构造 `CMBlockBuffer` 直接喂 `VTDecompressionSession`，
那更是原样透传，同样中招。

**主机端拧不动。** `nvEncodeAPI.h` 的 `NV_ENC_CONFIG_AV1` / `NV_ENC_PIC_PARAMS_AV1` 里
**没有任何字段控制 OBU 打包方式**（没有 `enableFrameOBU` 之类），由驱动自己决定。
所以只能修在客户端 —— 好处是对**任何**主机都生效：上游 Sunshine、老驱动、GFN 都不用改。

### 时间线

主机端 AV1 的静态 HDR 元数据（`pMasteringDisplay` / `pMaxCll`）是 foundation-sunshine
`a3bd8799`（2025-12-26，#389）加进去的。**macOS/iOS 上的 AV1 HDR 很可能从 2025 年 12 月起
就一直是黑的**，不是最近哪个 PR 弄坏的。如果 iOS 那边有「AV1 开 HDR 就黑屏」的老 issue，
八成就是这个。

---

## 修法

在**把 temporal unit 交给 VideoToolbox 之前**，就地把
`FRAME_HEADER + TILE_GROUP` 合并成单个 `OBU_FRAME`，顺带把夹在中间的 metadata OBU
提到合并帧之前。

### 三个关键点

**1. `trailing_bits()` 必须处理**（唯一的位级操作，漏掉会解出花屏或直接失败）

`OBU_FRAME_HEADER` 的载荷末尾是 `trailing_bits(obu_size * 8 - payloadBits)`：一个 `1` 停止位，
然后**一路补零到 `obu_size` 声明的末尾**。注意 `nbBits` 是按 `obu_size` 算的，所以编码器如果把
`obu_size` 报大了，补零可以跨越整字节 —— **载荷最后一个字节合法地是 `0x00`**。

而在 `OBU_FRAME` 内部，`frame_header_obu()` 后面跟的是 `byte_alignment()`：

```c
byte_alignment() {
    while ( get_position( ) & 7 )
        zero_bit
}
```

它**只补到下一个字节边界，永远跨不过一整个字节**。所以合并时要做三件事：

1. 清掉停止位 —— 即**最后一个非零字节**的最低置位位（`lastByte &= lastByte - 1`，例 `0x88` → `0x80`）；
2. **丢掉它后面所有的整零字节**。留着的话，它们会被挪进 `tile_group_obu()` 的载荷里，把帧解坏。
3. 如果这个字节的**原值恰好是 `0x80`**，**把它也整个丢掉**，而不是留一个零字节在那里。

第 3 条特别容易漏：`frame_header_obu()` 正好在字节边界结束时，`trailing_bits()` 补出来的就是
整个 `0x80` 字节，概率大约 1/8，不是边角情况。而合并后 `byte_alignment()` 在这里一位都不补，
所以那个字节必须消失。留成 `0x00` 跟第 2 条留着补零字节是**同一个 bug**，一样会解坏帧。

但判据必须是**原值等于 `0x80`**，不能是「清完停止位之后等于 `0x00`」—— 后者太宽。
比如 `0x20`（`0010 0000`）清完也是 `0x00`，可它的高 2 位是**真实的载荷位**（值恰好为零），
只是 `frame_header_obu()` 在这个字节里只用掉了 2 位。这时 `byte_alignment()` 会补满剩下的
6 位，字节仍然存在，必须原样留成 `0x00`。丢掉它就会把 `tile_group_obu()` 整体前移一个字节。
只有原值 `0x80` 时停止位落在字节的第一位，整个字节不含任何载荷位，才该消失。

只做第 1 件事是**错的**，这是个很容易踩的坑。若整个载荷全是零（或只有一个停止位），
说明码流非法，放弃重写。

**2. 输出恒不变长，可以就地覆写**

- 旧开销：`2` 个 OBU 头 + `leb128(fh) + leb128(tg)`
- 新开销：`1` 个 OBU 头 + `leb128(fh + tg)`

因为 `leb128(a+b) <= leb128(a) + leb128(b)`，新开销恒小于旧开销，所以**输出永远不比输入长**，
不需要额外分配缓冲区。搬运顺序从后往前（先 tile group 再 frame header），
保证没有一次 `memmove` 会踩到后面还要读的字节。

这里有个坑：算「原 OBU 头去掉长度字段之后还剩几字节」时，必须用**长度字段实际占的字节数**，
不能拿 `leb128Size(payloadLength)` 反推。AV1 的 leb128 **允许非最短编码**（载荷长度 1
可以写成 `81 00`），拿规范长度去减会少减一个字节，结果在合并头和载荷之间留下游离字节，
整个 TU 的 OBU 边界就错位了。所以解析时要把实际读到的字节数存下来。

metadata 是唯一需要临时暂存的部分 —— 它要往前挪，而 frame header 的载荷要往后挪，
两者会交叉。栈上 512 字节足够（HDR10+ T.35 / mastering display / MaxCLL 都远小于 100 字节）。
这 512 字节算的是**所有待前移 metadata OBU 的头 + 载荷之和**：合计 **≤ 512 接受，≥ 513
在任何拷贝发生之前就原样返回**，所以 `memcpy` 到栈缓冲永远不会越界。

**3. 保守匹配，不认识就别动**

只处理「恰好一个 frame header，其后恰好一个 tile group 且位于 TU 末尾，两者之间只有 metadata」
这一种布局。以下情况一律**原样返回、一个字节都不碰**：

- 已经是 `OBU_FRAME`（AMF 主机、NVENC 的 SDR 路径）
- 多个 tile group（多 tile 分片传输）
- 多帧 TU
- 任何 OBU 的 `obu_has_size_field == 0`（长度不可解，动不得）
- frame header 载荷全是 `0x00`，或者除了停止位什么都没有（码流非法）
- 待前移的 metadata 合计超过 512 字节
- frame header 和 tile group 的 `obu_extension_flag` 不一致，或扩展字节
  （`temporal_id` / `spatial_id`）不同 —— 合并会沿用 frame header 的 OBU 头，
  两者不一致就等于把 tile group 挪进了别的时域/空域层

反过来说，命中重写的条件只跟**布局**有关，跟是哪个编码器无关：任何编码器只要发出
「frame header + metadata + tile group」这种拆开的形式，都会被合并。上面列出的情况
则一律一个字节都不碰。所以对已经发 `OBU_FRAME` 的码流（AMF、NVENC 的 SDR 路径）
以及所有非 AV1 / 非 VideoToolbox 的路径，这段代码是零影响的空操作。

### 完整实现（纯 C，可直接拖进 Xcode 工程）

不依赖 FFmpeg、不依赖任何库，只用 `<stdint.h>` 和 `<string.h>`。
头文件带 `extern "C"` 守卫，`.m` / `.mm` / `.cpp` 都能用。

**`av1obu.h`**

```c
#ifndef AV1OBU_H
#define AV1OBU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
```

**`av1obu.c`**

```c
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

typedef struct {
    int type;
    int headerOffset;   // start of the OBU header
    int headerLength;   // header + leb128 size field
    int sizeBytes;      // length of the leb128 size field as actually encoded
    int payloadOffset;
    int payloadLength;
} Obu;

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

    Obu frameHeader = obus[frameHeaderIdx];
    Obu tileGroup = obus[tileGroupIdx];

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
    int dropLastByte = (origLastByte == 0x80);
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
```

> 上面这份 C 版本我用 `clang -std=c99 -Wall -Wextra` 编过，零警告，
> 并且用同一套 harness 跑出来跟 moonlight-qt 里的 C++ 版**逐字节一致**。

### 接在哪里

调用点要满足两个条件：**整个 temporal unit 已经拼成一段连续内存**，且**还没交给 VideoToolbox**。

对 moonlight-common-c 的客户端来说这很好定位：AV1 的 DU **所有 buffer 都是
`BUFFER_TYPE_PICDATA`**（见 `Limelight.h`，只有 H.264/HEVC 才会拆出 VPS/SPS/PPS 类型），
所以在 `DecoderRendererSubmitDecodeUnit` 回调里把 `du->bufferList` 顺着 `next` 拼完之后，
缓冲区里就是完整的一个 temporal unit。在这之后、构造 `CMBlockBuffer` 之前插一行：

```c
if (needsAv1ObuRepack) {
    offset = repackAv1TemporalUnit(buffer, offset);
}
// 然后拿 offset 当长度去建 CMBlockBuffer / CMSampleBuffer
```

moonlight-qt 里就是这么接的（`ffmpeg.cpp` 的 `submitDecodeUnit()`，紧跟在
`writeBuffer()` 循环之后、`m_Pkt->size = offset` 之前）。

**开关建议**：只在「AV1 + VideoToolbox」时置位，别无条件开。

```c
needsAv1ObuRepack = (videoFormat & VIDEO_FORMAT_MASK_AV1) != 0;
```

iOS 上解码器只有 VideoToolbox 一条路，所以判 AV1 就够了。
不建议再按「是否 HDR」收窄 —— 触发条件是编码器的打包选择，不是 HDR 本身，
而且函数对已经是 `OBU_FRAME` 的码流是零成本空操作（解析几个 OBU 头就返回了）。

### 注意事项

- **不要动 sequence header。** 重写只碰 frame header / tile group / metadata。
  如果你从 sequence header 构造 `av1C` / `CMFormatDescription`，那条路完全不受影响。
- **函数会就地修改缓冲区。** 确保传进去的是你自己的可写拼接缓冲，不是 moonlight-common-c
  的 DU 内存。
- **返回值是新长度，必须用它**，别继续用原来的 `offset`，否则尾部会多出 2 字节垃圾。
- **性能可忽略。** 只解析 OBU 头 + 两次 `memmove`，4K 帧上是微秒级，
  而且不匹配时连 `memmove` 都不会发生。

---

## 验证

### 单元层面

我用的 harness 覆盖了这些用例，移植后建议照着跑一遍：

| 用例 | 期望 |
|---|---|
| 拆帧、无 metadata | `2,1,3,4` → `2,1,6`；长度 63→61；停止位 `0x88`→`0x80` |
| 拆帧、带 metadata | `2,1,3,5,4` → `2,1,5,6`；metadata 前移，内容不变 |
| 已是 `OBU_FRAME` | 原样返回，字节全等 |
| AMF 布局 `2,5,6` | 原样返回，字节全等 |
| 两个 tile group | 原样返回，字节全等 |
| 只有 tile group、没有 frame header | 原样返回，字节全等 |
| frame header 尾部有整零字节填充 | 零字节被截掉，输出与「没有填充」的同一帧**逐字节相同** |
| frame header 载荷全为 `0x00` | 原样返回，字节全等 |
| metadata 合计 512 字节 | 正常重写（边界内） |
| metadata 合计 513 字节 | 原样返回，字节全等（拷贝前就退出） |
| frame header 末字节正好是 `0x80` | 该字节被整个丢掉，不是写成 `0x00` |
| frame header 末字节是 `0x20` 等其他单比特值 | 该字节**保留**并写成 `0x00`（高位是真实载荷位） |
| 长度字段用非最短 leb128（如 `81 00`） | 输出与最短编码的同一帧**逐字节相同** |
| fh / tg 扩展头不一致 | 原样返回，字节全等 |
| 合并后长度跨 leb128 边界（如 203） | 长度字段写成 `cb 01`，总长 208→206 |
| `obu_extension_flag` 置位 | 合并头 `0x36`，ext 字节原样保留，长度正确 |

一个通用的自检：重写后从头把 OBU 走一遍，**消耗的字节数必须正好等于返回的新长度**。

### 实机

连一台 NVENC 主机开 AV1 + HDR，主判据：

1. **出画、不黑**，且 `-12911` 出现 0 次。
2. 如果你能打出 OBU 序列，应该从 `2,1,3,5,4` 变成 `2,1,5,6`。
3. 顺带看一眼 HDR10+ 有没有被识别 —— metadata 前移之后正好满足
   HDR10+ AV1 Metadata Handling Specification「metadata 须先于 frame header」的要求，
   AV1 的动态元数据理应也能被取出来用上。

moonlight-qt 上的实测结果（M4、4K120、AV1 10-bit HDR、NVENC 主机）：

```text
Video stream is 3840x2160x120 (format 0x2000)   # AV1 MAIN10，请求里 hdrMode=1
Using AV1 OBU repack for VideoToolbox
[av1] Format videotoolbox_vld chosen by get_format().
[av1] Total OBUs on this packet: 3.   OBU idx:0 type:2 / idx:1 type:1 / idx:2 type:6
Received HDR10+ dynamic metadata from the AV1 bitstream
```

- `-12911` / `output image buffer is null` / `HW accel end frame fail` /
  `avcodec_send_packet() failed` **各 0 次**。
- 整场会话里**没有出现过一个 `type:3` 或 `type:4`** —— 拆帧全部被合并掉了。
- HDR10+ 那行**没有 `(ignored: ...)` 后缀**，说明渲染器真的取用了动态元数据。
- 55.0 Rx / 55.0 De / 54.1 Rd，丢包 0.00%，解码 4.16ms。

注意这里渲染器是 Vulkan(libplacebo)，但硬解走的仍然是 `videotoolbox_vld`
（日志里的 `AV1 decode get format: videotoolbox_vld`），所以命中的正是原来黑屏的那条路。
判断开关时要看的是 **hwaccel 是不是 VideoToolbox**，不是渲染器是什么。

**这份实测能证明什么、不能证明什么：** 它证明了合并逻辑在真实码流上有效、且没有引入回归。
但日志里只看得到重写**之后**的结果，看不出这台编码器有没有把 `obu_size` 报大、
在 frame header 尾部留下整字节补零 —— 也就是「截掉尾部整零填充」那条分支
**是否被实机走到过，无法从日志判定**。那条分支目前由单元测试覆盖
（有补零和无补零的同一帧输出逐字节相同）。它是纯收紧的改动：没有补零时行为与不做截断完全一致。

### 回归

这三条路重写函数都会原样返回、一个字节都不碰，但还是各连一次更稳：

- AMF 主机的 AV1 HDR（本来就发 `OBU_FRAME`）
- NVENC 的 AV1 SDR（同上）
- HEVC（`needsAv1ObuRepack` 为 false，一行都走不到）

---

## 附：一个顺带发现（不影响本问题）

foundation-sunshine 的 `src/nvenc/common_impl/nvenc_base.cpp` 里，
AV1 的 `outputMaxCll` / `outputMasteringDisplay` 是**无条件**设的 —— SDR 也设。
看着像个独立的小 bug，但跟这次的黑屏无关，也不在本次修复范围内。
