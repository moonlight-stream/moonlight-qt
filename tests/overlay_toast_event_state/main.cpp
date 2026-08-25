#include "streaming/video/overlaytoasteventstate.h"

#include <QCoreApplication>
#include <QtGlobal>

#include <limits>

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
    OverlayToastEventState state;

    require(!state.isVisible(), "new toast state must be hidden");
    require(!state.needsEventProcessing(0), "hidden toast must remain idle");
    require(state.nextEventDelayMs(0) == -1, "hidden toast must have no deadline");

    state.show(100, 2000);
    require(state.isVisible(), "shown toast must be visible");
    require(state.needsEventProcessing(100), "new toast must request initial processing");
    require(state.nextEventDelayMs(100) == -1,
            "pending initial paint must take precedence over the deadline");

    require(!state.beginEventProcessing(100), "initial processing must not start fading");
    require(!state.needsEventProcessing(100), "static toast must become idle after painting");
    require(state.nextEventDelayMs(100) == 2000, "static toast must expose its deadline");
    require(!state.beginEventProcessing(500),
            "unrelated Qt work must not start the toast fade early");
    require(state.nextEventDelayMs(500) == 1600,
            "unrelated Qt work must preserve the toast deadline");

    constexpr int idleChecks = 1000000;
    for (int i = 0; i < idleChecks; ++i) {
        require(!state.needsEventProcessing(1000),
                "static toast must not request continuous event pumping");
    }

    require(state.nextEventDelayMs(2099) == 1, "deadline must retain millisecond precision");
    require(state.needsEventProcessing(2100), "expired toast must request processing");
    require(state.beginEventProcessing(2100), "expired toast must enter fade phase");
    require(state.isFading(), "toast must report an active fade");
    require(state.needsEventProcessing(2100), "fade animation must keep processing events");
    require(state.nextEventDelayMs(2100) == -1, "fade phase must not retain a deadline");

    state.finishFade();
    require(!state.isVisible(), "finished fade must hide the toast");
    require(!state.needsEventProcessing(2100), "finished fade must return to idle");

    state.show(3000, 1000);
    state.beginEventProcessing(3000);
    state.show(3200, 4000);
    require(state.needsEventProcessing(3200), "replacement toast must request a fresh paint");
    require(!state.beginEventProcessing(3200), "replacement must not inherit the old deadline");
    require(state.nextEventDelayMs(3200) == 4000,
            "replacement toast must own a new dismissal deadline");
    require(!state.needsEventProcessing(4000),
            "the replaced toast deadline must not dismiss the new toast");

    require(state.beginEventProcessing(7200), "replacement toast must eventually fade");
    state.show(7300, 500);
    require(!state.isFading(), "a toast replacing an active fade must return to static phase");
    require(!state.beginEventProcessing(7300),
            "replacement during fade must receive a full display interval");
    require(state.nextEventDelayMs(7300) == 500,
            "replacement during fade must own its new deadline");

    state.cancel();
    require(!state.isVisible(), "cancel must hide the toast");
    require(state.nextEventDelayMs(4000) == -1, "cancel must clear the deadline");

    state.show(0, std::numeric_limits<int>::max());
    state.beginEventProcessing(0);
    require(state.nextEventDelayMs(0) == std::numeric_limits<int>::max(),
            "large valid deadlines must remain representable");

    state.show(5000, -1);
    require(state.beginEventProcessing(5000),
            "negative durations must be clamped to an immediate fade");

    return 0;
}
