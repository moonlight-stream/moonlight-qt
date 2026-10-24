// MoonlightAWDLHelper.m
//
// Privileged helper tool for Moonlight, installed via SMJobBless and run
// as root by launchd. It exists for one purpose: toggling Apple Wireless
// Direct Link (interface awdl0) on and off so that AWDL's periodic Wi-Fi
// radio use does not stutter game streams.
//
// SECURITY MODEL -- read before touching this file:
//  * The helper accepts exactly one XPC operation (setAwdlEnabled:) whose
//    only parameter is a boolean. It never accepts commands, paths, shell
//    strings, or any other caller-controlled data.
//  * The ifconfig invocation uses a fixed launch path (/sbin/ifconfig)
//    and fixed arguments ("awdl0", "up"/"down"). No shell is involved.
//  * Every connecting client is verified in
//    listener:shouldAcceptNewConnection: against the code-signing
//    requirement for the Moonlight app (same team identifier as the helper
//    itself when one is present, otherwise identifier + Apple anchor).
//    Connections from anything else are rejected -- this is the fix for
//    the class of flaw described in CVE-2019-20057 against Apple's
//    EvenBetterAuthorizationSample, whose helper accepted any client.
//  * The helper runs no GUI code and loads no plugins.

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import "AWDLHelperProtocol.h"

// Mach service name / launchd job label. Must match:
//  * The file name of this tool inside Moonlight.app/Contents/Library/LaunchServices/
//  * The key in Moonlight's Info.plist SMPrivilegedExecutables dictionary
//  * The Label in this tool's embedded launchd plist
static NSString * const kHelperLabel = @"com.moonlight-stream.Moonlight.AWDLHelper";

// Bundle identifier of the Moonlight app allowed to talk to this helper.
static NSString * const kMoonlightBundleIdentifier = @"com.moonlight-stream.Moonlight";

static BOOL ClientMeetsRequirement(NSXPCConnection *connection, NSString *requirementString)
{
    // kSecGuestAttributeAuditToken takes CFData containing an audit_token_t
    audit_token_t auditToken = connection.auditToken;
    NSData *tokenData = [NSData dataWithBytes:&auditToken length:sizeof(auditToken)];
    NSDictionary *attributes = @{(__bridge NSString *)kSecGuestAttributeAuditToken: tokenData};
    SecCodeRef clientCode = NULL;
    OSStatus status = SecCodeCopyGuestWithAttributes(NULL,
        (__bridge CFDictionaryRef)attributes,
        kSecCSDefaultFlags, &clientCode);
    if (status != errSecSuccess || clientCode == NULL) {
        return NO;
    }
    SecRequirementRef requirement = NULL;
    status = SecRequirementCreateWithString((__bridge CFStringRef)requirementString,
                                            kSecCSDefaultFlags, &requirement);
    if (status != errSecSuccess || requirement == NULL) {
        CFRelease(clientCode);
        return NO;
    }
    status = SecCodeCheckValidity(clientCode, kSecCSDefaultFlags, requirement);
    CFRelease(requirement);
    CFRelease(clientCode);
    return status == errSecSuccess;
}

// Build the requirement the helper enforces on clients: the Moonlight
// bundle identifier, signed by the same team as this helper when the
// helper itself carries a team identifier. This avoids hard-coding a team
// ID at authoring time while still binding clients to our developer team.
// Falls back to identifier + Apple anchor when the helper has no team ID
// (e.g. ad-hoc signature), in which case the plist-side SMAuthorizedClients
// requirement remains the primary gate.
static NSString *ClientRequirementString(void)
{
    NSString *teamID = nil;
    SecCodeRef selfCode = NULL;
    if (SecCodeCopySelf(kSecCSDefaultFlags, &selfCode) == errSecSuccess && selfCode != NULL) {
        CFDictionaryRef signingInfo = NULL;
        if (SecCodeCopySigningInformation(selfCode, kSecCSSigningInformation, &signingInfo) == errSecSuccess && signingInfo != NULL) {
            teamID = (__bridge NSString *)CFDictionaryGetValue(signingInfo, kSecCodeInfoTeamIdentifier);
            CFRelease(signingInfo);
        }
        CFRelease(selfCode);
    }
    if (teamID != nil && teamID.length > 0) {
        return [NSString stringWithFormat:@"identifier \"%@\" and certificate leaf[subject.OU] = \"%@\"",
                kMoonlightBundleIdentifier, teamID];
    }
    return [NSString stringWithFormat:@"identifier \"%@\" and anchor apple generic",
            kMoonlightBundleIdentifier];
}

@interface AWDLHelper : NSObject <NSXPCListenerDelegate, AWDLHelperProtocol>
@end

@implementation AWDLHelper

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    // Reject any client that is not the Moonlight app. The Mach service
    // name is public, so any local process can attempt a connection --
    // this check is what stops a malicious process from toggling AWDL
    // (or probing a root service) through us.
    if (!ClientMeetsRequirement(newConnection, ClientRequirementString())) {
        NSLog(@"MoonlightAWDLHelper: rejected connection from unauthorized client (pid %d)", newConnection.processIdentifier);
        return NO;
    }
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(AWDLHelperProtocol)];
    newConnection.exportedObject = self;
    [newConnection resume];
    return YES;
}

- (void)setAwdlEnabled:(BOOL)enabled withReply:(void (^)(BOOL, NSError *))reply
{
    // Fixed command, fixed arguments, no shell, no caller input.
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/sbin/ifconfig";
    task.arguments = @[@"awdl0", enabled ? @"up" : @"down"];
    task.standardOutput = [NSPipe pipe];
    task.standardError = [NSPipe pipe];
    @try {
        [task run];
    } @catch (NSException *exception) {
        reply(NO, [NSError errorWithDomain:@"MoonlightAWDLHelper" code:1 userInfo:@{NSLocalizedDescriptionKey: exception.reason ?: @"Failed to launch ifconfig"}]);
        return;
    }
    [task waitUntilExit];
    if (task.terminationStatus == 0) {
        NSLog(@"MoonlightAWDLHelper: awdl0 %@", enabled ? @"up" : @"down");
        reply(YES, nil);
    } else {
        NSString *detail = [[NSString alloc] initWithData:[[task.standardError fileHandleForReading] readDataToEndOfFile] encoding:NSUTF8StringEncoding] ?: @"";
        reply(NO, [NSError errorWithDomain:@"MoonlightAWDLHelper" code:(NSInteger)task.terminationStatus userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"ifconfig awdl0 %@ failed (exit %d): %@", enabled ? @"up" : @"down", task.terminationStatus, detail]}]);
    }
}

@end

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        // launchd starts this tool on demand when a client connects to
        // its Mach service; it exits when idle (see the launchd plist).
        NSXPCListener *listener = [[NSXPCListener alloc] initWithMachServiceName:kHelperLabel];
        AWDLHelper *helper = [[AWDLHelper alloc] init];
        listener.delegate = helper;
        [listener resume];
        [[NSRunLoop currentRunLoop] run];
    }
    return 0;
}
