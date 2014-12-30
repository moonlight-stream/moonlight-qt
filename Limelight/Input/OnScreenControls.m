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
    CALayer* _leftStickBackground;
    CALayer* _leftStick;
    CALayer* _rightStickBackground;
    CALayer* _rightStick;
    CALayer* _startButton;
    CALayer* _selectButton;
    CALayer* _r1Button;
    CALayer* _r2Button;
    CALayer* _l1Button;
    CALayer* _l2Button;

    short buttonFlags;
    short leftStickX, leftStickY;
    short rightStickX, rightStickY;
    char leftTrigger, rightTrigger;
    
    UITouch* _aTouch;
    UITouch* _bTouch;
    UITouch* _xTouch;
    UITouch* _yTouch;
    UITouch* _upTouch;
    UITouch* _downTouch;
    UITouch* _leftTouch;
    UITouch* _rightTouch;
    UITouch* _lsTouch;
    UITouch* _rsTouch;
    UITouch* _startTouch;
    UITouch* _selectTouch;
    UITouch* _r1Touch;
    UITouch* _r2Touch;
    UITouch* _l1Touch;
    UITouch* _l2Touch;
    
    UIView* _view;
    OnScreenControlsLevel _level;
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
static const float STICK_DEAD_ZONE = .1;
static float LS_CENTER_X;
static float LS_CENTER_Y;
static float RS_CENTER_X;
static float RS_CENTER_Y;

static const float START_WIDTH = 50;
static const float START_HEIGHT = 30;
static float START_X;
static float START_Y;

static const float SELECT_WIDTH = 50;
static const float SELECT_HEIGHT = 30;
static float SELECT_X;
static float SELECT_Y;

static const float BUMPER_SIZE = 70;
static const float TRIGGER_SIZE = 80;

static float R1_X;
static float R1_Y;
static float R2_X;
static float R2_Y;
static float L1_X;
static float L1_Y;
static float L2_X;
static float L2_Y;

- (id) initWithView:(UIView*)view {
    self = [self init];
    _view = view;
    
    D_PAD_CENTER_X = _view.frame.size.width * .15;
    D_PAD_CENTER_Y = _view.frame.size.height * .55;
    BUTTON_CENTER_X = _view.frame.size.width * .85;
    BUTTON_CENTER_Y = _view.frame.size.height * .55;

    LS_CENTER_X = _view.frame.size.width * .35;
    LS_CENTER_Y = _view.frame.size.height * .75;
    RS_CENTER_X = _view.frame.size.width * .65;
    RS_CENTER_Y = _view.frame.size.height * .75;

    START_X = _view.frame.size.width * .55;
    START_Y = _view.frame.size.height * .1;
    SELECT_X = _view.frame.size.width * .45;
    SELECT_Y = _view.frame.size.height * .1;

    L1_X = _view.frame.size.width * .15;
    L1_Y = _view.frame.size.height * .25;
    L2_X = _view.frame.size.width * .15;
    L2_Y = _view.frame.size.height * .1;
    R1_X = _view.frame.size.width * .85;
    R1_Y = _view.frame.size.height * .25;
    R2_X = _view.frame.size.width * .85;
    R2_Y = _view.frame.size.height * .1;
    
    _aButton = [CALayer layer];
    _bButton = [CALayer layer];
    _xButton = [CALayer layer];
    _yButton = [CALayer layer];
    _upButton = [CALayer layer];
    _downButton = [CALayer layer];
    _leftButton = [CALayer layer];
    _rightButton = [CALayer layer];
    _l1Button = [CALayer layer];
    _r1Button = [CALayer layer];
    _l2Button = [CALayer layer];
    _r2Button = [CALayer layer];
    _startButton = [CALayer layer];
    _selectButton = [CALayer layer];
    _leftStickBackground = [CALayer layer];
    _rightStickBackground = [CALayer layer];
    _leftStick = [CALayer layer];
    _rightStick = [CALayer layer];

    return self;
}

- (void) setLevel:(OnScreenControlsLevel)level {
    _level = level;
    [self updateControls];
}

- (void) updateControls {
    switch (_level) {
        case OnScreenControlsLevelOff:
            [self hideButtons];
            [self hideBumpers];
            [self hideTriggers];
            [self hideStartSelect];
            [self hideSticks];
            break;
        case OnScreenControlsLevelSimple:
            [self drawTriggers];
            [self drawStartSelect];
            [self hideButtons];
            [self hideBumpers];
            [self hideSticks];
            break;
        case OnScreenControlsLevelFull:
            [self drawButtons];
            [self drawStartSelect];
            [self drawBumpers];
            [self drawTriggers];
            [self drawSticks];
            break;
    }
}

