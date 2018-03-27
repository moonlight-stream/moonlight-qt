//
//  StreamView.h
//  Moonlight
//
//  Created by Cameron Gutman on 10/19/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "ControllerSupport.h"

@protocol EdgeDetectionDelegate <NSObject>

- (void) edgeSwiped;

@end

@interface StreamView : OSView

- (void) setupOnScreenControls:(ControllerSupport*)controllerSupport swipeDelegate:(id<EdgeDetectionDelegate>)swipeDelegate;
- (void) setMouseDeltaFactors:(float)x y:(float)y;

@end
