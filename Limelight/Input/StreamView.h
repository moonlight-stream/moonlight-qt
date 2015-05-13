//
//  StreamView.h
//  Moonlight
//
//  Created by Cameron Gutman on 10/19/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ControllerSupport.h"

@interface StreamView : UIView

- (void) setupOnScreenControls:(ControllerSupport*)controllerSupport;
- (void) setMouseDeltaFactors:(float)x y:(float)y;

@end
