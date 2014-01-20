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
    nv_avc_init(width, height, 0, 2);
}

void start(void)
{
    [VideoRenderer startRendering];
}

void stop(void)
{
    [VideoRenderer stopRendering];
}

void release(void)
{
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
    //callbacks.submitDecodeUnit = submitDecodeUnit;
    
    return self;
}

-(void) main
{
    int err;
    
    NSLog(@"Starting connection process\n");
    
    err = performHandshake(host);
    NSLog(@"Handshake: %d\n", err);
    
    err = initializeControlStream(host, &streamConfig);
    NSLog(@"Control stream init: %d\n", err);
    
    initializeVideoStream(host, &streamConfig, &callbacks);
    
    err = startControlStream();
    NSLog(@"Control stream start: %d\n", err);
    
    err = startVideoStream(NULL, 0);
    NSLog(@"Video stream start: %d\n", err);
}

@end
