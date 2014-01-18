//
//  VideoDecoder.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "VideoDecoder.h"

@implementation VideoDecoder
- (void)decode:(uint8_t *)buffer length:(unsigned int)length
{
    for (int i = 0; i < length; i++) {
        printf("%02x ", buffer[i]);
        if (i != 0 && i % 16 == 0) {
            NSLog(@"");
        }
    }
    
    
}
@end
