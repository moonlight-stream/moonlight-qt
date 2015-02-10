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

- (void) populateWithData:(NSData *)xml {
    self.data = xml;
    [super parseData];
}

- (void) populateHost:(Host*)host {
    host.name = [self getStringTag:TAG_HOSTNAME];
    host.externalAddress = [self getStringTag:TAG_EXTERNAL_IP];
    host.localAddress = [self getStringTag:TAG_LOCAL_IP];
    host.uuid = [self getStringTag:TAG_UNIQUE_ID];
    host.mac = [self getStringTag:TAG_MAC_ADDRESS];
    
    NSInteger pairStatus;
    if ([self getIntTag:TAG_PAIR_STATUS value:&pairStatus]) {
        host.pairState = pairStatus ? PairStatePaired : PairStateUnpaired;
    } else {
        host.pairState = PairStateUnknown;
    }
}

@end
