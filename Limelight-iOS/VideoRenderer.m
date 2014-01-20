//
//  VideoRenderer.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "VideoRenderer.h"

@implementation VideoRenderer
static bool render = false;

- (id)initWithTarget:(UIView *)target
{
    self = [super init];
    
    self.renderTarget = target;
    
    return self;
}

- (void)main
{
    while (true)
    {
        if (render)
        {
            [self.renderTarget performSelectorOnMainThread:@selector(setNeedsDisplay) withObject:NULL waitUntilDone:TRUE];
            usleep(5000);
        } else {
            sleep(1);
        }
    }
}

+ (void) startRendering
{
    render = true;
}

+ (void) stopRendering
{
    render = false;
}

+ (BOOL) isRendering
{
    return render;
}
@end
