//
//  ControllerSupport.m
//  Limelight
//
//  Created by Cameron Gutman on 10/20/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "ControllerSupport.h"
#include "Limelight.h"

@import GameController;

@implementation ControllerSupport

// UPDATE_BUTTON(flag, pressed)
#define UPDATE_BUTTON(x, y) (buttonFlags = \
    (y) ? (buttonFlags | (x)) : (buttonFlags & ~(x)))

static NSLock *controllerStreamLock;

+(void) registerControllerCallbacks
{
    for (int i = 0; i < [[GCController controllers] count]; i++) {
        GCController *controller = [GCController controllers][i];
        
        if (controller != NULL) {
            NSLog(@"Controller connected!");
            controller.controllerPausedHandler = ^(GCController *controller) {
                // We call LiSendControllerEvent while holding a lock to prevent
                // multiple simultaneous calls since this function isn't thread safe.
                [controllerStreamLock lock];
                LiSendControllerEvent(PLAY_FLAG, 0, 0, 0, 0, 0, 0);
                
                // Pause for 100 ms
                usleep(100 * 1000);
                
                LiSendControllerEvent(0, 0, 0, 0, 0, 0, 0);
                [controllerStreamLock unlock];
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
                    
                    // We call LiSendControllerEvent while holding a lock to prevent
                    // multiple simultaneous calls since this function isn't thread safe.
                    [controllerStreamLock lock];
                    LiSendControllerEvent(buttonFlags, leftTrigger, rightTrigger,
                                          leftStickX, leftStickY, rightStickX, rightStickY);
                    [controllerStreamLock unlock];
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
                    
                    // We call LiSendControllerEvent while holding a lock to prevent
                    // multiple simultaneous calls since this function isn't thread safe.
                    [controllerStreamLock lock];
                    LiSendControllerEvent(buttonFlags, 0, 0, 0, 0, 0, 0);
                    [controllerStreamLock unlock];
                };
            }
        }
    }

}

-(id) init
{
    self = [super init];
    
    if (controllerStreamLock == NULL) {
        controllerStreamLock = [[NSLock alloc] init];
    }
    
    self.connectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        [ControllerSupport registerControllerCallbacks];
    }];
    self.disconnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        NSLog(@"Controller disconnected!");
    }];
    
    [ControllerSupport registerControllerCallbacks];
    
    return self;
}

-(void) cleanup
{
    [[NSNotificationCenter defaultCenter] removeObserver:self.connectObserver];
    [[NSNotificationCenter defaultCenter] removeObserver:self.disconnectObserver];
}

@end
