//
//  OnScreenControls.h
//  Limelight
//
//  Created by Diego Waxemberg on 12/28/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface OnScreenControls : NSObject

typedef NS_ENUM(NSInteger, OnScreenControlsLevel) {
    OnScreenControlsLevelOff,
    OnScreenControlsLevelSimple,
    OnScreenControlsLevelFull
};

- (id) initWithView:(UIView*)view;
- (BOOL) handleTouchDownEvent:(UIEvent*)event;
- (BOOL) handleTouchUpEvent:(UIEvent*) event;
- (BOOL) handleTouchMovedEvent:(UIEvent*)event;
- (void) setLevel:(OnScreenControlsLevel)level;

@end
