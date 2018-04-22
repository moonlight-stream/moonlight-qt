//
//  ViewController.h
//  Moonlight macOS
//
//  Created by Felix Kratz on 09.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PairManager.h"
#import "StreamConfiguration.h"

@interface ViewController : NSViewController <PairCallback, NSURLConnectionDelegate>

- (IBAction)buttonLaunchPressed:(id)sender;
- (IBAction)textFieldAction:(id)sender;
- (IBAction)buttonConnectPressed:(id)sender;
- (IBAction)buttonSettingsPressed:(id)sender;
- (IBAction)popupButtonSelectionPressed:(id)sender;

- (void)setError:(long)errorCode;
- (long) error;

@property (weak) IBOutlet NSLayoutConstraint *layoutConstraintSetupFrame;
@property (weak) IBOutlet NSButton *buttonConnect;
@property (weak) IBOutlet NSTextField *textFieldHost;
@property (weak) IBOutlet NSButton *buttonSettings;
@property (weak) IBOutlet NSButton *buttonLaunch;
@property (weak) IBOutlet NSPopUpButton *popupButtonSelection;
@property (weak) IBOutlet NSView *containerViewController;


@end

