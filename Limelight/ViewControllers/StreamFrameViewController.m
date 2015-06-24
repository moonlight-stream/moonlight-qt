//
//  StreamFrameViewController.m
//  Moonlight
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
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
    
    [self.navigationController setNavigationBarHidden:YES animated:YES];
    
    [self.stageLabel setText:@"Starting App"];
    [self.stageLabel sizeToFit];
    self.stageLabel.center = CGPointMake(self.view.frame.size.width / 2, self.view.frame.size.height / 2);
    self.spinner.center = CGPointMake(self.view.frame.size.width / 2, self.view.frame.size.height / 2 - self.stageLabel.frame.size.height - self.spinner.frame.size.height);
    [UIApplication sharedApplication].idleTimerDisabled = YES;
    
    _controllerSupport = [[ControllerSupport alloc] init];
    
    _streamMan = [[StreamManager alloc] initWithConfig:self.streamConfig
                                            renderView:self.view
                                   connectionCallbacks:self];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:_streamMan];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillResignActive:)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
}

- (void) returnToMainFrame {
    [_controllerSupport cleanup];
    [self.navigationController popToRootViewControllerAnimated:YES];
}

- (void)applicationWillResignActive:(NSNotification *)notification {
    [_streamMan stopStream];
    [self returnToMainFrame];
}

- (void)edgeSwiped {
    Log(LOG_D, @"User swiped to end stream");
    [_streamMan stopStream];
    [self returnToMainFrame];
}

- (void) connectionStarted {
    Log(LOG_I, @"Connection started");
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.spinner stopAnimating];
        [self.stageLabel setText:@"Waiting for first frame..."];
        [self.stageLabel sizeToFit];
        [(StreamView*)self.view setupOnScreenControls: _controllerSupport swipeDelegate:self];
    });
}

- (void)connectionTerminated:(long)errorCode {
    Log(LOG_I, @"Connection terminated: %ld", errorCode);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        UIAlertController* conTermAlert = [UIAlertController alertControllerWithTitle:@"Connection Terminated" message:@"The connection was terminated" preferredStyle:UIAlertControllerStyleAlert];
        [conTermAlert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
            [self returnToMainFrame];
        }]];
        [self presentViewController:conTermAlert animated:YES completion:nil];
    });

    [_streamMan stopStreamInternal];
}

- (void) stageStarting:(char*)stageName {
    Log(LOG_I, @"Starting %s", stageName);
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString* lowerCase = [NSString stringWithFormat:@"%s in progress...", stageName];
        NSString* titleCase = [[[lowerCase substringToIndex:1] uppercaseString] stringByAppendingString:[lowerCase substringFromIndex:1]];
        [self.stageLabel setText:titleCase];
        [self.stageLabel sizeToFit];
        self.stageLabel.center = CGPointMake(self.view.frame.size.width / 2, self.stageLabel.center.y);
    });
}

- (void) stageComplete:(char*)stageName {
}

- (void) stageFailed:(char*)stageName withError:(long)errorCode {
    Log(LOG_I, @"Stage %s failed: %ld", stageName, errorCode);

    dispatch_async(dispatch_get_main_queue(), ^{
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                       message:[NSString stringWithFormat:@"%s failed with error %ld",
                                                                                stageName, errorCode]
                                                                preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
            [self returnToMainFrame];
        }]];
        [self presentViewController:alert animated:YES completion:nil];
    });
}

- (void) launchFailed:(NSString*)message {
    Log(LOG_I, @"Launch failed: %@", message);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                       message:message
                                                                preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
            [self returnToMainFrame];
        }]];
        [self presentViewController:alert animated:YES completion:nil];
    });
}

- (void) displayMessage:(char*)message {
    Log(LOG_I, @"Display message: %s", message);
}

- (void) displayTransientMessage:(char*)message {
    Log(LOG_I, @"Display transient message: %s", message);
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (BOOL)shouldAutorotate {
    return YES;
}
@end
