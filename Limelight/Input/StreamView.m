//
//  StreamView.m
//  Limelight
//
//  Created by Cameron Gutman on 10/19/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "StreamView.h"
#include <Limelight.h>
#import "OnScreenControls.h"
#import "DataManager.h"

@implementation StreamView {
    CGPoint touchLocation;
    BOOL touchMoved;
    OnScreenControls* onScreenControls;
}

- (void) setupOnScreenControls {
    onScreenControls = [[OnScreenControls alloc] initWithView:self];
    DataManager* dataMan = [[DataManager alloc] init];
    OnScreenControlsLevel level = (OnScreenControlsLevel)[[dataMan retrieveSettings].onscreenControls integerValue];
    NSLog(@"Setting on-screen controls level: %d", (int)level);
    [onScreenControls setLevel:level];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    NSLog(@"Touch down");
    if (![onScreenControls handleTouchDownEvent:event]) {
        UITouch *touch = [[event allTouches] anyObject];
        touchLocation = [touch locationInView:self];
        touchMoved = false;
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
    if (![onScreenControls handleTouchMovedEvent:event]) {
        if ([[event allTouches] count] == 1) {
            UITouch *touch = [[event allTouches] anyObject];
            CGPoint currentLocation = [touch locationInView:self];
            
            if (touchLocation.x != currentLocation.x ||
                touchLocation.y != currentLocation.y)
            {
                LiSendMouseMoveEvent(currentLocation.x - touchLocation.x,
                                     currentLocation.y - touchLocation.y );

                touchMoved = true;
                touchLocation = currentLocation;
            }
        } else if ([[event allTouches] count] == 2) {
            CGPoint firstLocation = [[[[event allTouches] allObjects] objectAtIndex:0] locationInView:self];
            CGPoint secondLocation = [[[[event allTouches] allObjects] objectAtIndex:1] locationInView:self];

            CGPoint avgLocation = CGPointMake((firstLocation.x + secondLocation.x) / 2, (firstLocation.y + secondLocation.y) / 2);
            if (touchLocation.y != avgLocation.y) {
                LiSendScrollEvent(avgLocation.y - touchLocation.y);
            }
            touchMoved = true;
            touchLocation = avgLocation;
        }
    }
    
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    NSLog(@"Touch up");
    if (![onScreenControls handleTouchUpEvent:event]) {
        if (!touchMoved) {
            if ([[event allTouches] count]  == 2) {
                NSLog(@"Sending right mouse button press");

                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);

                // Wait 100 ms to simulate a real button press
                usleep(100 * 1000);

                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_RIGHT);


            } else {
                NSLog(@"Sending left mouse button press");

                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);

                // Wait 100 ms to simulate a real button press
                usleep(100 * 1000);

                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
            }
        }
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
}


@end
