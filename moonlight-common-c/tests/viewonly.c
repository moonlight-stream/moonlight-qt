// Linked with the same moonlight-common-c library as the client. No host is
// needed: the interactive control case only sends a zero-distance no-op.
#include "../inputmode.h"
#include "Limelight-internal.h"
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "Failed: %s\n", #expr); return 1; } } while (0)

static void discardLog(const char* format, ...) { (void)format; }

int main(void) {
    CHECK(initializePlatform() == 0);
    ListenerCallbacks.logMessage = discardLog;
    AppVersionQuad[0] = 6;  // Control-channel input, no haptics startup packet.

    for (int cycle = 0; cycle < 3; ++cycle) {
        int viewing = cycle != 1;
        MlSetViewOnlyInput(viewing);
        CHECK(initializeInputStream() == 0);
        CHECK(startInputStream() == 0);
        if (viewing) {
            CHECK(LiSendMouseMoveEvent(1, 1) == -2);
            CHECK(LiSendMousePositionEvent(1, 1, 100, 100) == -2);
            CHECK(LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT) == -2);
#ifdef _WIN32
            CHECK(MlCancelFileDrag() == -2);
#endif
            CHECK(LiSendHighResScrollEvent(120) == -2);
            CHECK(LiSendKeyboardEvent(0x41, KEY_ACTION_DOWN, 0) == -2);
            CHECK(LiSendUtf8TextEvent("paste", 5) == -2);
            CHECK(LiSendTouchEvent(LI_TOUCH_EVENT_DOWN, 1, 0, 0, 1, 0, 0, 0) == -2);
            CHECK(LiSendPenEvent(LI_TOUCH_EVENT_DOWN, LI_TOOL_TYPE_PEN, 0, 0, 0, 1, 0, 0, 0, 0) == -2);
            CHECK(LiSendControllerArrivalEvent(0, 1, 0, 0, 0) == -2);
            CHECK(LiSendMultiControllerEvent(0, 1, 0, 0, 0, 0, 0, 0, 0) == -2);
            CHECK(LiSendControllerMotionEvent(0, LI_MOTION_TYPE_GYRO, 0, 0, 0) == -2);
            CHECK(LiSendControllerTouchEvent(0, LI_TOUCH_EVENT_DOWN, 1, 0, 0, 1) == -2);
            CHECK(LiSendControllerBatteryEvent(0, 0, 100) == -2);
        } else {
            CHECK(LiSendMouseMoveEvent(0, 0) == 0);
        }
        CHECK(stopInputStream() == 0);
        destroyInputStream();
    }

    cleanupPlatform();
    puts("View-only and interactive input lifetimes passed.");
    return 0;
}