- (void) drawButtons {
    // create A button
    _aButton.contents = (id) [UIImage imageNamed:@"AButton"].CGImage;
    _aButton.frame = CGRectMake(BUTTON_CENTER_X - BUTTON_SIZE / 2, BUTTON_CENTER_Y + BUTTON_DIST, BUTTON_SIZE, BUTTON_SIZE);
    [_view.layer addSublayer:_aButton];

    // create B button
    _bButton.frame = CGRectMake(BUTTON_CENTER_X + BUTTON_DIST, BUTTON_CENTER_Y - BUTTON_SIZE / 2, BUTTON_SIZE, BUTTON_SIZE);
    _bButton.contents = (id) [UIImage imageNamed:@"BButton"].CGImage;
    [_view.layer addSublayer:_bButton];

    // create X Button
    _xButton.frame = CGRectMake(BUTTON_CENTER_X - BUTTON_DIST - BUTTON_SIZE, BUTTON_CENTER_Y - BUTTON_SIZE / 2, BUTTON_SIZE, BUTTON_SIZE);
    _xButton.contents = (id) [UIImage imageNamed:@"XButton"].CGImage;
    [_view.layer addSublayer:_xButton];

    // create Y Button
    _yButton.frame = CGRectMake(BUTTON_CENTER_X - BUTTON_SIZE / 2, BUTTON_CENTER_Y - BUTTON_DIST - BUTTON_SIZE, BUTTON_SIZE, BUTTON_SIZE);
    _yButton.contents = (id) [UIImage imageNamed:@"YButton"].CGImage;
    [_view.layer addSublayer:_yButton];

    // create Down button
    _downButton.frame = CGRectMake(D_PAD_CENTER_X - D_PAD_SHORT / 2, D_PAD_CENTER_Y + D_PAD_DIST, D_PAD_SHORT, D_PAD_LONG);
    _downButton.contents = (id) [UIImage imageNamed:@"DownButton"].CGImage;
    [_view.layer addSublayer:_downButton];

    // create Right button
    _rightButton.frame = CGRectMake(D_PAD_CENTER_X + D_PAD_DIST, D_PAD_CENTER_Y - D_PAD_SHORT / 2, D_PAD_LONG, D_PAD_SHORT);
    _rightButton.contents = (id) [UIImage imageNamed:@"RightButton"].CGImage;
    [_view.layer addSublayer:_rightButton];

    // create Up button
    _upButton.frame = CGRectMake(D_PAD_CENTER_X - D_PAD_SHORT / 2, D_PAD_CENTER_Y - D_PAD_DIST - D_PAD_LONG, D_PAD_SHORT, D_PAD_LONG);
    _upButton.contents = (id) [UIImage imageNamed:@"UpButton"].CGImage;
    [_view.layer addSublayer:_upButton];

    // create Left button
    _leftButton.frame = CGRectMake(D_PAD_CENTER_X - D_PAD_DIST - D_PAD_LONG, D_PAD_CENTER_Y - D_PAD_SHORT / 2, D_PAD_LONG, D_PAD_SHORT);
    _leftButton.contents = (id) [UIImage imageNamed:@"LeftButton"].CGImage;
    [_view.layer addSublayer:_leftButton];
}

- (void) drawStartSelect {
    // create Start button
    _startButton.frame = CGRectMake(START_X - START_WIDTH / 2, START_Y - START_HEIGHT / 2, START_WIDTH, START_HEIGHT);
    _startButton.contents = (id) [UIImage imageNamed:@"StartButton"].CGImage;
    [_view.layer addSublayer:_startButton];

    // create Select button
    _selectButton.frame = CGRectMake(SELECT_X - SELECT_WIDTH / 2, SELECT_Y - SELECT_HEIGHT / 2, SELECT_WIDTH, SELECT_HEIGHT);
    _selectButton.contents = (id) [UIImage imageNamed:@"SelectButton"].CGImage;
    [_view.layer addSublayer:_selectButton];
}

- (void) drawBumpers {
    // create L1 button
    _l1Button.frame = CGRectMake(L1_X - BUMPER_SIZE / 2, L1_Y - BUMPER_SIZE / 2, BUMPER_SIZE, BUMPER_SIZE);
    _l1Button.contents = (id) [UIImage imageNamed:@"L1"].CGImage;
    [_view.layer addSublayer:_l1Button];
    
    // create R1 button
    _r1Button.frame = CGRectMake(R1_X - BUMPER_SIZE / 2, R1_Y - BUMPER_SIZE / 2, BUMPER_SIZE, BUMPER_SIZE);
    _r1Button.contents = (id) [UIImage imageNamed:@"R1"].CGImage;
    [_view.layer addSublayer:_r1Button];
}

