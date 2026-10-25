#pragma once

// Pre-check and request microphone permission on macOS.
// Returns true if permission is granted, false otherwise.
// On non-macOS platforms, always returns true.
bool checkAndRequestMicrophonePermission();
