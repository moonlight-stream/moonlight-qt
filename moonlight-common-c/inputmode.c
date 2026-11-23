#include "inputmode.h"

// Keep the upstream packet encoder unchanged. A viewing session skips the
// input sender's entire lifetime, leaving every LiSend* input API inactive.
// This also suppresses the mouse wake-up sent by LiStartConnection itself.
#define startInputStream startInteractiveInputStream
#define stopInputStream stopInteractiveInputStream
#ifdef _WIN32
#include "moonlight-common-c/src/Limelight-internal.h"
enum { normalKeyboardChannel = CTRL_CHANNEL_KEYBOARD };
static _Thread_local int cancellingFileDrag;
// ENet orders packets within a channel. During a file handoff Escape must
// reach Explorer before mouse-up, including when either packet is retransmitted.
#undef CTRL_CHANNEL_KEYBOARD
#define CTRL_CHANNEL_KEYBOARD (cancellingFileDrag ? CTRL_CHANNEL_MOUSE : normalKeyboardChannel)
#endif
#include "moonlight-common-c/src/InputStream.c"
#undef startInputStream
#undef stopInputStream

#ifdef _WIN32
int MlCancelFileDrag(void) {
    const short escape = (short)0x801b;
    cancellingFileDrag = 1;
    const int down = LiSendKeyboardEvent(escape, KEY_ACTION_DOWN, 0);
    const int release = LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
    const int up = LiSendKeyboardEvent(escape, KEY_ACTION_UP, 0);
    cancellingFileDrag = 0;
    if (down != 0) return down;
    return release != 0 ? release : up;
}
#endif

static int viewOnly;

void MlSetViewOnlyInput(int enabled) {
    viewOnly = enabled;
}

int startInputStream(void) {
    return viewOnly ? 0 : startInteractiveInputStream();
}

int stopInputStream(void) {
    return viewOnly ? 0 : stopInteractiveInputStream();
}
