// macpermissions_stub.cpp - Stub for non-macOS platforms
#include "macpermissions.h"

bool checkAndRequestMicrophonePermission()
{
    // No permission pre-check needed on non-macOS platforms
    return true;
}
