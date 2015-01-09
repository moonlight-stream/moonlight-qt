//
//  WakeOnLanManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 1/2/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "WakeOnLanManager.h"
#import "Utils.h"
#import <CoreFoundation/CoreFoundation.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>

@implementation WakeOnLanManager

static const int numPorts = 5;
static const int ports[numPorts] = {7, 9, 47998, 47999, 48000};

+ (void) wakeHost:(Host*)host {
    NSData* wolPayload = [WakeOnLanManager createPayload:host];
    CFDataRef dataPayload = CFDataCreate(kCFAllocatorDefault, [wolPayload bytes], [wolPayload length]);
    CFSocketRef wolSocket = CFSocketCreate(kCFAllocatorDefault, PF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, NULL, NULL);
    if (!wolSocket) {
        CFRelease(dataPayload);
        NSLog(@"Failed to create WOL socket");
        return;
    }
    NSLog(@"WOL socket created");
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_len = sizeof(addr);
    
    
    for (int i = 0; i < 2; i++) {
        // try all ip addresses
        if (i == 0) {
            inet_aton([host.localAddress UTF8String], &addr.sin_addr);
        } else {
            inet_aton([host.externalAddress UTF8String], &addr.sin_addr);
        }

        CFDataRef dataAddress = CFDataCreate(kCFAllocatorDefault, (unsigned char*)&addr, sizeof(addr));
        
        // try all ports
        for (int j = 0; j < numPorts; j++) {
            addr.sin_port = htons(ports[j]);
            NSLog(@"Sending WOL packet");
            CFSocketSendData(wolSocket, dataAddress, dataPayload, 0);
        }
        CFRelease(dataAddress);
    }
    CFRelease(dataPayload);
}

+ (NSData*) createPayload:(Host*)host {
    NSMutableData* payload = [[NSMutableData alloc] initWithCapacity:102];
    
    // 6 bytes of FF
    UInt8 header = 0xFF;
    for (int i = 0; i < 6; i++) {
        [payload appendBytes:&header length:1];
    }
    
    // 16 repitiions of MAC address
    NSData* macAddress = [self macStringToBytes:host.mac];
    for (int j = 0; j < 16; j++) {
        [payload appendData:macAddress];
    }
    
    return payload;
}

+ (NSData*) macStringToBytes:(NSString*)mac {
    NSString* macString = [mac stringByReplacingOccurrencesOfString:@":" withString:@""];
    NSLog(@"MAC: %@", macString);
    return [Utils hexToBytes:macString];
}

@end
