//
//  OnScreenControls.h
//  Limelight
//
//  Created by Diego Waxemberg on 12/28/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface OnScreenControls : NSObject

- (id) initWithView:(UIView*)view;
- (void) handleTouchDownEvent:(UIEvent*)event;
- (void) handleTouchUpEvent:(UIEvent*) event;
@end
