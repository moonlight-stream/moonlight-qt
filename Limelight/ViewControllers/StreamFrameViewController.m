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
    NSTimer *_inactivityTimer;
    UITapGestureRecognizer *_menuGestureRecognizer;
    UITapGestureRecognizer *_menuDoubleTapGestureRecognizer;
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    
#if !TARGET_OS_TV
    [[self revealViewController] setPrimaryViewController:self];
#endif
}

#ifdef TARGET_OS_TV
- (void)controllerPauseButtonPressed:(id)sender { }
- (void)controllerPauseButtonDoublePressed:(id)sender {
    Log(LOG_I, @"Menu double-pressed -- backing out of stream");
    [self returnToMainFrame];
}
#endif


- (void)viewDidLoad
{
    [super viewDidLoad];
    
    [self.navigationController setNavigationBarHidden:YES animated:YES];
    
    [self.stageLabel setText:[NSString stringWithFormat:@"Starting %@...", self.streamConfig.appName]];
    [self.stageLabel sizeToFit];
    self.stageLabel.textAlignment = NSTextAlignmentCenter;
    self.stageLabel.center = CGPointMake(self.view.frame.size.width / 2, self.view.frame.size.height / 2);
    self.spinner.center = CGPointMake(self.view.frame.size.width / 2, self.view.frame.size.height / 2 - self.stageLabel.frame.size.height - self.spinner.frame.size.height);
    [UIApplication sharedApplication].idleTimerDisabled = YES;
    
    _controllerSupport = [[ControllerSupport alloc] initWithConfig:self.streamConfig];
    _inactivityTimer = nil;
#if TARGET_OS_TV
    if (!_menuGestureRecognizer || !_menuDoubleTapGestureRecognizer) {
        _menuGestureRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(controllerPauseButtonPressed:)];
        _menuGestureRecognizer.allowedPressTypes = @[@(UIPressTypeMenu)];

        _menuDoubleTapGestureRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(controllerPauseButtonDoublePressed:)];
        _menuDoubleTapGestureRecognizer.numberOfTapsRequired = 2;
        [_menuGestureRecognizer requireGestureRecognizerToFail:_menuDoubleTapGestureRecognizer];
        _menuDoubleTapGestureRecognizer.allowedPressTypes = @[@(UIPressTypeMenu)];
    }
    
    [self.view addGestureRecognizer:_menuGestureRecognizer];
    [self.view addGestureRecognizer:_menuDoubleTapGestureRecognizer];
#endif
    
#if TARGET_OS_TV
    [self.tipLabel setText:@"Tip: Double tap the Menu button to disconnect from your PC"];
#else
    [self.tipLabel setText:@"Tip: Swipe from the left edge to disconnect from your PC"];
#endif
    
    [self.tipLabel sizeToFit];
    self.tipLabel.textAlignment = NSTextAlignmentCenter;
    self.tipLabel.center = CGPointMake(self.view.frame.size.width / 2, self.tipLabel.center.y);
    
    _streamMan = [[StreamManager alloc] initWithConfig:self.streamConfig
                                            renderView:self.view
                                   connectionCallbacks:self];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:_streamMan];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillResignActive:)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(applicationDidBecomeActive:)
                                                 name: UIApplicationDidBecomeActiveNotification
                                               object: nil];
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(applicationDidEnterBackground:)
                                                 name: UIApplicationDidEnterBackgroundNotification
                                               object: nil];
}

- (void)willMoveToParentViewController:(UIViewController *)parent {
    [_controllerSupport cleanup];
    [UIApplication sharedApplication].idleTimerDisabled = NO;
    [_streamMan stopStream];
    if (_inactivityTimer != nil) {
        [_inactivityTimer invalidate];
        _inactivityTimer = nil;
    }
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void) returnToMainFrame {
    [self.navigationController popToRootViewControllerAnimated:YES];
}

// This will fire if the user opens control center or gets a low battery message
- (void)applicationWillResignActive:(NSNotification *)notification {
    if (_inactivityTimer != nil) {
        [_inactivityTimer invalidate];
    }
    
    // Terminate the stream if the app is inactive for 10 seconds
    Log(LOG_I, @"Starting inactivity termination timer");
    _inactivityTimer = [NSTimer scheduledTimerWithTimeInterval:10
                                                      target:self
                                                    selector:@selector(inactiveTimerExpired:)
                                                    userInfo:nil
                                                     repeats:NO];
}

