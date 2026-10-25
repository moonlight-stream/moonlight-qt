// macpermissions.mm - macOS microphone permission pre-check
//
// On macOS, accessing QMediaDevices (CoreAudio) before the user has
// granted microphone permission can trigger multiple TCC dialogs,
// especially with universal (fat) binaries where each architecture
// slice has a different CDHash. By using AVCaptureDevice to request
// permission once before touching QMediaDevices we ensure only a
// single system dialog is shown.

#include "macpermissions.h"

#import <AVFoundation/AVFoundation.h>
#import <dispatch/dispatch.h>

bool checkAndRequestMicrophonePermission()
{
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

    switch (status) {
    case AVAuthorizationStatusAuthorized:
        return true;

    case AVAuthorizationStatusNotDetermined: {
        // Block until the user responds to the single permission dialog.
        __block BOOL granted = NO;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                 completionHandler:^(BOOL g) {
            granted = g;
            dispatch_semaphore_signal(sem);
        }];
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        return granted;
    }

    case AVAuthorizationStatusDenied:
    case AVAuthorizationStatusRestricted:
    default:
        return false;
    }
}
