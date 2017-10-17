//
//  ServerInfoResponse.m
//  Moonlight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "ServerInfoResponse.h"
#import <libxml2/libxml/xmlreader.h>

@implementation ServerInfoResponse
@synthesize data, statusCode, statusMessage;

- (void) populateWithData:(NSData *)xml {
    self.data = xml;
    [super parseData];
}

- (void) populateHost:(TemporaryHost*)host {
    host.name = [[self getStringTag:TAG_HOSTNAME] trim];
    host.externalAddress = [[self getStringTag:TAG_EXTERNAL_IP] trim];
    host.localAddress = [[self getStringTag:TAG_LOCAL_IP] trim];
    host.uuid = [[self getStringTag:TAG_UNIQUE_ID] trim];
    host.mac = [[self getStringTag:TAG_MAC_ADDRESS] trim];
    host.currentGame = [[self getStringTag:TAG_CURRENT_GAME] trim];
    
    NSString *state = [[self getStringTag:TAG_STATE] trim];
    if (![state hasSuffix:@"_SERVER_BUSY"]) {
        // GFE 2.8 started keeping currentgame set to the last game played. As a result, it no longer
        // has the semantics that its name would indicate. To contain the effects of this change as much
        // as possible, we'll force the current game to zero if the server isn't in a streaming session.
        host.currentGame = @"0";
    }
    
    NSInteger pairStatus;
    if ([self getIntTag:TAG_PAIR_STATUS value:&pairStatus]) {
        host.pairState = pairStatus ? PairStatePaired : PairStateUnpaired;
    } else {
        host.pairState = PairStateUnknown;
    }
}

@end