- (void) drawTriggers {
    // create L2 button
    _l2Button.frame = CGRectMake(L2_X - TRIGGER_SIZE / 2, L2_Y - TRIGGER_SIZE / 2, TRIGGER_SIZE, TRIGGER_SIZE);
    _l2Button.contents = (id) [UIImage imageNamed:@"L2"].CGImage;
    [_view.layer addSublayer:_l2Button];
    
    // create R2 button
    _r2Button.frame = CGRectMake(R2_X - TRIGGER_SIZE / 2, R2_Y - TRIGGER_SIZE / 2, TRIGGER_SIZE, TRIGGER_SIZE);
    _r2Button.contents = (id) [UIImage imageNamed:@"R2"].CGImage;
    [_view.layer addSublayer:_r2Button];
}

- (void) drawSticks {
    // create left analog stick
    _leftStickBackground.frame = CGRectMake(LS_CENTER_X - STICK_OUTER_SIZE / 2, LS_CENTER_Y - STICK_OUTER_SIZE / 2, STICK_OUTER_SIZE, STICK_OUTER_SIZE);
    _leftStickBackground.contents = (id) [UIImage imageNamed:@"StickOuter"].CGImage;
    [_view.layer addSublayer:_leftStickBackground];
    
    _leftStick = [CALayer layer];
    _leftStick.frame = CGRectMake(LS_CENTER_X - STICK_INNER_SIZE / 2, LS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
    _leftStick.contents = (id) [UIImage imageNamed:@"StickInner"].CGImage;
    [_view.layer addSublayer:_leftStick];

    // create right analog stick
    _rightStickBackground.frame = CGRectMake(RS_CENTER_X - STICK_OUTER_SIZE / 2, RS_CENTER_Y - STICK_OUTER_SIZE / 2, STICK_OUTER_SIZE, STICK_OUTER_SIZE);
    _rightStickBackground.contents = (id) [UIImage imageNamed:@"StickOuter"].CGImage;
    [_view.layer addSublayer:_rightStickBackground];
    
    _rightStick = [CALayer layer];
    _rightStick.frame = CGRectMake(RS_CENTER_X - STICK_INNER_SIZE / 2, RS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
    _rightStick.contents = (id) [UIImage imageNamed:@"StickInner"].CGImage;
    [_view.layer addSublayer:_rightStick];
}

- (void) hideButtons {
    [_aButton removeFromSuperlayer];
    [_bButton removeFromSuperlayer];
    [_xButton removeFromSuperlayer];
    [_yButton removeFromSuperlayer];
    [_upButton removeFromSuperlayer];
    [_downButton removeFromSuperlayer];
    [_leftButton removeFromSuperlayer];
    [_rightButton removeFromSuperlayer];
}

- (void) hideStartSelect {
    [_startButton removeFromSuperlayer];
    [_selectButton removeFromSuperlayer];
}

- (void) hideBumpers {
    [_l1Button removeFromSuperlayer];
    [_r1Button removeFromSuperlayer];
}

- (void) hideTriggers {
    [_l2Button removeFromSuperlayer];
    [_r2Button removeFromSuperlayer];
}

- (void) hideSticks {
    [_leftStickBackground removeFromSuperlayer];
    [_rightStickBackground removeFromSuperlayer];
    [_leftStick removeFromSuperlayer];
    [_rightStick removeFromSuperlayer];
}

- (BOOL) handleTouchMovedEvent:(UIEvent*)event {
    BOOL shouldSendPacket = false;
    BOOL buttonTouch = false;
    float rsMaxX = RS_CENTER_X + STICK_OUTER_SIZE / 2;
    float rsMaxY = RS_CENTER_Y + STICK_OUTER_SIZE / 2;
    float rsMinX = RS_CENTER_X - STICK_OUTER_SIZE / 2;
    float rsMinY = RS_CENTER_Y - STICK_OUTER_SIZE / 2;
    float lsMaxX = LS_CENTER_X + STICK_OUTER_SIZE / 2;
    float lsMaxY = LS_CENTER_Y + STICK_OUTER_SIZE / 2;
    float lsMinX = LS_CENTER_X - STICK_OUTER_SIZE / 2;
    float lsMinY = LS_CENTER_Y - STICK_OUTER_SIZE / 2;

    for (UITouch* touch in [event allTouches]) {
        CGPoint touchLocation = [touch locationInView:_view];
        float xLoc = touchLocation.x;
        float yLoc = touchLocation.y;
        if (touch == _lsTouch) {
            if (xLoc > lsMaxX) xLoc = lsMaxX;
            if (xLoc < lsMinX) xLoc = lsMinX;
            if (yLoc > lsMaxY) yLoc = lsMaxY;
            if (yLoc < lsMinY) yLoc = lsMinY;

            _leftStick.frame = CGRectMake(xLoc - STICK_INNER_SIZE / 2, yLoc - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);

            float xStickVal = (xLoc - LS_CENTER_X) / (lsMaxX - LS_CENTER_X);
            float yStickVal = (yLoc - LS_CENTER_Y) / (lsMaxY - LS_CENTER_Y);
            NSLog(@"Left Stick x: %f y: %f", xStickVal, yStickVal);

            if (fabsf(xStickVal) < STICK_DEAD_ZONE) xStickVal = 0;
            if (fabsf(yStickVal) < STICK_DEAD_ZONE) yStickVal = 0;

            leftStickX = 0x7FFE * xStickVal;
            leftStickY = 0x7FFE * -yStickVal;

            shouldSendPacket = true;
        } else if (touch == _rsTouch) {
            if (xLoc > rsMaxX) xLoc = rsMaxX;
            if (xLoc < rsMinX) xLoc = rsMinX;
            if (yLoc > rsMaxY) yLoc = rsMaxY;
            if (yLoc < rsMinY) yLoc = rsMinY;

            _rightStick.frame = CGRectMake(xLoc - STICK_INNER_SIZE / 2, yLoc - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);

            float xStickVal = (xLoc - RS_CENTER_X) / (rsMaxX - RS_CENTER_X);
            float yStickVal = (yLoc - RS_CENTER_Y) / (rsMaxY - RS_CENTER_Y);
            NSLog(@"Right Stick x: %f y: %f", xStickVal, yStickVal);

            if (fabsf(xStickVal) < STICK_DEAD_ZONE) xStickVal = 0;
            if (fabsf(yStickVal) < STICK_DEAD_ZONE) yStickVal = 0;

            rightStickX = 0x7FFE * xStickVal;
            rightStickY = 0x7FFE * -yStickVal;

            shouldSendPacket = true;
        } else if (touch == _aTouch) {
            buttonTouch = true;
        } else if (touch == _bTouch) {
            buttonTouch = true;
        } else if (touch == _xTouch) {
            buttonTouch = true;
        } else if (touch == _yTouch) {
            buttonTouch = true;
        } else if (touch == _upTouch) {
            buttonTouch = true;
        } else if (touch == _downTouch) {
            buttonTouch = true;
        } else if (touch == _leftTouch) {
            buttonTouch = true;
        } else if (touch == _rightTouch) {
            buttonTouch = true;
        } else if (touch == _startTouch) {
            buttonTouch = true;
        } else if (touch == _selectTouch) {
            buttonTouch = true;
        } else if (touch == _l1Touch) {
            buttonTouch = true;
        } else if (touch == _r1Touch) {
            buttonTouch = true;
        } else if (touch == _l2Touch) {
            buttonTouch = true;
        } else if (touch == _r2Touch) {
            buttonTouch = true;
        }
    }
    if (shouldSendPacket) {
        LiSendControllerEvent(buttonFlags, leftTrigger, rightTrigger,
                          leftStickX, leftStickY, rightStickX, rightStickY);
    }
    return shouldSendPacket || buttonTouch;
}

- (BOOL)handleTouchDownEvent:(UIEvent*)event {
    BOOL shouldSendPacket = false;
    for (UITouch* touch in [event allTouches]) {
        CGPoint touchLocation = [touch locationInView:_view];

        if ([_aButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(A_FLAG, 1);
            _aTouch = touch;
            shouldSendPacket = true;
        } else if ([_bButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(B_FLAG, 1);
            _bTouch = touch;
            shouldSendPacket = true;
        } else if ([_xButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(X_FLAG, 1);
            _xTouch = touch;
            shouldSendPacket = true;
        } else if ([_yButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(Y_FLAG, 1);
            _yTouch = touch;
            shouldSendPacket = true;
        } else if ([_upButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(UP_FLAG, 1);
            _upTouch = touch;
            shouldSendPacket = true;
        } else if ([_downButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(DOWN_FLAG, 1);
            _downTouch = touch;
            shouldSendPacket = true;
        } else if ([_leftButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(LEFT_FLAG, 1);
            _leftTouch = touch;
            shouldSendPacket = true;
        } else if ([_rightButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(RIGHT_FLAG, 1);
            _rightTouch = touch;
            shouldSendPacket = true;
        } else if ([_startButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(PLAY_FLAG, 1);
            _startTouch = touch;
            shouldSendPacket = true;
        } else if ([_selectButton.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(BACK_FLAG, 1);
            _selectTouch = touch;
            shouldSendPacket = true;
        } else if ([_l1Button.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(LB_FLAG, 1);
            _l1Touch = touch;
            shouldSendPacket = true;
        } else if ([_r1Button.presentationLayer hitTest:touchLocation]) {
            UPDATE_BUTTON(RB_FLAG, 1);
            _r1Touch = touch;
            shouldSendPacket = true;
        } else if ([_l2Button.presentationLayer hitTest:touchLocation]) {
            leftTrigger = 1 * 0xFF;
            _l2Touch = touch;
            shouldSendPacket = true;
        } else if ([_r2Button.presentationLayer hitTest:touchLocation]) {
            rightTrigger = 1 * 0xFF;
            _r2Touch = touch;
            shouldSendPacket = true;
        } else if ([_leftStick.presentationLayer hitTest:touchLocation]) {
            _lsTouch = touch;
            shouldSendPacket = true;
        } else if ([_rightStick.presentationLayer hitTest:touchLocation]) {
            _rsTouch = touch;
            shouldSendPacket = true;
        }
    }
    if (shouldSendPacket) {
        LiSendControllerEvent(buttonFlags, leftTrigger, rightTrigger,
                              leftStickX, leftStickY, rightStickX, rightStickY);
    }
    return shouldSendPacket;
}

- (BOOL)handleTouchUpEvent:(UIEvent*)event {
    BOOL shouldSendPacket = false;
    for (UITouch* touch in [event allTouches]) {
        if (touch == _aTouch) {
            UPDATE_BUTTON(A_FLAG, 0);
            _aTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _bTouch) {
            UPDATE_BUTTON(B_FLAG, 0);
            _bTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _xTouch) {
            UPDATE_BUTTON(X_FLAG, 0);
            _xTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _yTouch) {
            UPDATE_BUTTON(Y_FLAG, 0);
            _yTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _upTouch) {
            UPDATE_BUTTON(UP_FLAG, 0);
            _upTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _downTouch) {
            UPDATE_BUTTON(DOWN_FLAG, 0);
            _downTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _leftTouch) {
            UPDATE_BUTTON(LEFT_FLAG, 0);
            _leftTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _rightTouch) {
            UPDATE_BUTTON(RIGHT_FLAG, 0);
            _rightTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _startTouch) {
            UPDATE_BUTTON(PLAY_FLAG, 0);
            _startTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _selectTouch) {
            UPDATE_BUTTON(BACK_FLAG, 0);
            _selectTouch = nil;
            shouldSendPacket = true;
        } else if (touch == _l1Touch) {
            UPDATE_BUTTON(LB_FLAG, 0);
            _l1Touch = nil;
            shouldSendPacket = true;
        } else if (touch == _r1Touch) {
            UPDATE_BUTTON(RB_FLAG, 0);
            _r1Touch = nil;
            shouldSendPacket = true;
        } else if (touch == _l2Touch) {
            leftTrigger = 0 * 0xFF;
            _l2Touch = nil;
            shouldSendPacket = true;
        } else if (touch == _r2Touch) {
            rightTrigger = 0 * 0xFF;
            _r2Touch = nil;
            shouldSendPacket = true;
        } else if (touch == _lsTouch) {
            _leftStick.frame = CGRectMake(LS_CENTER_X - STICK_INNER_SIZE / 2, LS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
            leftStickX = 0 * 0x7FFE;
            leftStickY = 0 * 0x7FFE;
            shouldSendPacket = true;
            _lsTouch = nil;
        } else if (touch == _rsTouch) {
            _rightStick.frame = CGRectMake(RS_CENTER_X - STICK_INNER_SIZE / 2, RS_CENTER_Y - STICK_INNER_SIZE / 2, STICK_INNER_SIZE, STICK_INNER_SIZE);
            rightStickX = 0 * 0x7FFE;
            rightStickY = 0 * 0x7FFE;
            _rsTouch = nil;
            shouldSendPacket = true;
        }
    }
    if (shouldSendPacket) {
        LiSendControllerEvent(buttonFlags, leftTrigger, rightTrigger,
                              leftStickX, leftStickY, rightStickX, rightStickY);
    }
    return  shouldSendPacket;
}

@end
