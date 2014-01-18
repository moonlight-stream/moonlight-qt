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
int nv_avc_init(int width, int height, int perf_lvl, int thread_count);
void nv_avc_destroy(void);

int nv_avc_get_raw_frame(char* buffer, int size);

int nv_avc_get_rgb_frame(char* buffer, int size);
int nv_avc_get_rgb_frame_int(int* buffer, int size);
int nv_avc_redraw(void);

int nv_avc_get_input_padding_size(void);
int nv_avc_decode(unsigned char* indata, int inlen);
@end
