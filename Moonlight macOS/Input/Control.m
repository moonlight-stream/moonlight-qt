//
//  Control.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 15.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//


#include "Gamepad.h"
#include "Control.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Limelight.h"

#import "Moonlight-Swift.h"
@class Controller;

#ifdef _MSC_VER
#define snprintf _snprintf
#endif


Controller* _controller;
ControllerSupport* _controllerSupport;
NSMutableDictionary* _controllers;

typedef enum {
    SELECT,
    L3,
    R3,
    START,
    UP,
    RIGHT,
    DOWN,
    LEFT,
    LB = 10,
    RB,
    Y,
    B,
    A,
    X,
} ControllerKeys;

typedef enum {
    LEFT_X,
    LEFT_Y,
    RIGHT_X,
    RIGHT_Y,
    LT = 14,
    RT,
} ControllerAxis;


void onButtonDown(struct Gamepad_device * device, unsigned int buttonID, double timestamp, void * context) {
    _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
    switch (buttonID) {
        case SELECT:
            [_controllerSupport setButtonFlag:_controller flags:BACK_FLAG];
            break;
        case L3:
            [_controllerSupport setButtonFlag:_controller flags:LS_CLK_FLAG];
            break;
        case R3:
            [_controllerSupport setButtonFlag:_controller flags:RS_CLK_FLAG];
            break;
        case START:
            [_controllerSupport setButtonFlag:_controller flags:PLAY_FLAG];
            break;
        case UP:
            [_controllerSupport setButtonFlag:_controller flags:UP_FLAG];
            break;
        case RIGHT:
            [_controllerSupport setButtonFlag:_controller flags:RIGHT_FLAG];
            break;
        case DOWN:
            [_controllerSupport setButtonFlag:_controller flags:DOWN_FLAG];
            break;
        case LEFT:
            [_controllerSupport setButtonFlag:_controller flags:LEFT_FLAG];
            break;
        case LB:
            [_controllerSupport setButtonFlag:_controller flags:LB_FLAG];
            break;
        case RB:
            [_controllerSupport setButtonFlag:_controller flags:RB_FLAG];
            break;
        case Y:
            [_controllerSupport setButtonFlag:_controller flags:Y_FLAG];
            break;
        case B:
            [_controllerSupport setButtonFlag:_controller flags:B_FLAG];
            break;
        case A:
            [_controllerSupport setButtonFlag:_controller flags:A_FLAG];
            break;
        case X:
            [_controllerSupport setButtonFlag:_controller flags:X_FLAG];
            break;

        default:
            break;
    }
    [_controllerSupport updateFinished:_controller];
}

void onButtonUp(struct Gamepad_device * device, unsigned int buttonID, double timestamp, void * context) {
    _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
    switch (buttonID) {
        case SELECT:
            [_controllerSupport clearButtonFlag:_controller flags:BACK_FLAG];
            break;
        case L3:
            [_controllerSupport clearButtonFlag:_controller flags:LS_CLK_FLAG];
            break;
        case R3:
            [_controllerSupport clearButtonFlag:_controller flags:RS_CLK_FLAG];
            break;
        case START:
            [_controllerSupport clearButtonFlag:_controller flags:PLAY_FLAG];
            break;
        case UP:
            [_controllerSupport clearButtonFlag:_controller flags:UP_FLAG];
            break;
        case RIGHT:
            [_controllerSupport clearButtonFlag:_controller flags:RIGHT_FLAG];
            break;
        case DOWN:
            [_controllerSupport clearButtonFlag:_controller flags:DOWN_FLAG];
            break;
        case LEFT:
            [_controllerSupport clearButtonFlag:_controller flags:LEFT_FLAG];
            break;
        case LB:
            [_controllerSupport clearButtonFlag:_controller flags:LB_FLAG];
            break;
        case RB:
            [_controllerSupport clearButtonFlag:_controller flags:RB_FLAG];
            break;
        case Y:
            [_controllerSupport clearButtonFlag:_controller flags:Y_FLAG];
            break;
        case B:
            [_controllerSupport clearButtonFlag:_controller flags:B_FLAG];
            break;
        case A:
            [_controllerSupport clearButtonFlag:_controller flags:A_FLAG];
            break;
        case X:
            [_controllerSupport clearButtonFlag:_controller flags:X_FLAG];
            break;

        default:
            break;
      }
      [_controllerSupport updateFinished:_controller];
}

void onAxisMoved(struct Gamepad_device * device, unsigned int axisID, float value, float lastValue, double timestamp, void * context) {
    if (fabsf(lastValue - value) > 0.01) {
        // The dualshock controller has much more than these axis because of the motion axis, so it
        // is better to call the updateFinished in the cases, because otherwise all of these
        // motion axis will also trigger an updateFinished event.
        switch (axisID) {
            case LEFT_X:
                _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
                _controller.lastLeftStickX = value * 0X7FFE;
                [_controllerSupport updateFinished:_controller];
                break;
            case LEFT_Y:
                _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
                _controller.lastLeftStickY = -value * 0X7FFE;
                [_controllerSupport updateFinished:_controller];
                break;
            case RIGHT_X:
                _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
                _controller.lastRightStickX = value * 0X7FFE;
                [_controllerSupport updateFinished:_controller];
                break;
            case RIGHT_Y:
                _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
                _controller.lastRightStickY = -value * 0X7FFE;
                [_controllerSupport updateFinished:_controller];
                break;
            case LT:
                _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
                _controller.lastLeftTrigger = value * 0xFF;
                [_controllerSupport updateFinished:_controller];
                break;
            case RT:
                _controller = [_controllers objectForKey:[NSNumber numberWithInteger:device->deviceID]];
                _controller.lastRightTrigger = value * 0xFF;
                [_controllerSupport updateFinished:_controller];
                break;
                
            default:
                break;
        }
    }
}

void onDeviceAttached(struct Gamepad_device * device, void * context) {
    [_controllerSupport assignGamepad:device];
    _controllers = [_controllerSupport getControllers];
}

void onDeviceRemoved(struct Gamepad_device * device, void * context) {
    [_controllerSupport removeGamepad:device];
    _controllers = [_controllerSupport getControllers];
}

void initGamepad(ControllerSupport* controllerSupport) {
    _controllerSupport = controllerSupport;
    _controller = [[Controller alloc] init];
    Gamepad_deviceAttachFunc(onDeviceAttached, NULL);
    Gamepad_deviceRemoveFunc(onDeviceRemoved, NULL);
    Gamepad_buttonDownFunc(onButtonDown, NULL);
    Gamepad_buttonUpFunc(onButtonUp, NULL);
    Gamepad_axisMoveFunc(onAxisMoved, NULL);
    Gamepad_init();
}
