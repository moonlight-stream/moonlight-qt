//
//  ControllerSupport.h
//  Moonlight
//
//  Created by Cameron Gutman on 10/20/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

// Swift
#import "Moonlight-Swift.h"
@class Controller;

@class OnScreenControls;

@interface ControllerSupport : NSObject

-(id) init;
-(void) initAutoOnScreenControlMode:(OnScreenControls*)osc;
-(void) cleanup;

-(void) updateLeftStick:(Controller*)controller x:(short)x y:(short)y;
-(void) updateRightStick:(Controller*)controller x:(short)x y:(short)y;

-(void) updateLeftTrigger:(Controller*)controller left:(char)left;
-(void) updateRightTrigger:(Controller*)controller right:(char)right;
-(void) updateTriggers:(Controller*)controller left:(char)left right:(char)right;

-(void) updateButtonFlags:(Controller*)controller flags:(int)flags;
-(void) setButtonFlag:(Controller*)controller flags:(int)flags;
-(void) clearButtonFlag:(Controller*)controller flags:(int)flags;

-(void) updateFinished:(Controller*)controller;
-(Controller*) getOscController;

@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@end
