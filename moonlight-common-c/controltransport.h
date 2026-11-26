#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum MlControlDisconnect {
    MlControlStillConnected,
    MlControlClosedByPeer,
    MlControlTimedOut
};

// Query after an established connection failed and before calling LiStopConnection().
enum MlControlDisconnect MlGetControlDisconnect(void);

#ifdef __cplusplus
}
#endif
