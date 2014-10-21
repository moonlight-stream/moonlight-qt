//
//  MDNSManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "MDNSManager.h"
#import "Computer.h"

@implementation MDNSManager : NSObject
NSNetServiceBrowser* mDNSBrowser;
NSMutableArray* domains;
NSMutableArray* services;

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
    NSLog(@"Searching for hosts...");
    [mDNSBrowser searchForServicesOfType:NV_SERVICE_TYPE inDomain:@""];
}

- (NSArray*) getFoundHosts {
    NSMutableArray* hosts = [[NSMutableArray alloc] init];
    for (NSNetService* service in services) {
        if (service.hostName != nil) {
            [hosts addObject:[[Computer alloc] initWithHost:service]];
        }
    }
    return [[NSArray alloc] initWithArray:hosts];
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
}

@end