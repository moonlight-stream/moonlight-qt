//
//  DiscoveryWorker.m
//  Limelight
//
//  Created by Diego Waxemberg on 1/2/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "DiscoveryWorker.h"
#import "Utils.h"
#import "HttpManager.h"
#import "ServerInfoResponse.h"
#import "HttpRequest.h"

@implementation DiscoveryWorker {
    Host* _host;
    NSString* _uniqueId;
    NSData* _cert;
}

static const float POLL_RATE = 2.0f; // Poll every 2 seconds

- (id) initWithHost:(Host*)host uniqueId:(NSString*)uniqueId cert:(NSData*)cert {
    self = [super init];
    _host = host;
    _uniqueId = uniqueId;
    _cert = cert;
    return self;
}

- (Host*) getHost {
    return _host;
}

- (void)main {
    while (!self.cancelled) {
        [self discoverHost];
        if (!self.cancelled) {
            [NSThread sleepForTimeInterval:POLL_RATE];
        }
    }
}

- (void) discoverHost {
    BOOL receivedResponse = NO;
    if (!self.cancelled && _host.localAddress != nil) {
        HttpManager* hMan = [[HttpManager alloc] initWithHost:_host.localAddress uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        
        ServerInfoResponse* serverInfoResp = [[ServerInfoResponse alloc] init];
        [hMan executeRequestSynchronously:[HttpRequest requestForResponse:serverInfoResp withUrlRequest:[hMan newServerInfoRequest]]];
        if ([serverInfoResp isStatusOk]) {
            [serverInfoResp populateHost:_host];
            _host.address = _host.localAddress;
            receivedResponse = YES;
        }
    }
    if (!self.cancelled && !receivedResponse && _host.externalAddress != nil) {
        HttpManager* hMan = [[HttpManager alloc] initWithHost:_host.externalAddress uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        ServerInfoResponse* serverInfoResp = [[ServerInfoResponse alloc]init];
        [hMan executeRequestSynchronously:[HttpRequest requestForResponse:serverInfoResp withUrlRequest:[hMan newServerInfoRequest]]];
        if ([serverInfoResp isStatusOk]) {
            [serverInfoResp populateHost:_host];
            _host.address = _host.externalAddress;
            receivedResponse = YES;
        }
    }
    
    if (!self.cancelled && !receivedResponse && _host.address != nil) {
        HttpManager* hMan = [[HttpManager alloc] initWithHost:_host.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        ServerInfoResponse* serverInfoResp = [[ServerInfoResponse alloc] init];
        [hMan executeRequestSynchronously:[HttpRequest requestForResponse:serverInfoResp withUrlRequest:[hMan newServerInfoRequest]]];
        if ([serverInfoResp isStatusOk]) {
            [serverInfoResp populateHost:_host];
            receivedResponse = YES;
        }
    }
    _host.online = receivedResponse;
    if (receivedResponse) {
         NSLog(@"Received response from: %@\n{\n\t address:%@ \n\t localAddress:%@ \n\t externalAddress:%@ \n\t uuid:%@ \n\t mac:%@ \n\t pairState:%d \n\t online:%d \n}", _host.name, _host.address, _host.localAddress, _host.externalAddress, _host.uuid, _host.mac, _host.pairState, _host.online);
    } else {
        // If the host is not online, we do not know the pairstate
        _host.pairState = PairStateUnknown;
    }
}

@end
