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
    _discoveredHosts = [[NSMutableArray alloc] init];
    _opQueue = [[NSOperationQueue alloc] init];
    _mdnsMan = [[MDNSManager alloc] initWithCallback:self];
    [CryptoManager generateKeyPairUsingSSl];
    _uniqueId = [CryptoManager getUniqueID];
    _cert = [CryptoManager readCertFromFile];
    shouldDiscover = NO;
    return self;
}

- (void) discoverHost:(NSString *)hostAddress withCallback:(void (^)(Host *))callback {
    HttpManager* hMan = [[HttpManager alloc] initWithHost:hostAddress uniqueId:_uniqueId deviceName:deviceName cert:_cert];
    NSData* serverInfoData = [hMan executeRequestSynchronously:[hMan newServerInfoRequest]];
    
    Host* host = nil;
    if ([[HttpManager getStatusStringFromXML:serverInfoData] isEqualToString:@"OK"]) {
        host = [[[DataManager alloc] init] createHost];
        host.address = hostAddress;
        [self updateHost:host withServerInfo:serverInfoData];
        [self addHostToDiscovery:host];
    }
    callback(host);
}

- (void) discoverHost:(Host*)host {
    if (!shouldDiscover) return;
    BOOL receivedResponse = NO;
    if (host.localAddress != nil) {
        HttpManager* hMan = [[HttpManager alloc] initWithHost:host.localAddress uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* serverInfoData = [hMan executeRequestSynchronously:[hMan newServerInfoRequest]];
        if ([[HttpManager getStatusStringFromXML:serverInfoData] isEqualToString:@"OK"]) {
            [self updateHost:host withServerInfo:serverInfoData];
            host.address = host.localAddress;
            receivedResponse = YES;
        }
    }
    if (shouldDiscover && !receivedResponse && host.externalAddress != nil) {
        HttpManager* hMan = [[HttpManager alloc] initWithHost:host.externalAddress uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* serverInfoData = [hMan executeRequestSynchronously:[hMan newServerInfoRequest]];
        if ([[HttpManager getStatusStringFromXML:serverInfoData] isEqualToString:@"OK"]) {
            [self updateHost:host withServerInfo:serverInfoData];
            host.address = host.externalAddress;
            receivedResponse = YES;
        }
    }
    
    if (shouldDiscover && !receivedResponse && host.address != nil) {
        HttpManager* hMan = [[HttpManager alloc] initWithHost:host.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* serverInfoData = [hMan executeRequestSynchronously:[hMan newServerInfoRequest]];
        if ([[HttpManager getStatusStringFromXML:serverInfoData] isEqualToString:@"OK"]) {
            [self updateHost:host withServerInfo:serverInfoData];
            receivedResponse = YES;
        }
    }
    host.online = receivedResponse;
    if (receivedResponse) {
        NSLog(@"Received response from: %@\n{\n\t address:%@ \n\t localAddress:%@ \n\t externalAddress:%@ \n\t uuid:%@ \n\t mac:%@ \n\t pairState:%d \n\t online:%d \n}", host.name, host.address, host.localAddress, host.externalAddress, host.uuid, host.mac, host.pairState, host.online);
    }
    // If the host has already been discovered, we update the reference
    BOOL hostInList = NO;
    for (int i = 0; i < _discoveredHosts.count; i++) {
        Host* discoveredHost = [_discoveredHosts objectAtIndex:i];
        if ([discoveredHost.uuid isEqualToString:host.uuid]) {
            [_discoveredHosts removeObject:discoveredHost];
            [_discoveredHosts insertObject:host atIndex:i];
            hostInList = YES;
        }
    }
    if (!hostInList) {
        [_discoveredHosts addObject:host];
    }
}

- (void) updateHost:(Host*)host withServerInfo:(NSData*)serverInfoData {
    host.name = [HttpManager getStringFromXML:serverInfoData tag:@"hostname"];
    host.externalAddress = [HttpManager getStringFromXML:serverInfoData tag:@"ExternalIP"];
    host.localAddress = [HttpManager getStringFromXML:serverInfoData tag:@"LocalIP"];
    host.uuid = [HttpManager getStringFromXML:serverInfoData tag:@"uniqueid"];
    host.mac = [HttpManager getStringFromXML:serverInfoData tag:@"mac"];
    NSString* pairState = [HttpManager getStringFromXML:serverInfoData tag:@"PairStatus"];
    if ([pairState isEqualToString:@"1"]) {
        host.pairState = PairStatePaired;
    } else {
        host.pairState = PairStateUnpaired;
    }
}

- (void) startDiscovery {
    NSLog(@"Starting discovery");
    shouldDiscover = YES;
    [_mdnsMan searchForHosts];
    [_opQueue addOperation:self];
}

- (void) stopDiscovery {
    shouldDiscover = NO;
    [_mdnsMan stopSearching];
}

- (void) addHostToDiscovery:(Host *)host {
    [_hostQueue addObject:host];
}

- (void) removeHostFromDiscovery:(Host *)host {
    [_hostQueue removeObject:host];
}

- (void)updateHosts:(NSArray *)hosts {
    [_hostQueue addObjectsFromArray:hosts];
}

- (void)main {
    while (shouldDiscover && !self.isCancelled) {
        NSLog(@"Running discovery loop");
        [_discoveredHosts removeAllObjects];
        for (Host* host in _hostQueue) {
            [self discoverHost:host];
        }
        [_callback updateAllHosts:_discoveredHosts];
        [NSThread sleepForTimeInterval:2.0f];
    }
}

@end
