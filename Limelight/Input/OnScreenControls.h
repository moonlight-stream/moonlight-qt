//
//  OnScreenControls.h
//  Moonlight
//
//  Created by Diego Waxemberg on 12/28/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "StreamView.h"

@class ControllerSupport;

@interface OnScreenControls : NSObject

typedef NS_ENUM(NSInteger, OnScreenControlsLevel) {
    OnScreenControlsLevelOff,
    OnScreenControlsLevelAuto,
    OnScreenControlsLevelSimple,
    OnScreenControlsLevelFull,
    
    // Internal levels selected by ControllerSupport
    OnScreenControlsLevelAutoGCGamepad,
    OnScreenControlsLevelAutoGCExtendedGamepad,
};

#if TARGET_OS_IPHONE
- (id) initWithView:(UIView*)view controllerSup:(ControllerSupport*)controllerSupport swipeDelegate:(id<EdgeDetectionDelegate>)edgeDelegate;
- (BOOL) handleTouchDownEvent:(NSSet*)touches;
- (BOOL) handleTouchUpEvent:(NSSet*)touches;
- (BOOL) handleTouchMovedEvent:(NSSet*)touches;
- (void) setLevel:(OnScreenControlsLevel)level;
#endif

@end
