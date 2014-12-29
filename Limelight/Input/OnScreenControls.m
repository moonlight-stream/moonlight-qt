//
//  OnScreenControls.m
//  Limelight
//
//  Created by Diego Waxemberg on 12/28/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "OnScreenControls.h"
#include "Limelight.h"

#define UPDATE_BUTTON(x, y) (buttonFlags = \
(y) ? (buttonFlags | (x)) : (buttonFlags & ~(x)))

@implementation OnScreenControls {
    UIView* _view;
    BOOL shouldDrawControls;
    CALayer* _aButton;
    CALayer* _bButton;
    CALayer* _xButton;
    CALayer* _yButton;
    CALayer* _upButton;
    CALayer* _downButton;
    CALayer* _leftButton;
    CALayer* _rightButton;

    short buttonFlags;
    short leftStickX, leftStickY;
    short rightStickX, rightStickY;
    char leftTrigger, rightTrigger;
}

static const float BUTTON_SIZE = 50;
static const float BUTTON_DIST = 20;
static float BUTTON_CENTER_X;
static float BUTTON_CENTER_Y;

static const float D_PAD_SHORT = 50;
static const float D_PAD_LONG = 65;
static const float D_PAD_DIST = 15;
static float D_PAD_CENTER_X;
static float D_PAD_CENTER_Y;

- (id) initWithView:(UIView*)view {
    self = [self init];
    _view = view;
    shouldDrawControls = YES;

    D_PAD_CENTER_X = _view.frame.size.width * .15;
    D_PAD_CENTER_Y = _view.frame.size.height * .75;
    BUTTON_CENTER_X = _view.frame.size.width * .85;
    BUTTON_CENTER_Y = _view.frame.size.height * .75;

    [self drawButtons];
    return self;
}

- (void) drawButtons {
    // create A button
    _aButton = [CALayer layer];
    _aButton.frame = CGRectMake(BUTTON_CENTER_X - BUTTON_SIZE / 2, BUTTON_CENTER_Y + BUTTON_DIST, BUTTON_SIZE, BUTTON_SIZE);
    _aButton.contents = (id) [UIImage imageNamed:@"AButton"].CGImage;
    [_view.layer addSublayer:_aButton];

    // create B button
    _bButton = [CALayer layer];
    _bButton.frame = CGRectMake(BUTTON_CENTER_X + BUTTON_DIST, BUTTON_CENTER_Y - BUTTON_SIZE / 2, BUTTON_SIZE, BUTTON_SIZE);
    _bButton.contents = (id) [UIImage imageNamed:@"BButton"].CGImage;
    [_view.layer addSublayer:_bButton];

    // create X Button
    _xButton = [CALayer layer];
    _xButton.frame = CGRectMake(BUTTON_CENTER_X - BUTTON_DIST - BUTTON_SIZE, BUTTON_CENTER_Y - BUTTON_SIZE / 2, BUTTON_SIZE, BUTTON_SIZE);
    _xButton.contents = (id) [UIImage imageNamed:@"XButton"].CGImage;
    [_view.layer addSublayer:_xButton];

    // create Y Button
    _yButton = [CALayer layer];
    _yButton.frame = CGRectMake(BUTTON_CENTER_X - BUTTON_SIZE / 2, BUTTON_CENTER_Y - BUTTON_DIST - BUTTON_SIZE, BUTTON_SIZE, BUTTON_SIZE);
    _yButton.contents = (id) [UIImage imageNamed:@"YButton"].CGImage;
    [_view.layer addSublayer:_yButton];

    // create Down button
    _downButton = [CALayer layer];
    _downButton.frame = CGRectMake(D_PAD_CENTER_X - D_PAD_SHORT / 2, D_PAD_CENTER_Y + D_PAD_DIST, D_PAD_SHORT, D_PAD_LONG);
    _downButton.contents = (id) [UIImage imageNamed:@"DownButton"].CGImage;
    [_view.layer addSublayer:_downButton];

    // create Right button
    _rightButton = [CALayer layer];
    _rightButton.frame = CGRectMake(D_PAD_CENTER_X + D_PAD_DIST, D_PAD_CENTER_Y - D_PAD_SHORT / 2, D_PAD_LONG, D_PAD_SHORT);
    _rightButton.contents = (id) [UIImage imageNamed:@"RightButton"].CGImage;
    [_view.layer addSublayer:_rightButton];

    // create Up button
    _upButton = [CALayer layer];
    _upButton.frame = CGRectMake(D_PAD_CENTER_X - D_PAD_SHORT / 2, D_PAD_CENTER_Y - D_PAD_DIST - D_PAD_LONG, D_PAD_SHORT, D_PAD_LONG);
    _upButton.contents = (id) [UIImage imageNamed:@"UpButton"].CGImage;
    [_view.layer addSublayer:_upButton];

    // create Left button
    _leftButton = [CALayer layer];
    _leftButton.frame = CGRectMake(D_PAD_CENTER_X - D_PAD_DIST - D_PAD_LONG, D_PAD_CENTER_Y - D_PAD_SHORT / 2, D_PAD_LONG, D_PAD_SHORT);
    _leftButton.contents = (id) [UIImage imageNamed:@"LeftButton"].CGImage;
    [_view.layer addSublayer:_leftButton];
}

