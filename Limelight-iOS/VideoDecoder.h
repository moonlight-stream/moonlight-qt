//
//  VideoDecoder.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface VideoDecoder : NSObject
- (void) decode:(uint8_t*)buffer length:(unsigned int)length;
@end
