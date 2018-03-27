//
//  VideoDecoderRenderer.h
//  Moonlight
//
//  Created by Cameron Gutman on 10/18/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@import AVFoundation;

@interface VideoDecoderRenderer : NSObject
#if TARGET_OS_IPHONE
- (id)initWithView:(UIView*)view;
#else
- (id)initWithView:(NSView*)view;
#endif

- (void)setupWithVideoFormat:(int)videoFormat;

- (void)updateBufferForRange:(CMBlockBufferRef)existingBuffer data:(unsigned char *)data offset:(int)offset length:(int)nalLength;

- (int)submitDecodeBuffer:(unsigned char *)data length:(int)length bufferType:(int)bufferType;

@end
