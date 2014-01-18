//
//  VideoDepacketizer.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VideoDecoder.h"

@interface VideoDepacketizer : NSObject <NSStreamDelegate>
@property uint8_t* byteBuffer;
@property unsigned int offset;
@property VideoDecoder* decoder;

- (void) readFile:(NSString*) file;

@end
