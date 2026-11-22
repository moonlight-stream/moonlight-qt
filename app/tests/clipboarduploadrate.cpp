// Run as a standalone C++17 executable.
#include "../streaming/clipboarduploadrate.h"
#include <cassert>

int main()
{
    constexpr int MiB = 1024 * 1024;
    ClipboardUploadRate gigabit(1'000'000'000);
    assert(gigabit.bytesPerSecond() == 2 * MiB);
    gigabit.sample(3, 1);
    assert(gigabit.bytesPerSecond() == 62'500'000);
    assert(gigabit.chunkBytes() == MiB / 2);
    gigabit.acknowledge(MiB / 2, 8);
    assert(gigabit.chunkBytes() == 62'500'000 / 32);

    for (int i = 0; i < 30; ++i) gigabit.sample(4, 2);
    assert(gigabit.bytesPerSecond() == 112'500'000);
    assert(gigabit.chunkBytes() == 2 * MiB);
    gigabit.acknowledge(gigabit.chunkBytes(), 18);
    assert(gigabit.chunkBytes() == 112'500'000 / 32);
    gigabit.acknowledge(53, 300);  // Final short request of a file.
    assert(gigabit.chunkBytes() == 112'500'000 / 32);
    gigabit.acknowledge(gigabit.chunkBytes(), 320);  // Slow peer on a fast switch.
    assert(gigabit.chunkBytes() == 351'562);
    gigabit.sample(35, 2);  // Control traffic is queueing.
    assert(gigabit.bytesPerSecond() == 56'250'000);
    gigabit.sample(4, 25);  // A variance spike also backs off.
    assert(gigabit.bytesPerSecond() == 28'125'000);
    for (int i = 0; i < 30; ++i) gigabit.sample(50, 25);
    assert(gigabit.bytesPerSecond() == 128 * 1024);
    gigabit.sample(4, 2);
    assert(gigabit.bytesPerSecond() == 192 * 1024);

    ClipboardUploadRate missingSample(1'000'000'000);
    missingSample.sample(0, 0);
    assert(missingSample.bytesPerSecond() == 2 * MiB);
    missingSample.sample(4, 1);
    assert(missingSample.bytesPerSecond() == 62'500'000);
    missingSample.sample(0, 0);
    assert(missingSample.bytesPerSecond() == 2 * MiB);
    missingSample.sample(4, 1);
    assert(missingSample.bytesPerSecond() == 3 * MiB);

    ClipboardUploadRate multiGigabit(2'500'000'000);
    multiGigabit.sample(2, 1);
    assert(multiGigabit.bytesPerSecond() == 64 * MiB);
    for (int i = 0; i < 20; ++i) {
        multiGigabit.sample(2, 1);
        multiGigabit.acknowledge(multiGigabit.chunkBytes(), 2);
    }
    assert(multiGigabit.bytesPerSecond() == 281'250'000);
    assert(multiGigabit.chunkBytes() == 4 * MiB);

    ClipboardUploadRate alreadyBusy(1'000'000'000);
    alreadyBusy.sample(4, 30);
    assert(alreadyBusy.bytesPerSecond() == 2 * MiB);
    ClipboardUploadRate tenMegabit(10'000'000);
    tenMegabit.sample(2, 1);
    assert(tenMegabit.bytesPerSecond() == 312'500);
    for (int i = 0; i < 20; ++i) tenMegabit.sample(3, 1);
    assert(tenMegabit.bytesPerSecond() == 937'500);

    ClipboardUploadRate unknown(0);
    ClipboardUploadRate loopback(0, true);
    for (int i = 0; i < 50; ++i) {
        unknown.sample(1, 1);
        loopback.sample(1, 1);
    }
    assert(unknown.bytesPerSecond() == 2 * MiB && unknown.chunkBytes() == 64 * 1024);
    assert(loopback.bytesPerSecond() == 16 * MiB && loopback.chunkBytes() == 256 * 1024);
}
