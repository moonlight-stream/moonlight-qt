// AWDLHelperProtocol.h
//
// Shared XPC protocol between the Moonlight app and its privileged helper
// tool (MoonlightAWDLHelper). The helper exposes EXACTLY ONE operation:
// enable or disable Apple Wireless Direct Link (interface awdl0).
//
// The protocol deliberately takes no strings, paths, commands, or any other
// caller-controlled data -- the only parameter is a boolean. The helper
// itself decides the exact command (/sbin/ifconfig awdl0 up|down) with
// fixed arguments, so a compromised or malicious client cannot turn the
// helper into a generic root command runner.

#import <Foundation/Foundation.h>

@protocol AWDLHelperProtocol

// Enable (YES) or disable (NO) AWDL. The reply reports success and, on
// failure, an NSError describing what went wrong (e.g. ifconfig failure).
- (void)setAwdlEnabled:(BOOL)enabled
             withReply:(void (^)(BOOL success, NSError * _Nullable error))reply;

@end
