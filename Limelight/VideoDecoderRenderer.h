//
//  VideoDecoderRenderer.h
//  Limelight
//
//  Created by Cameron Gutman on 10/18/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface VideoDecoderRenderer : NSOperation

- (id)initWithTarget:(UIView *)target;

@property UIView* renderTarget;

@end
