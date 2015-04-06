//
//  CleanupCrew.m
//  Limelight
//
//  Created by Diego Waxemberg on 4/5/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "CleanupCrew.h"

@implementation CleanupCrew {
    NSArray* _hostList;
    id<CleanupCallback> _callback;
}

static const int CLEANUP_RATE = 2;


- (id) initWithHostList:(NSArray *)hostList andCallback:(id<CleanupCallback>)callback {
    self = [super init];
    _hostList = hostList;
    _callback = callback;
    return self;
}

- (void) main {
    while (!self.isCancelled) {
        NSMutableSet* dirtyHosts = [[NSMutableSet alloc] init];
        
        for (Host* host in _hostList) {
            for (Host* other in _hostList) {
                if (self.isCancelled) {
                    break;
                }
                
                // We check the name as well as a hack to remove duplicates
                if (host != other && ([host.uuid isEqualToString:other.uuid]
                                      || [host.name isEqualToString:other.name]))  {
                    [dirtyHosts addObject:other];
                }
            }
        }
        
        if (dirtyHosts.count > 0) {
            [_callback cleanedUpHosts:dirtyHosts];
        }
        
        if (!self.cancelled) {
            [NSThread sleepForTimeInterval:CLEANUP_RATE];
        }
    }
}

@end
