//
//  ControllerSupport.m
//  Limelight
//
//  Created by Cameron Gutman on 10/20/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "ControllerSupport.h"
#import "OnScreenControls.h"
#include "Limelight.h"

@import GameController;

@implementation ControllerSupport {
    NSLock *_controllerValueLock;
    NSLock *_controllerStreamLock;
    
    OnScreenControls *_osc;
    
    int _lastButtonFlags;
    char _lastLeftTrigger, _lastRightTrigger;
    short _lastLeftStickX, _lastLeftStickY;
    short _lastRightStickX, _lastRightStickY;
    
#define EMULATING_SELECT     0x1
#define EMULATING_SPECIAL    0x2
    int _emulatingButtonFlags;
}

// UPDATE_BUTTON(flag, pressed)
#define UPDATE_BUTTON(x, y) (buttonFlags = \
    (y) ? (buttonFlags | (x)) : (buttonFlags & ~(x)))

-(void) updateLeftStick:(short)x y:(short)y
{
    [_controllerValueLock lock];
    _lastLeftStickX = x;
    _lastLeftStickY = y;
    [_controllerValueLock unlock];
}

-(void) updateRightStick:(short)x y:(short)y
{
    [_controllerValueLock lock];
    _lastRightStickX = x;
    _lastRightStickY = y;
    [_controllerValueLock unlock];
}

-(void) updateLeftTrigger:(char)left
{
    [_controllerValueLock lock];
    _lastLeftTrigger = left;
    [_controllerValueLock unlock];
}

-(void) updateRightTrigger:(char)right
{
    [_controllerValueLock lock];
    _lastRightTrigger = right;
    [_controllerValueLock unlock];
}

-(void) updateTriggers:(char)left right:(char)right
{
    [_controllerValueLock lock];
    _lastLeftTrigger = left;
    _lastRightTrigger = right;
    [_controllerValueLock unlock];
}

-(void) handleSpecialCombosReleased:(int)releasedButtons
{
    if (releasedButtons & PLAY_FLAG) {
        if ((_emulatingButtonFlags & EMULATING_SELECT) &&
            (releasedButtons & LB_FLAG)) {
            _lastButtonFlags &= ~BACK_FLAG;
            _emulatingButtonFlags &= ~EMULATING_SELECT;
        }
        if ((_emulatingButtonFlags & EMULATING_SPECIAL) &&
            (releasedButtons & RB_FLAG)) {
            _lastButtonFlags &= ~SPECIAL_FLAG;
            _emulatingButtonFlags &= ~EMULATING_SPECIAL;
        }
    }
}

-(void) handleSpecialCombosPressed
{
    // Special button combos for select and special
    if (_lastButtonFlags & PLAY_FLAG) {
        // If LB and start are down, trigger select
        if (_lastButtonFlags & LB_FLAG) {
            _lastButtonFlags |= BACK_FLAG;
            _lastButtonFlags &= ~(PLAY_FLAG | LB_FLAG);
            _emulatingButtonFlags |= EMULATING_SELECT;
        }
        // If RB and start are down, trigger special
        else if (_lastButtonFlags & RB_FLAG) {
            _lastButtonFlags |= SPECIAL_FLAG;
            _lastButtonFlags &= ~(PLAY_FLAG | RB_FLAG);
            _emulatingButtonFlags |= EMULATING_SPECIAL;
        }
    }
}

-(void) updateButtonFlags:(int)flags
{
    [_controllerValueLock lock];
    int releasedButtons = (_lastButtonFlags ^ flags) & ~flags;
    
    _lastButtonFlags = flags;
    
    // This must be called before handleSpecialCombosPressed
    // because we clear the original button flags there
    [self handleSpecialCombosReleased: releasedButtons];
    
    [self handleSpecialCombosPressed];
    
    [_controllerValueLock unlock];
}

-(void) setButtonFlag:(int)flags
{
    [_controllerValueLock lock];
    _lastButtonFlags |= flags;
    [self handleSpecialCombosPressed];
    [_controllerValueLock unlock];
}

-(void) clearButtonFlag:(int)flags
{
    [_controllerValueLock lock];
    _lastButtonFlags &= ~flags;
    [self handleSpecialCombosReleased: flags];
    [_controllerValueLock unlock];
}

-(void) updateFinished
{
    [_controllerStreamLock lock];
    [_controllerValueLock lock];
    
    LiSendControllerEvent(_lastButtonFlags, _lastLeftTrigger, _lastRightTrigger, _lastLeftStickX, _lastLeftStickY, _lastRightStickX, _lastRightStickY);
    
    [_controllerValueLock unlock];
    [_controllerStreamLock unlock];
}

-(void) unregisterControllerCallbacks
{
    for (int i = 0; i < [[GCController controllers] count]; i++) {
        GCController *controller = [GCController controllers][i];
        
        if (controller != NULL) {
            controller.controllerPausedHandler = NULL;
            
            if (controller.extendedGamepad != NULL) {
                controller.extendedGamepad.valueChangedHandler = NULL;
            }
            else if (controller.gamepad != NULL) {
                controller.gamepad.valueChangedHandler = NULL;
            }
        }
    }
}

