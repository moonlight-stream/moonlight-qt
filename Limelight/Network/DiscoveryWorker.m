//
//  DiscoveryWorker.m
//  Moonlight
//
//  Created by Diego Waxemberg on 1/2/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
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
        ServerInfoResponse* serverInfoResp = [self requestInfoAtAddress:_host.localAddress];
        receivedResponse = [self checkResponse:serverInfoResp];
    }
    if (!self.cancelled && !receivedResponse && _host.externalAddress != nil) {
        ServerInfoResponse* serverInfoResp = [self requestInfoAtAddress:_host.externalAddress];
        receivedResponse = [self checkResponse:serverInfoResp];
    }
    if (!self.cancelled && !receivedResponse && _host.address != nil) {
        ServerInfoResponse* serverInfoResp = [self requestInfoAtAddress:_host.address];
        receivedResponse = [self checkResponse:serverInfoResp];
    }
    _host.online = receivedResponse;
    if (receivedResponse) {
        Log(LOG_D, @"Received response from: %@\n{\n\t address:%@ \n\t localAddress:%@ \n\t externalAddress:%@ \n\t uuid:%@ \n\t mac:%@ \n\t pairState:%d \n\t online:%d \n}", _host.name, _host.address, _host.localAddress, _host.externalAddress, _host.uuid, _host.mac, _host.pairState, _host.online);
    }
}

- (ServerInfoResponse*) requestInfoAtAddress:(NSString*)address {
    HttpManager* hMan = [[HttpManager alloc] initWithHost:address
                                                 uniqueId:_uniqueId
                                               deviceName:deviceName
                                                     cert:_cert];
    ServerInfoResponse* response = [[ServerInfoResponse alloc] init];
    [hMan executeRequestSynchronously:[HttpRequest requestForResponse:response
                                                       withUrlRequest:[hMan newServerInfoRequest]]];
    return response;
}

- (BOOL) checkResponse:(ServerInfoResponse*)response {
    if ([response isStatusOk]) {
        // If the response is from a different host then do not update this host
        if ((_host.uuid == nil || [[response getStringTag:TAG_UNIQUE_ID] isEqualToString:_host.uuid])) {
            [response populateHost:_host];
            return YES;
        } else {
            Log(LOG_I, @"Received response from incorrect host: %@ expected: %@", [response getStringTag:TAG_UNIQUE_ID], _host.uuid);
        }
    }
    return NO;
}

@end
