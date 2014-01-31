//
//  Connection.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "Connection.h"

#include <Limelight.h>

#include "VideoDecoder.h"
#include "VideoRenderer.h"

@implementation Connection {
    IP_ADDRESS host;
    STREAM_CONFIGURATION streamConfig;
    DECODER_RENDERER_CALLBACKS callbacks;
}

void setup(int width, int height, int fps, void* context, int drFlags)
{
    printf("Setup video\n");
    nv_avc_init(width, height, DISABLE_LOOP_FILTER | FAST_DECODE | FAST_BILINEAR_FILTERING, 2);
}

void submitDecodeUnit(PDECODE_UNIT decodeUnit)
{
    unsigned char* data = (unsigned char*) malloc(decodeUnit->fullLength + nv_avc_get_input_padding_size());
    if (data != NULL) {
        int offset = 0;
        int err;
        
        PLENTRY entry = decodeUnit->bufferList;
        while (entry != NULL) {
            memcpy(&data[offset], entry->data, entry->length);
            offset += entry->length;
            entry = entry->next;
        }
        
        err = nv_avc_decode(data, decodeUnit->fullLength);
        if (err != 0) {
            printf("Decode failed: %d\n", err);
        }
        
        free(data);
    }
}

void start(void)
{
    printf("Start video\n");
    [VideoRenderer startRendering];
}

void stop(void)
{
    printf("Stop video\n");
    [VideoRenderer stopRendering];
}

void release(void)
{
    printf("Release video\n");
    nv_avc_destroy();
}

-(id) initWithHost:(int)ipaddr width:(int)width height:(int)height
{
    self = [super init];
    host = ipaddr;
    
    streamConfig.width = width;
    streamConfig.height = height;
    streamConfig.fps = 30;
    
    callbacks.setup = setup;
    callbacks.start = start;
    callbacks.stop = stop;
    callbacks.release = release;
    callbacks.submitDecodeUnit = submitDecodeUnit;
    
    return self;
}

-(void) main
{
    LiStartConnection(host, &streamConfig, &callbacks, NULL, 0);
}

@end