-(void) registerControllerCallbacks
{
    for (int i = 0; i < [[GCController controllers] count]; i++) {
        GCController *controller = [GCController controllers][i];
        
        if (controller != NULL) {
            NSLog(@"Controller connected!");
            controller.controllerPausedHandler = ^(GCController *controller) {
                [self setButtonFlag:PLAY_FLAG];
                [self updateFinished];
                
                // Pause for 100 ms
                usleep(100 * 1000);
                
                [self clearButtonFlag:PLAY_FLAG];
                [self updateFinished];
            };
            
            if (controller.extendedGamepad != NULL) {
                controller.extendedGamepad.valueChangedHandler = ^(GCExtendedGamepad *gamepad, GCControllerElement *element) {
                    short buttonFlags = 0;
                    short leftStickX, leftStickY;
                    short rightStickX, rightStickY;
                    char leftTrigger, rightTrigger;
                    
                    UPDATE_BUTTON(A_FLAG, gamepad.buttonA.pressed);
                    UPDATE_BUTTON(B_FLAG, gamepad.buttonB.pressed);
                    UPDATE_BUTTON(X_FLAG, gamepad.buttonX.pressed);
                    UPDATE_BUTTON(Y_FLAG, gamepad.buttonY.pressed);
                    
                    UPDATE_BUTTON(UP_FLAG, gamepad.dpad.up.pressed);
                    UPDATE_BUTTON(DOWN_FLAG, gamepad.dpad.down.pressed);
                    UPDATE_BUTTON(LEFT_FLAG, gamepad.dpad.left.pressed);
                    UPDATE_BUTTON(RIGHT_FLAG, gamepad.dpad.right.pressed);
                    
                    UPDATE_BUTTON(LB_FLAG, gamepad.leftShoulder.pressed);
                    UPDATE_BUTTON(RB_FLAG, gamepad.rightShoulder.pressed);
                    
                    leftStickX = gamepad.leftThumbstick.xAxis.value * 0x7FFE;
                    leftStickY = gamepad.leftThumbstick.yAxis.value * 0x7FFE;
                    
                    rightStickX = gamepad.rightThumbstick.xAxis.value * 0x7FFE;
                    rightStickY = gamepad.rightThumbstick.yAxis.value * 0x7FFE;
                    
                    leftTrigger = gamepad.leftTrigger.value * 0xFF;
                    rightTrigger = gamepad.rightTrigger.value * 0xFF;
                    
                    [self updateButtonFlags:buttonFlags];
                    [self updateLeftStick:leftStickX y:leftStickY];
                    [self updateRightStick:rightStickX y:rightStickY];
                    [self updateTriggers:leftTrigger right:rightTrigger];
                    [self updateFinished];
                };
            }
            else if (controller.gamepad != NULL) {
                controller.gamepad.valueChangedHandler = ^(GCGamepad *gamepad, GCControllerElement *element) {
                    short buttonFlags = 0;
                    
                    UPDATE_BUTTON(A_FLAG, gamepad.buttonA.pressed);
                    UPDATE_BUTTON(B_FLAG, gamepad.buttonB.pressed);
                    UPDATE_BUTTON(X_FLAG, gamepad.buttonX.pressed);
                    UPDATE_BUTTON(Y_FLAG, gamepad.buttonY.pressed);
                    
                    UPDATE_BUTTON(UP_FLAG, gamepad.dpad.up.pressed);
                    UPDATE_BUTTON(DOWN_FLAG, gamepad.dpad.down.pressed);
                    UPDATE_BUTTON(LEFT_FLAG, gamepad.dpad.left.pressed);
                    UPDATE_BUTTON(RIGHT_FLAG, gamepad.dpad.right.pressed);
                    
                    UPDATE_BUTTON(LB_FLAG, gamepad.leftShoulder.pressed);
                    UPDATE_BUTTON(RB_FLAG, gamepad.rightShoulder.pressed);
                    
                    [self updateButtonFlags:buttonFlags];
                    [self updateFinished];
                };
            }
        }
    }

}

-(void) updateAutoOnScreenControlMode
{
    // Auto on-screen control support may not be enabled
    if (_osc == NULL) {
        return;
    }
    
    OnScreenControlsLevel level = OnScreenControlsLevelFull;
    
    // We currently stop after the first controller we find.
    // Maybe we'll want to change that logic later.
    for (int i = 0; i < [[GCController controllers] count]; i++) {
        GCController *controller = [GCController controllers][i];
        
        if (controller != NULL) {
            if (controller.extendedGamepad != NULL) {
                level = OnScreenControlsLevelAutoGCExtendedGamepad;
                break;
            }
            else if (controller.gamepad != NULL) {
                level = OnScreenControlsLevelAutoGCGamepad;
                break;
            }
        }
    }
    
    [_osc setLevel:level];
}

-(void) initAutoOnScreenControlMode:(OnScreenControls*)osc
{
    _osc = osc;
    
    [self updateAutoOnScreenControlMode];
}

-(id) init
{
    self = [super init];
    
    _controllerStreamLock = [[NSLock alloc] init];
    _controllerValueLock = [[NSLock alloc] init];
    
    self.connectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        // Register callbacks on the new controller
        [self registerControllerCallbacks];
        
        // Re-evaluate the on-screen control mode
        [self updateAutoOnScreenControlMode];
    }];
    self.disconnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        NSLog(@"Controller disconnected!");
        
        // Reset all controller state to be safe
        [self updateButtonFlags:0];
        [self updateLeftStick:0 y:0];
        [self updateRightStick:0 y:0];
        [self updateTriggers:0 right:0];
        [self updateFinished];
        
        // Re-evaluate the on-screen control mode
        [self updateAutoOnScreenControlMode];
    }];
    
    // Register for controller callbacks on any existing controllers
    [self registerControllerCallbacks];
    
    return self;
}

-(void) cleanup
{
    [[NSNotificationCenter defaultCenter] removeObserver:self.connectObserver];
    [[NSNotificationCenter defaultCenter] removeObserver:self.disconnectObserver];
    
    [self unregisterControllerCallbacks];
}

@end
