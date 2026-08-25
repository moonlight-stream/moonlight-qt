#include "streaming/video/overlayeventwakestate.h"

#include <QCoreApplication>
#include <QtGlobal>

#include <atomic>
#include <thread>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) {
        qFatal("%s", message);
    }
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    OverlayEventWakeState state;
    constexpr int keyPairs = 1000000;

    // An idle visible button must not create work while keyboard events pass
    // through the SDL loop. This is the regression that caused continuous Qt
    // pumping after v6.3.7.
    for (int i = 0; i < keyPairs; ++i) {
        require(!state.isPending(),
                "idle overlay state must remain quiet during keyboard pressure");
    }

    require(state.request(), "first request must schedule a wakeup");
    require(!state.request(), "repeated requests must be coalesced");
    require(state.take(), "pending request must be consumed");
    require(!state.take(), "consumed state must remain clear");

    constexpr int producerCount = 4;
    constexpr int requestsPerProducer = 250000;
    std::atomic_int activeProducers{producerCount};
    std::atomic_int scheduledWakeups{0};
    std::atomic_int consumedWakeups{0};
    std::vector<std::thread> producers;
    producers.reserve(producerCount);

    for (int producer = 0; producer < producerCount; ++producer) {
        producers.emplace_back([&]() {
            for (int i = 0; i < requestsPerProducer; ++i) {
                if (state.request()) {
                    scheduledWakeups.fetch_add(1, std::memory_order_relaxed);
                }
            }
            activeProducers.fetch_sub(1, std::memory_order_release);
        });
    }

    while (activeProducers.load(std::memory_order_acquire) != 0 ||
           state.isPending()) {
        if (state.take()) {
            consumedWakeups.fetch_add(1, std::memory_order_relaxed);
        }
        std::this_thread::yield();
    }

    for (auto& producer : producers) {
        producer.join();
    }
    if (state.take()) {
        consumedWakeups.fetch_add(1, std::memory_order_relaxed);
    }

    require(scheduledWakeups.load() == consumedWakeups.load(),
            "every scheduled wakeup edge must be consumable");
    require(!state.isPending(), "stress test must finish with a clear state");

    return 0;
}
