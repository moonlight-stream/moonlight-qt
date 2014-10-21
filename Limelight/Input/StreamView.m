//
//  StreamView.m
//  Limelight
//
//  Created by Cameron Gutman on 10/19/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "StreamView.h"
#include <Limelight.h>

@implementation StreamView {
    CGPoint touchLocation;
    BOOL touchMoved;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [[event allTouches] anyObject];
    touchLocation = [touch locationInView:self];
    touchMoved = false;
    
    NSLog(@"Touch down");
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
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
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    NSLog(@"Touch up");
    
    if (!touchMoved) {
        NSLog(@"Sending left mouse button press");
        
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
        
        // Wait 100 ms to simulate a real button press
        usleep(100 * 1000);
        
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
}


@end
