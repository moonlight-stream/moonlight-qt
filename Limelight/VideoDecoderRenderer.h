//
//  VideoDecoderRenderer.h
//  Limelight
//
//  Created by Cameron Gutman on 10/18/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@import AVFoundation;

@interface VideoDecoderRenderer : NSObject

- (id)init;

- (void)submitDecodeBuffer:(unsigned char *)data length:(int)length;

@end
