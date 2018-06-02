//
//  MDNSManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "MDNSManager.h"
#import "TemporaryHost.h"

@implementation MDNSManager {
    NSNetServiceBrowser* mDNSBrowser;
    NSMutableArray* services;
    BOOL scanActive;
    BOOL timerPending;
}

static NSString* NV_SERVICE_TYPE = @"_nvstream._tcp";

- (id) initWithCallback:(id<MDNSCallback>)callback {
    self = [super init];
    
    self.callback = callback;
    
    scanActive = FALSE;
    
    mDNSBrowser = [[NSNetServiceBrowser alloc] init];
    [mDNSBrowser setDelegate:self];
    
    services = [[NSMutableArray alloc] init];
    
    return self;
}

- (void) searchForHosts {
    if (scanActive) {
        return;
    }
    
    Log(LOG_I, @"Starting mDNS discovery");
    scanActive = TRUE;

    if (!timerPending) {
        timerPending = TRUE;

        // Just invoke the timer callback to save a little code
        [self startSearchTimerCallback:nil];
    }
}

- (void) stopSearching {
    if (!scanActive) {
        return;
    }
    
    Log(LOG_I, @"Stopping mDNS discovery");
    scanActive = FALSE;
    [mDNSBrowser stop];
}

- (void)netServiceDidResolveAddress:(NSNetService *)service {
    Log(LOG_D, @"Resolved address: %@ -> %@", service, service.hostName);
    TemporaryHost* host = [[TemporaryHost alloc] init];
    host.activeAddress = host.localAddress = service.hostName;
    host.name = service.hostName;
    [self.callback updateHost:host];
}

- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict {
    Log(LOG_W, @"Did not resolve address for: %@\n%@", sender, [errorDict description]);
    
    // Schedule a retry in 2 seconds
    [NSTimer scheduledTimerWithTimeInterval:2.0
                                     target:self
                                   selector:@selector(retryResolveTimerCallback:)
                                   userInfo:nil
                                    repeats:NO];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindService:(NSNetService *)aNetService moreComing:(BOOL)moreComing {
    Log(LOG_D, @"Found service: %@", aNetService);
    
    if (![services containsObject:aNetService]) {
        Log(LOG_I, @"Found new host: %@", aNetService.name);
        [aNetService setDelegate:self];
        [aNetService resolveWithTimeout:5];
        [services addObject:aNetService];
    }
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didRemoveService:(NSNetService *)aNetService moreComing:(BOOL)moreComing {
    Log(LOG_I, @"Removing service: %@", aNetService);
    [services removeObject:aNetService];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didNotSearch:(NSDictionary *)errorDict {
    Log(LOG_W, @"Did not perform search: \n%@", [errorDict description]);
    
    // We'll schedule a retry in startSearchTimerCallback
}

- (void)startSearchTimerCallback:(NSTimer *)timer {
    // Check if we've been stopped since this was queued
    if (!scanActive) {
        timerPending = FALSE;
        return;
    }
    
    Log(LOG_D, @"Restarting mDNS search");
    [mDNSBrowser stop];
    [mDNSBrowser searchForServicesOfType:NV_SERVICE_TYPE inDomain:@""];
    
    // Search again in 5 seconds. We need to do this because
    // we want more aggressive querying than Bonjour will normally
    // do for when we're at the hosts screen. This also covers scenarios
    // where discovery didn't work, like if WiFi was disabled.
    [NSTimer scheduledTimerWithTimeInterval:5.0
                                     target:self
                                   selector:@selector(startSearchTimerCallback:)
                                   userInfo:nil
                                    repeats:NO];
}

- (void)retryResolveTimerCallback:(NSTimer *)timer {
    // Check if we've been stopped since this was queued
    if (!scanActive) {
        return;
    }
    
    Log(LOG_I, @"Retrying mDNS resolution");
    for (NSNetService* service in services) {
        if (service.hostName == nil) {
            [service setDelegate:self];
            [service resolveWithTimeout:5];
        }
    }
}

@end
