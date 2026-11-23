#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Set before LiStartConnection, after any previous LiStopConnection completes.
void MlSetViewOnlyInput(int enabled);

#ifdef _WIN32
// End the remote Explorer drag and release its primary button in wire order.
int MlCancelFileDrag(void);
#endif

#ifdef __cplusplus
}
#endif
