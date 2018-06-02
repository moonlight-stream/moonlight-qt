//
//  ControllerSupport.h
//  Moonlight
//
//  Created by Cameron Gutman on 10/20/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

// Swift
#import "Moonlight-Swift.h"
#if !TARGET_OS_IPHONE
#import "Gamepad.h"
#endif
#import "StreamConfiguration.h"
@class Controller;

@class OnScreenControls;

@interface ControllerSupport : NSObject

-(id) initWithConfig:(StreamConfiguration*)streamConfig;

#if TARGET_OS_IPHONE
-(void) initAutoOnScreenControlMode:(OnScreenControls*)osc;
-(void) cleanup;
-(Controller*) getOscController;
#else
-(void) assignGamepad:(struct Gamepad_device *)gamepad;
-(void) removeGamepad:(struct Gamepad_device *)gamepad;
-(NSMutableDictionary*) getControllers;
#endif

-(void) updateLeftStick:(Controller*)controller x:(short)x y:(short)y;
-(void) updateRightStick:(Controller*)controller x:(short)x y:(short)y;

-(void) updateLeftTrigger:(Controller*)controller left:(char)left;
-(void) updateRightTrigger:(Controller*)controller right:(char)right;
-(void) updateTriggers:(Controller*)controller left:(char)left right:(char)right;

-(void) updateButtonFlags:(Controller*)controller flags:(int)flags;
-(void) setButtonFlag:(Controller*)controller flags:(int)flags;
-(void) clearButtonFlag:(Controller*)controller flags:(int)flags;

-(void) updateFinished:(Controller*)controller;

+(int) getConnectedGamepadMask:(StreamConfiguration*)streamConfig;

@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@end
