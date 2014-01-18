//
//  VideoRenderer.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "VideoRenderer.h"


@implementation VideoRenderer

- (void)main
{
    while (true)
    {
        [self.renderTarget drawRect:CGRectMake(0, 0, 0, 0)];
    }
}

- (id) initWithTarget:(UIView *)renderTarget
{
    self = [super init];
    self.renderTarget = renderTarget;
    return self;
}

@end
