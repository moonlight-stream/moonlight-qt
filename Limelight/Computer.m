//
//  Computer.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "Computer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

@implementation Computer

- (id) initWithHost:(NSNetService *)host {
    self = [super init];
    
    self.hostName = [host hostName];
    self.displayName = [host name];
    
    return self;
}

- (int) resolveHost
{
    struct hostent *hostent;

    if (inet_addr([self.hostName UTF8String]) != INADDR_NONE)
    {
        // Already an IP address
        int addr = inet_addr([self.hostName UTF8String]);
        NSLog(@"host address: %d", addr);
        return addr;
    }
    else
    {
        hostent = gethostbyname([self.hostName UTF8String]);
        if (hostent != NULL)
        {
            char* ipstr = inet_ntoa(*(struct in_addr*)hostent->h_addr_list[0]);
            NSLog(@"Resolved %@ -> %s", self.hostName, ipstr);
            int addr = inet_addr(ipstr);
            NSLog(@"host address: %d", addr);
            return addr;
        }
        else
        {
            NSLog(@"Failed to resolve host: %d", h_errno);
            return -1;
        }
    }
}
@end
