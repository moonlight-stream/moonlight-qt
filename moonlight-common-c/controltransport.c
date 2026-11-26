#include "controltransport.h"
#include "moonlight-common-c/src/ControlStream.c"

enum MlControlDisconnect MlGetControlDisconnect(void) {
    enum MlControlDisconnect cause = MlControlStillConnected;
    PltLockMutex(&enetMutex);
    // The upstream intercept records a peer-addressed DISCONNECT even if the
    // peer sends it only once. ENet also emits a disconnect on a local timeout.
    if (disconnectPending) {
        cause = MlControlClosedByPeer;
    }
    else if (peer && (peer->state == ENET_PEER_STATE_DISCONNECTED || peer->state == ENET_PEER_STATE_ZOMBIE)) {
        cause = MlControlTimedOut;
    }
    PltUnlockMutex(&enetMutex);
    return cause;
}
