#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Set before LiStartConnection, after any previous LiStopConnection completes.
void MlSetViewOnlyInput(int enabled);

#ifdef __cplusplus
}
#endif
