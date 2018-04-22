//
//  DiscoveryManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 1/1/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "MDNSManager.h"
#import "TemporaryHost.h"

@protocol DiscoveryCallback <NSObject>

- (void) updateAllHosts:(NSArray*)hosts;

@end

@interface DiscoveryManager : NSObject <MDNSCallback>

- (id) initWithHosts:(NSArray*)hosts andCallback:(id<DiscoveryCallback>) callback;
- (void) startDiscovery;
- (void) stopDiscovery;
- (void) stopDiscoveryBlocking;
- (BOOL) addHostToDiscovery:(TemporaryHost*)host;
- (void) removeHostFromDiscovery:(TemporaryHost*)host;
- (void) discoverHost:(NSString*)hostAddress withCallback:(void (^)(TemporaryHost*, NSString*))callback;

@end
