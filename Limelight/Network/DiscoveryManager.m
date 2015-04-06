//
//  DiscoveryManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 1/1/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "DiscoveryManager.h"
#import "CryptoManager.h"
#import "HttpManager.h"
#import "Utils.h"
#import "DataManager.h"
#import "DiscoveryWorker.h"
#import "ServerInfoResponse.h"

@implementation DiscoveryManager {
    NSMutableArray* _hostQueue;
    NSMutableArray* _discoveredHosts;
    id<DiscoveryCallback> _callback;
    MDNSManager* _mdnsMan;
    NSOperationQueue* _opQueue;
    NSString* _uniqueId;
    NSData* _cert;
    BOOL shouldDiscover;
}

- (id)initWithHosts:(NSArray *)hosts andCallback:(id<DiscoveryCallback>)callback {
    self = [super init];
    _hostQueue = [NSMutableArray arrayWithArray:hosts];
    _callback = callback;
    _opQueue = [[NSOperationQueue alloc] init];
    _mdnsMan = [[MDNSManager alloc] initWithCallback:self];
    [CryptoManager generateKeyPairUsingSSl];
    _uniqueId = [CryptoManager getUniqueID];
    _cert = [CryptoManager readCertFromFile];
    shouldDiscover = NO;
    return self;
}

- (void) discoverHost:(NSString *)hostAddress withCallback:(void (^)(Host *, NSString*))callback {
    HttpManager* hMan = [[HttpManager alloc] initWithHost:hostAddress uniqueId:_uniqueId deviceName:deviceName cert:_cert];
    ServerInfoResponse* serverInfoResponse = [[ServerInfoResponse alloc] init];
    [hMan executeRequestSynchronously:[HttpRequest requestForResponse:serverInfoResponse withUrlRequest:[hMan newServerInfoRequest]]];
    
    Host* host = nil;
    if ([serverInfoResponse isStatusOk]) {
        DataManager* dataMan = [[DataManager alloc] init];
        host = [dataMan createHost];
        host.address = hostAddress;
        host.online = YES;
        [serverInfoResponse populateHost:host];
        if (![self addHostToDiscovery:host]) {
            [dataMan removeHost:host];
            callback(nil, @"Host already added");
        } else {
            callback(host, nil);
        }
    } else {
        callback(nil, @"Could not connect to host");
    }
    
}

- (void) startDiscovery {
    Log(LOG_I, @"Starting discovery");
    shouldDiscover = YES;
    [_mdnsMan searchForHosts];
    for (Host* host in _hostQueue) {
        [_opQueue addOperation:[self createWorkerForHost:host]];
    }
}

- (void) stopDiscovery {
    Log(LOG_I, @"Stopping discovery");
    shouldDiscover = NO;
    [_mdnsMan stopSearching];
    [_opQueue cancelAllOperations];
}

- (void) stopDiscoveryBlocking {
    Log(LOG_I, @"Stopping discovery and waiting for workers to stop");
    shouldDiscover = NO;
    [_mdnsMan stopSearching];
    [_opQueue cancelAllOperations];
    [_opQueue waitUntilAllOperationsAreFinished];
    Log(LOG_I, @"All discovery workers stopped");
}

- (BOOL) addHostToDiscovery:(Host *)host {
    if (host.uuid.length > 0 && ![self isHostInDiscovery:host]) {
        [_hostQueue addObject:host];
        if (shouldDiscover) {
            [_opQueue addOperation:[self createWorkerForHost:host]];
        }
        return YES;
    }
    return NO;
}

- (void) removeHostFromDiscovery:(Host *)host {
    for (DiscoveryWorker* worker in [_opQueue operations]) {
        if ([worker getHost] == host) {
            [worker cancel];
        }
    }
    [_hostQueue removeObject:host];
}

// Override from MDNSCallback
- (void)updateHosts:(NSArray *)hosts {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        DataManager* dataMan = [[DataManager alloc] init];
        // Discover the hosts before adding to eliminate duplicates
        for (Host* host in hosts) {
            Log(LOG_I, @"Found host through MDNS: %@:", host.name);
            // Since this is on a background thread, we do not need to use the opQueue
            DiscoveryWorker* worker = (DiscoveryWorker*)[self createWorkerForHost:host];
            [worker discoverHost];
            if ([self addHostToDiscovery:host]) {
                Log(LOG_I, @"Adding host to discovery: %@", host.name);
                [_callback updateAllHosts:_hostQueue];
            } else {
                Log(LOG_I, @"Not adding host to discovery: %@", host.name);
                [dataMan removeHost:host];
            }
        }
        [dataMan saveHosts];
    });
}

- (BOOL) isHostInDiscovery:(Host*)host {
    for (int i = 0; i < _hostQueue.count; i++) {
        Host* discoveredHost = [_hostQueue objectAtIndex:i];
        if (discoveredHost.uuid.length > 0 && [discoveredHost.uuid isEqualToString:host.uuid]) {
            return YES;
        }
    }
    return NO;
}

- (NSOperation*) createWorkerForHost:(Host*)host {
    DiscoveryWorker* worker = [[DiscoveryWorker alloc] initWithHost:host uniqueId:_uniqueId cert:_cert];
    return worker;
}

@end
