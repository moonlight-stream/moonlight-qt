// Run as a standalone C++17 executable.
#include "../streaming/clipboarduploadrate.h"
#include <cassert>

int main()
{
    constexpr int MiB = 1024 * 1024;
    ClipboardUploadRate upload(0);
    upload.sample(0, 0);
    assert(upload.bytesPerSecond() == 0 && upload.chunkBytes() == MiB);
    upload.sample(3, 1);
    upload.acknowledge(MiB, 8);
    assert(upload.chunkBytes() == 4 * MiB);
    upload.acknowledge(50, 1000); // Ignore a short final chunk.
    assert(upload.chunkBytes() == 4 * MiB);

    upload.sample(45, 20);
    assert(upload.bytesPerSecond() == 65536000);
    assert(upload.chunkBytes() < 4 * MiB);
    upload.sample(0, 0);
    assert(upload.bytesPerSecond() == 65536000);
    for (int i = 0; i < 20; ++i) upload.sample(45, 20);
    assert(upload.bytesPerSecond() == 128 * 1024);
    for (int i = 0; i < 2; ++i) upload.sample(3, 1);
    assert(upload.bytesPerSecond() == 128 * 1024);
    for (int i = 0; i < 40; ++i) upload.sample(3, 1);
    assert(upload.bytesPerSecond() == 0);

    ClipboardUploadRate slow(10000000);
    assert(slow.chunkBytes() == 64 * 1024);
    slow.acknowledge(slow.chunkBytes(), 500);
    assert(slow.chunkBytes() == 64 * 1024);
}
