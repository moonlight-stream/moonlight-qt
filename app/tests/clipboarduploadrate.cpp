// Run as a standalone C++17 executable.
#include "../streaming/clipboarduploadrate.h"
#include <cassert>

int main()
{
    constexpr int MiB = 1024 * 1024;
    ClipboardUploadRate gigabit(1'000'000'000);
    assert(gigabit.bytesPerSecond() == 2 * MiB);
    gigabit.sample(3, 1);
    assert(gigabit.bytesPerSecond() == 31'250'000);
    assert(gigabit.chunkBytes() == 31'250'000 / 64);

    for (int i = 0; i < 30; ++i) gigabit.sample(4, 2);
    assert(gigabit.bytesPerSecond() == 93'750'000);
    assert(gigabit.chunkBytes() == MiB);
    gigabit.sample(35, 2);  // Control traffic is queueing.
    assert(gigabit.bytesPerSecond() == 46'875'000);
    gigabit.sample(4, 25);  // A variance spike also backs off.
    assert(gigabit.bytesPerSecond() == 23'437'500);
    for (int i = 0; i < 30; ++i) gigabit.sample(50, 25);
    assert(gigabit.bytesPerSecond() == 128 * 1024);
    gigabit.sample(4, 2);
    assert(gigabit.bytesPerSecond() == 160 * 1024);

    ClipboardUploadRate missingSample(1'000'000'000);
    missingSample.sample(0, 0);
    assert(missingSample.bytesPerSecond() == 2 * MiB);
    missingSample.sample(4, 1);
    assert(missingSample.bytesPerSecond() == 31'250'000);
    missingSample.sample(0, 0);
    assert(missingSample.bytesPerSecond() == 2 * MiB);
    missingSample.sample(4, 1);
    assert(missingSample.bytesPerSecond() == 2 * MiB * 5 / 4);

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
