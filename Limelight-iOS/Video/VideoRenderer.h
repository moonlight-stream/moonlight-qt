//
//  VideoRenderer.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VideoDecoder.h"

@interface VideoRenderer : NSOperation
@property UIView* renderTarget;
@property VideoDecoder* decoder;

- (id) initWithTarget:(UIView*)renderTarget;
@end
