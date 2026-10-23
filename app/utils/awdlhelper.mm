// awdlhelper.mm -- App-side client for the MoonlightAWDLHelper privileged
// helper tool. macOS only.
//
// Flow: when the user enables the "Disable AWDL while streaming" setting,
// Moonlight calls awdlHelperInstall(), which asks macOS (one admin prompt)
// to copy the helper from the app bundle into /Library/PrivilegedHelperTools
// and register it with launchd. At stream start/stop, Moonlight connects
// to the helper's Mach service over XPC and sends the single boolean
// operation it supports. No command strings ever cross this interface.
//
// This file follows the same pattern as Apple's SMJobBless sample code:
// AuthorizationCreate for the com.apple.ServiceManagement.blesshelper right,
// then SMJobBless(kSMDomainSystemLaunchd, ...). SMJobBless is deprecated
// in macOS 13 in favour of SMAppService; it still works, and this code
// keeps the SMJobBless path the project asked for. A future change could
// switch installation to SMAppService.daemon without changing the helper
// or the XPC protocol.

#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>
#import <Security/Security.h>

#include "awdlhelper.h"
#include "AWDLHelperProtocol.h"

const char * const kMoonlightAWDLHelperLabel = "com.moonlight-stream.Moonlight.AWDLHelper";

static NSString *NSStringFromCFError(CFErrorRef error)
{
    if (error == NULL) {
        return @"Unknown error";
    }
    NSString *desc = (__bridge_transfer NSString *)CFErrorCopyDescription(error);
    return desc ?: @"Unknown error";
}

bool awdlHelperIsInstalled()
{
    CFDictionaryRef job = SMJobCopyDictionary(kSMDomainSystemLaunchd,
        (__bridge CFStringRef)[NSString stringWithUTF8String:kMoonlightAWDLHelperLabel]);
    if (job != NULL) {
        CFRelease(job);
        return true;
    }
    return false;
}

bool awdlHelperInstall(QString *errorMessage)
{
    // The helper must be inside this app bundle for SMJobBless to find it:
    // Contents/Library/LaunchServices/<label>
    NSString *helperPath = [[NSBundle mainBundle] pathForAuxiliaryExecutable:
        [NSString stringWithUTF8String:kMoonlightAWDLHelperLabel]];
    if (helperPath == nil) {
        if (errorMessage != NULL) {
            *errorMessage = QStringLiteral("AWDL helper is missing from the app bundle (Contents/Library/LaunchServices). Rebuild with the helper target enabled.");
        }
        return false;
    }

    // The C-string form of the kSMRightBlessPrivilegedHelper right.
    // (The SDK constant is a CFStringRef; AuthorizationItem.name needs
    // a plain C string, so spell the right name out rather than casting.)
    AuthorizationItem authItem = { "com.apple.ServiceManagement.blesshelper", 0, NULL, 0 };
    AuthorizationRights authRights = { 1, &authItem };
    AuthorizationFlags flags = kAuthorizationFlagDefaults |
                               kAuthorizationFlagInteractionAllowed |
                               kAuthorizationFlagPreAuthorize |
                               kAuthorizationFlagExtendRights;
    AuthorizationRef authRef = NULL;
    OSStatus status = AuthorizationCreate(&authRights, kAuthorizationEmptyEnvironment, flags, &authRef);
    if (status != errAuthorizationSuccess) {
        if (errorMessage != NULL) {
            *errorMessage = QStringLiteral("Admin authorization was not granted (user cancelled or not an admin). The AWDL helper was not installed.");
        }
        return false;
    }

    CFErrorRef blessError = NULL;
    Boolean blessed = SMJobBless(kSMDomainSystemLaunchd,
        (__bridge CFStringRef)[NSString stringWithUTF8String:kMoonlightAWDLHelperLabel],
        authRef, &blessError);
    AuthorizationFree(authRef, kAuthorizationFlagDefaults);

    if (!blessed) {
        if (errorMessage != NULL) {
            // Common causes: app and helper signed with different identities,
            // or the build is unsigned (e.g. an unsigned CI artifact). An
            // unsigned build can never bless a helper -- see the PR notes.
            *errorMessage = QStringLiteral("Could not install the AWDL helper: %1. The app and helper must be signed with the same identity; unsigned builds cannot install helpers.")
                .arg(QString::fromUtf8([NSStringFromCFError(blessError) UTF8String]));
        }
        if (blessError != NULL) {
            CFRelease(blessError);
        }
        return false;
    }
    if (blessError != NULL) {
        CFRelease(blessError);
    }
    return true;
}

bool awdlHelperSetEnabled(bool enabled, int timeoutMs, QString *errorMessage)
{
    __block BOOL success = NO;
    __block NSError *opError = nil;
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    NSXPCConnection *connection = [[NSXPCConnection alloc]
        initWithMachServiceName:[NSString stringWithUTF8String:kMoonlightAWDLHelperLabel]
        options:NSXPCConnectionPrivileged];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(AWDLHelperProtocol)];
    connection.invalidationHandler = ^{
        dispatch_semaphore_signal(sema);
    };
    [connection resume];

    id<AWDLHelperProtocol> proxy = [connection remoteObjectProxyWithErrorHandler:^(NSError *error) {
        opError = error;
        dispatch_semaphore_signal(sema);
    }];
    [proxy setAwdlEnabled:(enabled ? YES : NO) withReply:^(BOOL replySuccess, NSError *replyError) {
        success = replySuccess;
        opError = replyError;
        dispatch_semaphore_signal(sema);
    }];

    dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * NSEC_PER_MSEC);
    if (dispatch_semaphore_wait(sema, deadline) != 0) {
        if (errorMessage != NULL) {
            *errorMessage = QStringLiteral("Timed out waiting for the AWDL helper. It may not be installed; try toggling the setting off and on to reinstall it.");
        }
        [connection invalidate];
        return false;
    }
    [connection invalidate];

    if (!success) {
        if (errorMessage != NULL) {
            *errorMessage = opError != nil
                ? QString::fromUtf8([opError.localizedDescription UTF8String])
                : QStringLiteral("The AWDL helper did not confirm the change. It may not be installed.");
        }
        return false;
    }
    return true;
}

void awdlHelperRestoreAtStartupAsync()
{
    // Never block startup (or the main thread) on the helper: the XPC
    // reply is waited on with a semaphore, so do it on a background queue.
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        if (!awdlHelperIsInstalled()) {
            return;
        }
        QString errorMessage;
        if (!awdlHelperSetEnabled(true, 10000, &errorMessage)) {
            NSLog(@"Moonlight: failed to restore AWDL at startup: %@", [NSString stringWithUTF8String:errorMessage.toUtf8().constData()]);
        }
    });
}
