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
#import "ConnectionHandler.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

@implementation StreamFrameViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

    [UIApplication sharedApplication].idleTimerDisabled = YES;
    
    VideoDecoderRenderer* renderer = [[VideoDecoderRenderer alloc]initWithView:self.view];

    Connection* conn = [[Connection alloc] initWithHost:[MainFrameViewController getResolvedHost] key:[MainFrameViewController getRiKey] keyId:[MainFrameViewController getRiKeyId] width:1280 height:720 refreshRate:60 renderer:renderer];
    
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:conn];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
