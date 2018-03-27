//
//  SettingsViewController.h
//  Moonlight macOS
//
//  Created by Felix Kratz on 11.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface SettingsViewController : NSViewController
@property (weak) IBOutlet NSTextField *textFieldResolutionHeight;
@property (weak) IBOutlet NSTextField *textFieldResolutionWidth;
@property (weak) IBOutlet NSTextField *textFieldBitrate;
@property (weak) IBOutlet NSTextField *textFieldFPS;
@property (weak) IBOutlet NSButton *buttonStreamingRemotelyToggle;
- (NSString*) getCurrentHost;
- (NSInteger) getChosenBitrate;
- (NSInteger) getChosenStreamWidth;
- (NSInteger) getChosenStreamHeight;
- (NSInteger) getChosenFrameRate;
- (NSInteger) getRemoteOptions;
@end
