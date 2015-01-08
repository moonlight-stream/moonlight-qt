//
//  DiscoveryWorker.h
//  Limelight
//
//  Created by Diego Waxemberg on 1/2/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Host.h"

@interface DiscoveryWorker : NSOperation

- (id) initWithHost:(Host*)host uniqueId:(NSString*)uniqueId cert:(NSData*)cert;
- (void) discoverHost;
- (Host*) getHost;

+ (void) updateHost:(Host*)host withServerInfo:(NSData*)serverInfoData;

@end
