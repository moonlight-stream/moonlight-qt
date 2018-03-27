//
//  StreamManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/20/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "StreamConfiguration.h"
#import "Connection.h"

@interface StreamManager : NSOperation

#if TARGET_OS_IPHONE
- (id) initWithConfig:(StreamConfiguration*)config renderView:(UIView*)view connectionCallbacks:(id<ConnectionCallbacks>)callback;
#else
- (id) initWithConfig:(StreamConfiguration*)config renderView:(NSView*)view connectionCallbacks:(id<ConnectionCallbacks>)callback;
#endif

- (void) stopStream;

@end
