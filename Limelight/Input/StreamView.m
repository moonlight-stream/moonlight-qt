//
//  StreamView.m
//  Moonlight
//
//  Created by Cameron Gutman on 10/19/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "StreamView.h"
#include <Limelight.h>
#import "OnScreenControls.h"
#import "DataManager.h"
#import "ControllerSupport.h"
#import "KeyboardSupport.h"

@implementation StreamView {
    CGPoint touchLocation, originalLocation;
    BOOL touchMoved;
    OnScreenControls* onScreenControls;
    
    BOOL isInputingText;
    BOOL isDragging;
    NSTimer* dragTimer;
    
    float xDeltaFactor;
    float yDeltaFactor;
    float screenFactor;
}

- (void) setMouseDeltaFactors:(float)x y:(float)y {
    xDeltaFactor = x;
    yDeltaFactor = y;
    
    screenFactor = [[UIScreen mainScreen] scale];
}

- (void) setupOnScreenControls:(ControllerSupport*)controllerSupport swipeDelegate:(id<EdgeDetectionDelegate>)swipeDelegate {
    onScreenControls = [[OnScreenControls alloc] initWithView:self controllerSup:controllerSupport swipeDelegate:swipeDelegate];
    DataManager* dataMan = [[DataManager alloc] init];
    OnScreenControlsLevel level = (OnScreenControlsLevel)[[dataMan getSettings].onscreenControls integerValue];
    
    if (level == OnScreenControlsLevelAuto) {
        [controllerSupport initAutoOnScreenControlMode:onScreenControls];
    }
    else {
        Log(LOG_I, @"Setting manual on-screen controls level: %d", (int)level);
        [onScreenControls setLevel:level];
    }
}

- (Boolean)isConfirmedMove:(CGPoint)currentPoint from:(CGPoint)originalPoint {
    // Movements of greater than 10 pixels are considered confirmed
    return hypotf(originalPoint.x - currentPoint.x, originalPoint.y - currentPoint.y) >= 10;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    Log(LOG_D, @"Touch down");
    if (![onScreenControls handleTouchDownEvent:touches]) {
        UITouch *touch = [[event allTouches] anyObject];
        originalLocation = touchLocation = [touch locationInView:self];
        touchMoved = false;
        if ([[event allTouches] count] == 1 && !isDragging) {
            dragTimer = [NSTimer scheduledTimerWithTimeInterval:0.650
                                                     target:self
                                                   selector:@selector(onDragStart:)
                                                   userInfo:nil
                                                    repeats:NO];
        }
    }
}

