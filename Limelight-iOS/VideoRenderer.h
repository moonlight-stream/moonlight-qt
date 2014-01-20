//
//  VideoRenderer.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface VideoRenderer : NSOperation
@property UIView* renderTarget;
- (id) initWithTarget:(UIView*)target;
+ (void) startRendering;
+ (void) stopRendering;
+ (BOOL) isRendering;
@end
