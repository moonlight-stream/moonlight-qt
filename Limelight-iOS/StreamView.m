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
    NSLog(@"initWithFrame");
    // Initialization code
    width = 1280;
    height = 720;
    bitsPerComponent = 8;
    bytesPerRow = (bitsPerComponent / 8) * width * 4;
    pixelData = malloc(width * height * 4);
    colorSpace = CGColorSpaceCreateDeviceRGB();
    //bitmapContext = CGBitmapContextCreate(pixelData, width, height, bitsPerComponent, bytesPerRow, colorSpace, kCGBitmapByteOrderDefault);
    //image = CGBitmapContextCreateImage(bitmapContext);
    
    return self;
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
    
    if (firstFrame) {
        
        NSData *data = [[NSData alloc] initWithBytes:pixelData length:(width*height*4)];
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        NSString *appFile = [documentsDirectory stringByAppendingPathComponent:@"MyFile"];
        [data writeToFile:appFile atomically:YES];
        NSLog(@"writing data to: %@",documentsDirectory);
        
        firstFrame = false;
    }
    
    bitmapContext = CGBitmapContextCreate(pixelData, width, height, bitsPerComponent, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
    image = CGBitmapContextCreateImage(bitmapContext);
    
    
    struct CGContext* context = UIGraphicsGetCurrentContext();
    
    CGContextTranslateCTM(context, 0, rect.size.width);
    CGContextScaleCTM(context, (float)width / self.frame.size.width, (float)height / -self.frame.size.height);
    
    CGContextDrawImage(context, rect, image);
    
    CGImageRelease(image);
    
    [super drawRect:rect];
}


@end
