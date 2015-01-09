//
//  ControllerSupport.h
//  Limelight
//
//  Created by Cameron Gutman on 10/20/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@class OnScreenControls;

@interface ControllerSupport : NSObject

-(id) init;
-(void) initAutoOnScreenControlMode:(OnScreenControls*)osc;
-(void) cleanup;

-(void) updateLeftStick:(short)x y:(short)y;
-(void) updateRightStick:(short)x y:(short)y;

-(void) updateLeftTrigger:(char)left;
-(void) updateRightTrigger:(char)right;
-(void) updateTriggers:(char)left right:(char)right;

-(void) updateButtonFlags:(int)flags;
-(void) setButtonFlag:(int)flags;
-(void) clearButtonFlag:(int)flags;

-(void) updateFinished;

@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@end
