#include "inputmode.h"

// Keep the upstream packet encoder unchanged. A viewing session skips the
// input sender's entire lifetime, leaving every LiSend* input API inactive.
// This also suppresses the mouse wake-up sent by LiStartConnection itself.
#define startInputStream startInteractiveInputStream
#define stopInputStream stopInteractiveInputStream
#include "moonlight-common-c/src/InputStream.c"
#undef startInputStream
#undef stopInputStream

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
