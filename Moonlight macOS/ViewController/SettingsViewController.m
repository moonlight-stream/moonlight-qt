//
//  SettingsViewController.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 11.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import "SettingsViewController.h"
#import "DataManager.h"

@interface SettingsViewController ()

@end

@implementation SettingsViewController

NSString* host;

- (void)viewDidLoad {
    [super viewDidLoad];
    [self loadSettings];
    // Do view setup here.
}

- (void) controlTextDidChange:(NSNotification *)obj {
    //[self saveSettings];
}

- (NSInteger) getRemoteOptions {
    return _buttonStreamingRemotelyToggle.state == NSOnState ? 1 : 0;
}

- (NSInteger) getChosenFrameRate {
    return _textFieldFPS.integerValue;
}

- (NSInteger) getChosenStreamHeight {
    return _textFieldResolutionHeight.integerValue;
}

- (NSInteger) getChosenStreamWidth {
    return _textFieldResolutionWidth.integerValue;
}

- (NSInteger) getChosenBitrate {
    return _textFieldBitrate.integerValue;
}

- (void) loadSettings {
    DataManager* dataMan = [[DataManager alloc] init];
    TemporarySettings* currentSettings = [dataMan getSettings];
    
    // Bitrate is persisted in kbps
    _textFieldBitrate.integerValue = [currentSettings.bitrate integerValue];
    _textFieldFPS.integerValue = [currentSettings.framerate integerValue];
    _textFieldResolutionHeight.integerValue = [currentSettings.height integerValue];
    _textFieldResolutionWidth.integerValue = [currentSettings.width integerValue];
    _buttonStreamingRemotelyToggle.state = [currentSettings.streamingRemotely integerValue] == 0 ? NSOffState: NSOnState;
}

- (void)didReceiveMemoryWarning {
    // Dispose of any resources that can be recreated.
}

-(NSString*) getCurrentHost {
    return host;
}


@end
