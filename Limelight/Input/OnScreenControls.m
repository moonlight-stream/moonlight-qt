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
    CALayer* _aButton;
    CALayer* _bButton;
    CALayer* _xButton;
    CALayer* _yButton;
    CALayer* _upButton;
    CALayer* _downButton;
    CALayer* _leftButton;
    CALayer* _rightButton;
    CALayer* _leftStick;
    CALayer* _rightStick;

    short buttonFlags;
    short leftStickX, leftStickY;
    short rightStickX, rightStickY;
    char leftTrigger, rightTrigger;

    UITouch* lsTouch;
    UITouch* rsTouch;

    UIView* _view;
    BOOL shouldDrawControls;
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

static const float STICK_INNER_SIZE = 80;
static const float STICK_OUTER_SIZE = 120;
static float LS_CENTER_X;
static float LS_CENTER_Y;
static float RS_CENTER_X;
static float RS_CENTER_Y;

- (id) initWithView:(UIView*)view {
    self = [self init];
    _view = view;
    shouldDrawControls = YES;

    D_PAD_CENTER_X = _view.frame.size.width * .15;
    D_PAD_CENTER_Y = _view.frame.size.height * .5;
    BUTTON_CENTER_X = _view.frame.size.width * .85;
    BUTTON_CENTER_Y = _view.frame.size.height * .5;

    LS_CENTER_X = _view.frame.size.width * .35;
    LS_CENTER_Y = _view.frame.size.height * .75;
    RS_CENTER_X = _view.frame.size.width * .65;
    RS_CENTER_Y = _view.frame.size.height * .75;

    [self drawButtons];
    [self drawSticks];
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

- (void) drawSticks {
    // create left analog stick
    CALayer* leftStickBackground = [CALayer layer];
    leftStickBackground.frame = CGRectMake(LS_CENTER_X - STICK_OUTER_SIZE / 2, LS_CENTER_Y - STICK_OUTER_SIZE / 2, STICK_OUTER_SIZE, STICK_OUTER_SIZE);
    leftStickBackground.contents = (id) [UIImage imageNamed:@"StickOuter"].CGImage;
    [_view.layer addSublayer:leftStickBackground];

    _leftStick = [CALayer layer];
    _leftStick.frame = CGRectMake(LS_CENTER_X - STICK_INNER_SIZE / 2, LS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
    _leftStick.contents = (id) [UIImage imageNamed:@"StickInner"].CGImage;
    [_view.layer addSublayer:_leftStick];

    // create right analog stick
    CALayer* rightStickBackground = [CALayer layer];
    rightStickBackground.frame = CGRectMake(RS_CENTER_X - STICK_OUTER_SIZE / 2, RS_CENTER_Y - STICK_OUTER_SIZE / 2, STICK_OUTER_SIZE, STICK_OUTER_SIZE);
    rightStickBackground.contents = (id) [UIImage imageNamed:@"StickOuter"].CGImage;
    [_view.layer addSublayer:rightStickBackground];

    _rightStick = [CALayer layer];
    _rightStick.frame = CGRectMake(RS_CENTER_X - STICK_INNER_SIZE / 2, RS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
    _rightStick.contents = (id) [UIImage imageNamed:@"StickInner"].CGImage;
    [_view.layer addSublayer:_rightStick];
}

- (void) handleTouchMovedEvent:(UIEvent*)event {
    float rsMaxX = RS_CENTER_X + STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;
    float rsMaxY = RS_CENTER_Y + STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;
    float rsMinX = RS_CENTER_X - STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;
    float rsMinY = RS_CENTER_Y - STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;
    float lsMaxX = LS_CENTER_X + STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;
    float lsMaxY = LS_CENTER_Y + STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;
    float lsMinX = LS_CENTER_X - STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;
    float lsMinY = LS_CENTER_Y - STICK_OUTER_SIZE / 2 - STICK_INNER_SIZE / 2;

    for (UITouch* touch in [event allTouches]) {
        CGPoint touchLocation = [touch locationInView:_view];
        float xLoc = touchLocation.x - STICK_INNER_SIZE / 2;
        float yLoc = touchLocation.y - STICK_INNER_SIZE / 2;
        if (touch == lsTouch) {
            if (xLoc > lsMaxX) xLoc = lsMaxX;
            if (xLoc < lsMinX) xLoc = lsMinX;
            if (yLoc > lsMaxY) yLoc = lsMaxY;
            if (yLoc < lsMinY) yLoc = lsMinY;

            _leftStick.frame = CGRectMake(xLoc, yLoc, STICK_INNER_SIZE, STICK_INNER_SIZE);

            leftStickX = 0x7FFF * (xLoc - LS_CENTER_X) / (lsMaxX - LS_CENTER_X);
            leftStickY = 0x7FFF * (yLoc - LS_CENTER_Y) / (lsMaxY - LS_CENTER_Y);
            NSLog(@"Left Stick: x: %d y: %d", leftStickX, leftStickY);

        } else if (touch == rsTouch) {
            if (xLoc > rsMaxX) xLoc = rsMaxX;
            if (xLoc < rsMinX) xLoc = rsMinX;
            if (yLoc > rsMaxY) yLoc = rsMaxY;
            if (yLoc < rsMinY) yLoc = rsMinY;

            _rightStick.frame = CGRectMake(xLoc, yLoc, STICK_INNER_SIZE, STICK_INNER_SIZE);

            rightStickX = 0x7FFF * (xLoc - RS_CENTER_X) / (rsMaxX - RS_CENTER_X);
            rightStickY = 0x7FFF * (yLoc - RS_CENTER_Y) / (rsMaxY - RS_CENTER_Y);
            NSLog(@"Right Stick: x: %d y: %d", rightStickX, rightStickY);
        }
    }
    LiSendControllerEvent(buttonFlags, leftTrigger, rightTrigger,
                          leftStickX, leftStickY, rightStickX, rightStickY);
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
        } else if ([_leftStick.presentationLayer hitTest:touchLocation]) {
            lsTouch = touch;
        } else if ([_rightStick.presentationLayer hitTest:touchLocation]) {
            rsTouch = touch;
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
        if (touch == lsTouch) {
            _leftStick.frame = CGRectMake(LS_CENTER_X - STICK_INNER_SIZE / 2, LS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
        } else if (touch == rsTouch) {
            _rightStick.frame = CGRectMake(RS_CENTER_X - STICK_INNER_SIZE / 2, RS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
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
