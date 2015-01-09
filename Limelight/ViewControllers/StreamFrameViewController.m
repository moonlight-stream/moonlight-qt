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
#import "StreamView.h"

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

- (void) returnToMainFrame {
    [self.navigationController popToRootViewControllerAnimated:YES];
}

- (void)applicationWillResignActive:(NSNotification *)notification {
    [_streamMan stopStream];
    [self returnToMainFrame];
}

- (void) connectionStarted {
    NSLog(@"Connection started");
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.spinner stopAnimating];
        [self.stageLabel setText:@"Waiting for first frame..."];
        [self.stageLabel sizeToFit];
    });
    [(StreamView*)self.view setupOnScreenControls];
}

- (void)connectionTerminated:(long)errorCode {
    printf("Connection terminated: %ld\n", errorCode);
    
    UIAlertController* conTermAlert = [UIAlertController alertControllerWithTitle:@"Connection Terminated" message:@"The connection was terminated" preferredStyle:UIAlertControllerStyleAlert];
    [conTermAlert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
        [self returnToMainFrame];
    }]];
    [self presentViewController:conTermAlert animated:YES completion:nil];
    
    [_streamMan stopStream];
}

- (void) stageStarting:(char*)stageName {
    printf("Starting %s\n", stageName);
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
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                   message:[NSString stringWithFormat:@"%s failed with error %ld",
                                                                            stageName, errorCode]
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
        [self returnToMainFrame];
    }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void) launchFailed:(NSString*)message {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
        [self returnToMainFrame];
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
@end
