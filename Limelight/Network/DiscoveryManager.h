//
//  DiscoveryManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 1/1/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "MDNSManager.h"
#import "Host.h"

@protocol DiscoveryCallback <NSObject>

- (void) updateAllHosts:(NSArray*)hosts;

@end

@interface DiscoveryManager : NSObject <MDNSCallback>

- (id) initWithHosts:(NSArray*)hosts andCallback:(id<DiscoveryCallback>) callback;
- (void) startDiscovery;
- (void) stopDiscovery;
- (void) stopDiscoveryBlocking;
- (BOOL) addHostToDiscovery:(Host*)host;
- (void) removeHostFromDiscovery:(Host*)host;
- (void) discoverHost:(NSString*)hostAddress withCallback:(void (^)(Host*, NSString*))callback;

@end
