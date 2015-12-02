//
//  ServerInfoResponse.h
//  Moonlight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "HttpResponse.h"
#import "TemporaryHost.h"

#define TAG_HOSTNAME @"hostname"
#define TAG_EXTERNAL_IP @"ExternalIP"
#define TAG_LOCAL_IP @"LocalIP"
#define TAG_UNIQUE_ID @"uniqueid"
#define TAG_MAC_ADDRESS @"mac"
#define TAG_PAIR_STATUS @"PairStatus"

@interface ServerInfoResponse : HttpResponse <Response>

- (void) populateWithData:(NSData *)data;
- (void) populateHost:(TemporaryHost*)host;

@end

