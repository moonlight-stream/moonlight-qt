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
    UIImage *uiImage;
    unsigned char* pixelData;
}

-(id)init
{
    NSLog(@"init");
    return [super init];
}

- (void)awakeFromNib
{
    NSLog(@"awakeFromNib");
    // Initialization code
    width = 1280;
    height = 720;
    bitsPerComponent = 8;
    bytesPerRow = (bitsPerComponent / 8) * width * 4;
    pixelData = malloc(width * height * 4);
    colorSpace = CGColorSpaceCreateDeviceRGB();
    bitmapContext = CGBitmapContextCreate(NULL, 1280, 720, 8, 5120, CGColorSpaceCreateDeviceRGB(), kCGImageAlphaNoneSkipLast);
    image = CGBitmapContextCreateImage(bitmapContext);
    uiImage = [UIImage imageWithCGImage:image];
    //VideoRenderer* renderer = [[VideoRenderer alloc]initWithTarget:self];
    //NSOperationQueue* opQueue = [[NSOperationQueue alloc]init];
    //[opQueue addOperation:renderer];
    [self setNeedsDisplay];
}


// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
    
    NSLog(@"drawRect");
    
    if (!nv_avc_get_rgb_frame(pixelData, width*height*4))
    {
        return;
    }
    
    struct CGContext* context = UIGraphicsGetCurrentContext();
    
    CGContextDrawImage(context, CGRectMake(0, 0, 1280, 720), image);

    [super drawRect:rect];
}


@end
