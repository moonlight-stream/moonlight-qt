//
//  VideoDecoder.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>

// Disables the deblocking filter at the cost of image quality
#define DISABLE_LOOP_FILTER         0x1
// Uses the low latency decode flag (disables multithreading)
#define LOW_LATENCY_DECODE      0x2
// Threads process each slice, rather than each frame
#define SLICE_THREADING             0x4
// Uses nonstandard speedup tricks
#define FAST_DECODE             0x8
// Uses bilinear filtering instead of bicubic
#define BILINEAR_FILTERING      0x10
// Uses a faster bilinear filtering with lower image quality
#define FAST_BILINEAR_FILTERING 0x20
// Disables color conversion (output is NV21)
#define NO_COLOR_CONVERSION     0x40
// Native color format: RGB0
#define NATIVE_COLOR_RGB0       0x80
// Native color format: 0RGB
#define NATIVE_COLOR_0RGB       0x100
// Native color format: ARGB
#define NATIVE_COLOR_ARGB       0x200
// Native color format: RGBA
#define NATIVE_COLOR_RGBA       0x400

@interface VideoDecoder : NSObject
int nv_avc_init(int width, int height, int perf_lvl, int thread_count);
void nv_avc_destroy(void);

int nv_avc_get_raw_frame(char* buffer, int size);

int nv_avc_get_rgb_frame(char* buffer, int size);
int nv_avc_get_rgb_frame_int(int* buffer, int size);
int nv_avc_redraw(void);

int nv_avc_get_input_padding_size(void);
int nv_avc_decode(unsigned char* indata, int inlen);
@end
