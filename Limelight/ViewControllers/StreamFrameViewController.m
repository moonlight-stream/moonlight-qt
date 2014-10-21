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
    
    _streamMan = [[StreamManager alloc] initWithConfig:[MainFrameViewController getStreamConfiguration]
                                            renderView:self.view
                                   connectionCallbacks:self];
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

- (void) connectionStarted {
    printf("Connection started\n");
}

- (void)connectionTerminated:(long)errorCode {
    printf("Connection terminated: %ld\n", errorCode);
    
    UIAlertController* conTermAlert = [UIAlertController alertControllerWithTitle:@"Connection Terminated" message:@"The connection terminated unexpectedly" preferredStyle:UIAlertControllerStyleAlert];
    [conTermAlert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
        [self performSegueWithIdentifier:@"returnToMainFrame" sender:self];
    }]];
    [self presentViewController:conTermAlert animated:YES completion:nil];
    
    [_streamMan stopStream];
}

- (void) stageStarting:(char*)stageName {
    printf("Starting %s\n", stageName);
}

- (void) stageComplete:(char*)stageName {
}

- (void) stageFailed:(char*)stageName withError:(long)errorCode {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                   message:[NSString stringWithFormat:@"%s failed with error %ld",
                                                                             stageName, errorCode]
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
        [self performSegueWithIdentifier:@"returnToMainFrame" sender:self];
    }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void) launchFailed {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                   message:@"Failed to start app"
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
        [self performSegueWithIdentifier:@"returnToMainFrame" sender:self];
    }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void) displayMessage:(char*)message {
    printf("Display message: %s\n", message);
}

- (void) displayTransientMessage:(char*)message {
    printf("Display transient message: %s\n", message);
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
    return UIInterfaceOrientationLandscapeRight;
}

@end
