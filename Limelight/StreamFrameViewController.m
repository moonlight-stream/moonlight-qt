//
//  StreamFrameViewController.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "StreamFrameViewController.h"
#import "MainFrameViewController.h"
#import "Connection.h"
#import "VideoDecoderRenderer.h"
#import "StreamManager.h"
#import "ControllerSupport.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

@implementation StreamFrameViewController {
    ControllerSupport *_controllerSupport;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    [UIApplication sharedApplication].idleTimerDisabled = YES;
    
    _controllerSupport = [[ControllerSupport alloc] init];
    
    StreamManager* streamMan = [[StreamManager alloc] initWithConfig:[MainFrameViewController getStreamConfiguration] renderView:self.view];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:streamMan];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
