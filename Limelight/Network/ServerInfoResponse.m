//
//  ServerInfoResponse.m
//  Limelight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "ServerInfoResponse.h"
#import <libxml2/libxml/xmlreader.h>

@implementation ServerInfoResponse
@synthesize data, statusCode, statusMessage;

static NSString* TAG_HOSTNAME = @"hostname";
static NSString* TAG_EXTERNAL_IP = @"ExternalIP";
static NSString* TAG_LOCAL_IP = @"LocalIP";
static NSString* TAG_UNIQUE_ID = @"uniqueid";
static NSString* TAG_MAC_ADDRESS = @"mac";
static NSString* TAG_PAIR_STATUS = @"PairStatus";

- (void) populateWithData:(NSData *)xml {
    self.data = xml;
    [super parseData];
}

- (void) populateHost:(Host*)host {
    NSString* hostname = [self getStringTag:TAG_HOSTNAME];
    NSString* externalIp = [self getStringTag:TAG_EXTERNAL_IP];
    NSString* localIp = [self getStringTag:TAG_LOCAL_IP];
    NSString* uniqueId = [self getStringTag:TAG_UNIQUE_ID];
    NSString* macAddress = [self getStringTag:TAG_MAC_ADDRESS];
    NSString* pairStatus = [self getStringTag:TAG_PAIR_STATUS];
    
    if (hostname) {
        host.name = hostname;
    }
    if (externalIp) {
        host.externalAddress = externalIp;
    }
    if (localIp) {
        host.localAddress = localIp;
    }
    if (uniqueId) {
        host.uuid = uniqueId;
    }
    if (macAddress) {
        host.mac = macAddress;
    }
    if (pairStatus) {
        host.pairState = [pairStatus isEqualToString:@"1"] ? PairStatePaired : PairStateUnpaired;
    }
}

@end