- (void)handleTouchDownEvent:(UIEvent*)event {
    for (UITouch* touch in [event allTouches]) {
        CGPoint touchLocation = [touch locationInView:_view];

        if ([_aButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(A_FLAG, 1);
        } else if ([_bButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(B_FLAG, 1);
        } else if ([_xButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(X_FLAG, 1);
        } else if ([_yButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(Y_FLAG, 1);
        } else if ([_upButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(UP_FLAG, 1);
        } else if ([_downButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(DOWN_FLAG, 1);
        } else if ([_leftButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(LEFT_FLAG, 1);
        } else if ([_rightButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(RIGHT_FLAG, 1);
        }
        /*

        UPDATE_BUTTON(LB_FLAG, gamepad.leftShoulder.pressed);
        UPDATE_BUTTON(RB_FLAG, gamepad.rightShoulder.pressed);

        leftStickX = gamepad.leftThumbstick.xAxis.value * 0x7FFE;
        leftStickY = gamepad.leftThumbstick.yAxis.value * 0x7FFE;

        rightStickX = gamepad.rightThumbstick.xAxis.value * 0x7FFE;
        rightStickY = gamepad.rightThumbstick.yAxis.value * 0x7FFE;

        leftTrigger = gamepad.leftTrigger.value * 0xFF;
        rightTrigger = gamepad.rightTrigger.value * 0xFF;
        */
        // We call LiSendControllerEvent while holding a lock to prevent
        // multiple simultaneous calls since this function isn't thread safe.
    }
    LiSendControllerEvent(buttonFlags, leftTrigger, rightTrigger,
                          leftStickX, leftStickY, rightStickX, rightStickY);
}

- (void)handleTouchUpEvent:(UIEvent*)event {

    for (UITouch* touch in [event allTouches]) {
        CGPoint touchLocation = [touch locationInView:_view];

        if ([_aButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(A_FLAG, 0);
        } else if ([_bButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(B_FLAG, 0);
        } else if ([_xButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(X_FLAG, 0);
        } else if ([_yButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(Y_FLAG, 0);
        } else if ([_upButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(UP_FLAG, 0);
        } else if ([_downButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(DOWN_FLAG, 0);
        } else if ([_leftButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(LEFT_FLAG, 0);
        } else if ([_rightButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(RIGHT_FLAG, 0);
        }
        /*
         UPDATE_BUTTON(LB_FLAG, gamepad.leftShoulder.pressed);
         UPDATE_BUTTON(RB_FLAG, gamepad.rightShoulder.pressed);

         leftStickX = gamepad.leftThumbstick.xAxis.value * 0x7FFE;
         leftStickY = gamepad.leftThumbstick.yAxis.value * 0x7FFE;

         rightStickX = gamepad.rightThumbstick.xAxis.value * 0x7FFE;
         rightStickY = gamepad.rightThumbstick.yAxis.value * 0x7FFE;

         leftTrigger = gamepad.leftTrigger.value * 0xFF;
         rightTrigger = gamepad.rightTrigger.value * 0xFF;
         */
        // We call LiSendControllerEvent while holding a lock to prevent
        // multiple simultaneous calls since this function isn't thread safe.
    }
    LiSendControllerEvent(buttonFlags, leftTrigger, rightTrigger,
                          leftStickX, leftStickY, rightStickX, rightStickY);
}


@end
