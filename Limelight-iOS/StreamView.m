//
//  StreamView.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "StreamView.h"
#import "VideoDecoder.h"
#import "VideoRenderer.h"

@implementation StreamView {
    size_t width;
    size_t height;
    size_t bitsPerComponent;
    size_t bytesPerRow;
    CGColorSpaceRef colorSpace;
    CGContextRef bitmapContext;
    CGImageRef image;
    unsigned char* pixelData;
}
static bool firstFrame = true;



- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];

    // Initialization code

    width = 1280;
    height = 720;
    bitsPerComponent = 8;
    bytesPerRow = (bitsPerComponent / 8) * width * 4;
    pixelData = malloc(width * height * 4);
    colorSpace = CGColorSpaceCreateDeviceRGB();

    return self;
}



// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
    
    if (!nv_avc_get_rgb_frame(pixelData, width*height*4))
    {
        NSLog(@"failed to decode frame!");
        return;
    }
    
    bitmapContext = CGBitmapContextCreate(pixelData, width, height, bitsPerComponent, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
    image = CGBitmapContextCreateImage(bitmapContext);
    
    ;
    struct CGContext* context = UIGraphicsGetCurrentContext();

    CGContextRotateCTM(context, -M_PI_2);
    CGContextScaleCTM(context, -(float)self.frame.size.width/self.frame.size.height, (float)self.frame.size.height/self.frame.size.width);
    CGContextDrawImage(context, rect, image);
    
    CGImageRelease(image);
    
    [super drawRect:rect];
}


@end
