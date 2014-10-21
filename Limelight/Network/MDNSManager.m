//
//  MDNSManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "MDNSManager.h"
#import "Computer.h"

@implementation MDNSManager {
    NSNetServiceBrowser* mDNSBrowser;
    NSMutableArray* domains;
    NSMutableArray* services;
    BOOL scanActive;
}

static NSString* NV_SERVICE_TYPE = @"_nvstream._tcp";

- (id) initWithCallback:(id<MDNSCallback>)callback {
    self = [super init];
    
    self.callback = callback;
    
    mDNSBrowser = [[NSNetServiceBrowser alloc] init];
    [mDNSBrowser setDelegate:self];
    
    domains = [[NSMutableArray alloc] init];
    services = [[NSMutableArray alloc] init];
    
    return self;
}

- (void) searchForHosts {
    NSLog(@"Starting mDNS discovery");
    scanActive = TRUE;
    [mDNSBrowser searchForServicesOfType:NV_SERVICE_TYPE inDomain:@""];
}

- (void) stopSearching {
    NSLog(@"Stopping mDNS discovery");
    scanActive = FALSE;
    [mDNSBrowser stop];
}

- (NSArray*) getFoundHosts {
    NSMutableArray* hosts = [[NSMutableArray alloc] init];
    for (NSNetService* service in services) {
        if (service.hostName != nil) {
            [hosts addObject:[[Computer alloc] initWithHost:service]];
        }
    }
    return hosts;
}

- (void)netServiceDidResolveAddress:(NSNetService *)service {
    NSLog(@"Resolved address: %@ -> %@", service, service.hostName);
    [self.callback updateHosts:[self getFoundHosts]];
}

- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict {
    NSLog(@"Did not resolve address for: %@\n%@", sender, [errorDict description]);
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindService:(NSNetService *)aNetService moreComing:(BOOL)moreComing {
    NSLog(@"Found service: %@", aNetService);
    [aNetService setDelegate:self];
    [aNetService resolveWithTimeout:5];
    [services addObject:aNetService];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didRemoveService:(NSNetService *)aNetService moreComing:(BOOL)moreComing {
    NSLog(@"Removing service: %@", aNetService);
    [services removeObject:aNetService];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didNotSearch:(NSDictionary *)errorDict {
    NSLog(@"Did not perform search");
    NSLog(@"%@", [errorDict description]);
    
    // Schedule a retry in 2 seconds
    [NSTimer scheduledTimerWithTimeInterval:2.0
                                     target:self
                                   selector:@selector(retryTimerCallback:)
                                   userInfo:nil
                                    repeats:NO];
}

- (void)retryTimerCallback:(NSTimer *)timer {
    // Check if we've been stopped since this was queued
    if (!scanActive) {
        return;
    }
    
    NSLog(@"Retrying mDNS search");
    [mDNSBrowser stop];
    [mDNSBrowser searchForServicesOfType:NV_SERVICE_TYPE inDomain:@""];
}

@end