//
//  StreamFrameViewController.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "StreamFrameViewController.h"
#import "MainFrameViewController.h"
#import "VideoDecoderRenderer.h"
#import "StreamManager.h"
#import "ControllerSupport.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

@implementation StreamFrameViewController {
    ControllerSupport *_controllerSupport;
    StreamManager *_streamMan;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    [UIApplication sharedApplication].idleTimerDisabled = YES;
    
    _controllerSupport = [[ControllerSupport alloc] init];
    
    _streamMan = [[StreamManager alloc] initWithConfig:[MainFrameViewController getStreamConfiguration] renderView:self.view connectionTerminatedCallback:self];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:_streamMan];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillResignActive:)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
}

- (void)applicationWillResignActive:(NSNotification *)notification {
    [_streamMan stopStream];
    [self performSegueWithIdentifier:@"returnToMainFrame" sender:self];
}

- (void)connectionTerminated {
    UIAlertController* conTermAlert = [UIAlertController alertControllerWithTitle:@"Connection Terminated" message:@"The connection terminated unexpectedly" preferredStyle:UIAlertControllerStyleAlert];
    [conTermAlert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
        [self performSegueWithIdentifier:@"returnToMainFrame" sender:self];
    }]];
    [self presentViewController:conTermAlert animated:YES completion:nil];
    
    [_streamMan stopStream];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (BOOL)shouldAutorotate {
    return YES;
}

- (NSUInteger)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscapeLeft | UIInterfaceOrientationMaskLandscapeRight;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    return UIInterfaceOrientationLandscapeLeft;
}

@end