- (void)onDragStart:(NSTimer*)timer {
    if (!touchMoved && !isDragging){
        isDragging = true;
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
    if (![onScreenControls handleTouchMovedEvent:touches]) {
        if ([[event allTouches] count] == 1) {
            UITouch *touch = [[event allTouches] anyObject];
            CGPoint currentLocation = [touch locationInView:self];
            
            if (touchLocation.x != currentLocation.x ||
                touchLocation.y != currentLocation.y)
            {
                int deltaX = currentLocation.x - touchLocation.x;
                int deltaY = currentLocation.y - touchLocation.y;
                
                deltaX *= xDeltaFactor * screenFactor;
                deltaY *= yDeltaFactor * screenFactor;
                
                if (deltaX != 0 || deltaY != 0) {
                    LiSendMouseMoveEvent(deltaX, deltaY);
                    touchLocation = currentLocation;
                    
                    // If we've moved far enough to confirm this wasn't just human/machine error,
                    // mark it as such.
                    if ([self isConfirmedMove:touchLocation from:originalLocation]) {
                        touchMoved = true;
                    }
                }
            }
        } else if ([[event allTouches] count] == 2) {
            CGPoint firstLocation = [[[[event allTouches] allObjects] objectAtIndex:0] locationInView:self];
            CGPoint secondLocation = [[[[event allTouches] allObjects] objectAtIndex:1] locationInView:self];
            
            CGPoint avgLocation = CGPointMake((firstLocation.x + secondLocation.x) / 2, (firstLocation.y + secondLocation.y) / 2);
            if (touchLocation.y != avgLocation.y) {
                LiSendScrollEvent(avgLocation.y - touchLocation.y);
            }

            // If we've moved far enough to confirm this wasn't just human/machine error,
            // mark it as such.
            if ([self isConfirmedMove:firstLocation from:originalLocation]) {
                touchMoved = true;
            }
            
            touchLocation = avgLocation;
        }
    }
    
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    Log(LOG_D, @"Touch up");
    if (![onScreenControls handleTouchUpEvent:touches]) {
        [dragTimer invalidate];
        dragTimer = nil;
        if (isDragging) {
            isDragging = false;
            LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
        } else if (!touchMoved) {
            if ([[event allTouches] count] == 3) {
                if (isInputingText) {
                    Log(LOG_D, @"Closing the keyboard");
                    [_keyInputField resignFirstResponder];
                    isInputingText = false;
                } else {
                    Log(LOG_D, @"Opening the keyboard");
                    // Prepare the textbox used to capture keyboard events.
                    _keyInputField.delegate = self;
                    _keyInputField.text = @"0";
                    [_keyInputField becomeFirstResponder];
                    [_keyInputField addTarget:self action:@selector(onKeyboardPressed:) forControlEvents:UIControlEventEditingChanged];
                    
                    // Undo causes issues for our state management, so turn it off
                    [_keyInputField.undoManager disableUndoRegistration];
                    
                    isInputingText = true;
                }
            } else if ([[event allTouches] count]  == 2) {
                dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                    Log(LOG_D, @"Sending right mouse button press");
                    
                    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);
                    
                    // Wait 100 ms to simulate a real button press
                    usleep(100 * 1000);
                    
                    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_RIGHT);
                });
            } else if ([[event allTouches] count]  == 1) {
                dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                    if (!self->isDragging){
                        Log(LOG_D, @"Sending left mouse button press");
                        
                        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
                        
                        // Wait 100 ms to simulate a real button press
                        usleep(100 * 1000);
                    }
                    self->isDragging = false;
                    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
                });
            }
        }
        
        // We we're moving from 2+ touches to 1. Synchronize the current position
        // of the active finger so we don't jump unexpectedly on the next touchesMoved
        // callback when finger 1 switches on us.
        if ([[event allTouches] count] - [touches count] == 1) {
            NSMutableSet *activeSet = [[NSMutableSet alloc] initWithCapacity:[[event allTouches] count]];
            [activeSet unionSet:[event allTouches]];
            [activeSet minusSet:touches];
            touchLocation = [[activeSet anyObject] locationInView:self];
            
            // Mark this touch as moved so we don't send a left mouse click if the user
            // right clicks without moving their other finger.
            touchMoved = true;
        }
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    // This method is called when the "Return" key is pressed.
    LiSendKeyboardEvent(0x0d, KEY_ACTION_DOWN, 0);
    usleep(50 * 1000);
    LiSendKeyboardEvent(0x0d, KEY_ACTION_UP, 0);
    return NO;
}

- (void)onKeyboardPressed:(UITextField *)textField {
    NSString* inputText = textField.text;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        // If the text became empty, we know the user pressed the backspace key.
        if ([inputText isEqual:@""]) {
            LiSendKeyboardEvent(0x08, KEY_ACTION_DOWN, 0);
            usleep(50 * 1000);
            LiSendKeyboardEvent(0x08, KEY_ACTION_UP, 0);
        } else {
            // Character 0 will be our known sentinel value
            for (int i = 1; i < [inputText length]; i++) {
                struct KeyEvent event = [KeyboardSupport translateKeyEvent:[inputText characterAtIndex:i]];
                if (event.keycode == 0) {
                    // If we don't know the code, don't send anything.
                    Log(LOG_W, @"Unknown key code: [%c]", [inputText characterAtIndex:i]);
                    continue;
                }
                
                // When we want to send a modified key (like uppercase letters) we need to send the
                // modifier ("shift") seperately from the key itself.
                if (event.modifier != 0) {
                    LiSendKeyboardEvent(event.modifierKeycode, KEY_ACTION_DOWN, event.modifier);
                    usleep(50 * 1000);
                }
                LiSendKeyboardEvent(event.keycode, KEY_ACTION_DOWN, event.modifier);
                usleep(50 * 1000);
                LiSendKeyboardEvent(event.keycode, KEY_ACTION_UP, event.modifier);
                if (event.modifier != 0) {
                    LiSendKeyboardEvent(event.modifierKeycode, KEY_ACTION_UP, event.modifier);
                }
            }
        }
    });
    
    // Reset text field back to known state
    textField.text = @"0";
    
    // Move the insertion point back to the end of the text box
    UITextRange *textRange = [textField textRangeFromPosition:textField.endOfDocument toPosition:textField.endOfDocument];
    [textField setSelectedTextRange:textRange];
}

@end
