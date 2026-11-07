#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Only the opted-in session owner calls start/stop. Never capture from a late
// permission callback. Status: 1 running, 0 stopped, -1 capture/transport error.
int startNativeMic(const char*, unsigned short, const char*, const char*);
// Synthetic diagnostic only; this never opens an input device.
int startNativeTone(const char*, unsigned short, const char*, const char*);
void stopNativeMic(void);
int nativeMicStatus(void);
#ifdef __cplusplus
}
#endif