- (void)inactiveTimerExpired:(NSTimer*)timer {
    Log(LOG_I, @"Terminating stream after inactivity");

    [self returnToMainFrame];
    
    _inactivityTimer = nil;
}

- (void)applicationDidBecomeActive:(NSNotification *)notification {
    // Stop the background timer, since we're foregrounded again
    if (_inactivityTimer != nil) {
        Log(LOG_I, @"Stopping inactivity timer after becoming active again");
        [_inactivityTimer invalidate];
        _inactivityTimer = nil;
    }
}

// This fires when the home button is pressed
- (void)applicationDidEnterBackground:(UIApplication *)application {
    Log(LOG_I, @"Terminating stream immediately for backgrounding");

    if (_inactivityTimer != nil) {
        [_inactivityTimer invalidate];
        _inactivityTimer = nil;
    }
    
    [self returnToMainFrame];
}

- (void)edgeSwiped {
    Log(LOG_I, @"User swiped to end stream");
    
    [self returnToMainFrame];
}

- (void) connectionStarted {
    Log(LOG_I, @"Connection started");
    dispatch_async(dispatch_get_main_queue(), ^{
        // Leave the spinner spinning until it's obscured by
        // the first frame of video.
        self.stageLabel.hidden = YES;
        self.tipLabel.hidden = YES;
#if !TARGET_OS_TV
        [(StreamView*)self.view setupOnScreenControls: self->_controllerSupport swipeDelegate:self];
#endif
    });
}

- (void)connectionTerminated:(long)errorCode {
    Log(LOG_I, @"Connection terminated: %ld", errorCode);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        // Allow the display to go to sleep now
        [UIApplication sharedApplication].idleTimerDisabled = NO;
        
        UIAlertController* conTermAlert = [UIAlertController alertControllerWithTitle:@"Connection Terminated" message:@"The connection was terminated" preferredStyle:UIAlertControllerStyleAlert];
        [conTermAlert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
            [self returnToMainFrame];
        }]];
        [self presentViewController:conTermAlert animated:YES completion:nil];
    });

    [_streamMan stopStream];
}

- (void) stageStarting:(const char*)stageName {
    Log(LOG_I, @"Starting %s", stageName);
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString* lowerCase = [NSString stringWithFormat:@"%s in progress...", stageName];
        NSString* titleCase = [[[lowerCase substringToIndex:1] uppercaseString] stringByAppendingString:[lowerCase substringFromIndex:1]];
        [self.stageLabel setText:titleCase];
        [self.stageLabel sizeToFit];
        self.stageLabel.center = CGPointMake(self.view.frame.size.width / 2, self.stageLabel.center.y);
    });
}

- (void) stageComplete:(const char*)stageName {
}

- (void) stageFailed:(const char*)stageName withError:(long)errorCode {
    Log(LOG_I, @"Stage %s failed: %ld", stageName, errorCode);

    dispatch_async(dispatch_get_main_queue(), ^{
        // Allow the display to go to sleep now
        [UIApplication sharedApplication].idleTimerDisabled = NO;
        
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                       message:[NSString stringWithFormat:@"%s failed with error %ld",
                                                                                stageName, errorCode]
                                                                preferredStyle:UIAlertControllerStyleAlert];
        [Utils addHelpOptionToDialog:alert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
            [self returnToMainFrame];
        }]];
        [self presentViewController:alert animated:YES completion:nil];
    });
    
    [_streamMan stopStream];
}

- (void) launchFailed:(NSString*)message {
    Log(LOG_I, @"Launch failed: %@", message);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        // Allow the display to go to sleep now
        [UIApplication sharedApplication].idleTimerDisabled = NO;
        
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Connection Failed"
                                                                       message:message
                                                                preferredStyle:UIAlertControllerStyleAlert];
        [Utils addHelpOptionToDialog:alert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
            [self returnToMainFrame];
        }]];
        [self presentViewController:alert animated:YES completion:nil];
    });
}

- (void) displayMessage:(const char*)message {
    Log(LOG_I, @"Display message: %s", message);
}

- (void) displayTransientMessage:(const char*)message {
    Log(LOG_I, @"Display transient message: %s", message);
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

#if !TARGET_OS_TV
// Require a confirmation when streaming to activate a system gesture
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return UIRectEdgeAll;
}

- (BOOL)shouldAutorotate {
    return YES;
}
#endif

@end
