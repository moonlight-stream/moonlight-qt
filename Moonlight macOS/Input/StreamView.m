//
//  StreamView.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 10.3.18.
//  Copyright (c) 2018 Felix Kratz. All rights reserved.
//

#import "StreamView.h"
#include <Limelight.h>
#import "DataManager.h"
#include <ApplicationServices/ApplicationServices.h>
#include "keyboardTranslation.h"

@implementation StreamView {
    BOOL isDragging;
    NSTrackingArea *trackingArea;
}

- (void) updateTrackingAreas {
    
    // This will be the area used to track the mouse movement
    if (trackingArea != nil) {
        [self removeTrackingArea:trackingArea];
    }
    NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingInVisibleRect |
                                     NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);
    
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                        options:options
                                                          owner:self
                                                       userInfo:nil];
    [self addTrackingArea:trackingArea];
}

-(void)mouseDragged:(NSEvent *)event {
    if (isDragging) {
        [self mouseMoved:event];
    }
    else {
        [self mouseDown:event];
        isDragging = true;
    }
}

-(void)rightMouseDragged:(NSEvent *)event {
    if (isDragging) {
        [self mouseMoved:event];
    }
    else {
        [self rightMouseDown:event];
        isDragging = true;
    }
}

-(void)scrollWheel:(NSEvent *)event {
    LiSendScrollEvent(event.scrollingDeltaY);
}

- (void)mouseDown:(NSEvent *)mouseEvent {
    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)mouseEvent {
    isDragging = false;
    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
    [self setNeedsDisplay:YES];
}

- (void)rightMouseUp:(NSEvent *)mouseEvent {
    isDragging = false;
    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_RIGHT);
    [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent *)mouseEvent {
    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);
    [self setNeedsDisplay:YES];
}

- (void)mouseMoved:(NSEvent *)mouseEvent {
    LiSendMouseMoveEvent(mouseEvent.deltaX, mouseEvent.deltaY);
}

-(void)keyDown:(NSEvent *)event {
    unsigned char keyChar = keyCharFromKeyCode(event.keyCode);
    printf("DOWN: KeyCode: %hu, keyChar: %d, keyModifier: %lu \n", event.keyCode, keyChar, event.modifierFlags);
    
    LiSendKeyboardEvent(keyChar, KEY_ACTION_DOWN, modifierFlagForKeyModifier(event.modifierFlags));
}

-(void)keyUp:(NSEvent *)event {
    unsigned char keyChar = keyCharFromKeyCode(event.keyCode);
    printf("UP: KeyChar: %d \n‚", keyChar);
    LiSendKeyboardEvent(keyChar, KEY_ACTION_UP, modifierFlagForKeyModifier(event.modifierFlags));
}

- (void)flagsChanged:(NSEvent *)event
{
    unsigned char keyChar = keyCodeFromModifierKey(event.modifierFlags);
    if(keyChar) {
        printf("DOWN: FlagChanged: %hhu \n", keyChar);
        LiSendKeyboardEvent(keyChar, KEY_ACTION_DOWN, 0x00);
    }
    else {
        LiSendKeyboardEvent(58, KEY_ACTION_UP, 0x00);
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}
@end
