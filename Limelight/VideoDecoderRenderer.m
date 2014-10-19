//
//  VideoDecoderRenderer.m
//  Limelight
//
//  Created by Cameron Gutman on 10/18/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "VideoDecoderRenderer.h"

@implementation VideoDecoderRenderer

- (id)initWithTarget:(UIView *)target
{
    self = [super init];
    
    self.renderTarget = target;
    
    return self;
}

- (void)main
{
    NSLog(@"Hi");
}

@end
