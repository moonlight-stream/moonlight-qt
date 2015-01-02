//
//  Utils.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/20/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "Utils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

@implementation Utils
NSString *const deviceName = @"roth";

+ (NSData*) randomBytes:(NSInteger)length {
    char* bytes = malloc(length);
    arc4random_buf(bytes, length);
    NSData* randomData = [NSData dataWithBytes:bytes length:length];
    free(bytes);
    return randomData;
}

+ (NSData*) hexToBytes:(NSString*) hex {
    unsigned long len = [hex length];
    NSMutableData* data = [NSMutableData dataWithCapacity:len / 2];
    char byteChars[3] = {'\0','\0','\0'};
    unsigned long wholeByte;
    
    const char *chars = [hex UTF8String];
    int i = 0;
    while (i < len) {
        byteChars[0] = chars[i++];
        byteChars[1] = chars[i++];
        wholeByte = strtoul(byteChars, NULL, 16);
        [data appendBytes:&wholeByte length:1];
    }
    
    return data;
}

+ (NSString*) bytesToHex:(NSData*)data {
    const unsigned char* bytes = [data bytes];
    NSMutableString *hex = [[NSMutableString alloc] init];
    for (int i = 0; i < [data length]; i++) {
        [hex appendFormat:@"%02X" , bytes[i]];
    }
    return hex;
}

+ (int) resolveHost:(NSString*)host {
    struct hostent *hostent;
    
    if (inet_addr([host UTF8String]) != INADDR_NONE) {
        // Already an IP address
        int addr = inet_addr([host UTF8String]);
        NSLog(@"host address: %d", addr);
        return addr;
    } else {
        hostent = gethostbyname([host UTF8String]);
        if (hostent != NULL) {
            char* ipstr = inet_ntoa(*(struct in_addr*)hostent->h_addr_list[0]);
            NSLog(@"Resolved %@ -> %s", host, ipstr);
            int addr = inet_addr(ipstr);
            NSLog(@"host address: %d", addr);
            return addr;
        } else {
            NSLog(@"Failed to resolve host: %d", h_errno);
            return 0;
        }
    }
}


@end
