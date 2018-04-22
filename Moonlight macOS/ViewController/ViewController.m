//
//  ViewController.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 09.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import "ViewController.h"

#import "CryptoManager.h"
#import "HttpManager.h"
#import "Connection.h"
#import "StreamManager.h"
#import "Utils.h"
#import "DataManager.h"
#import "TemporarySettings.h"
#import "WakeOnLanManager.h"
#import "AppListResponse.h"
#import "ServerInfoResponse.h"
#import "StreamFrameViewController.h"
#import "TemporaryApp.h"
#import "IdManager.h"
#import "SettingsViewController.h"
#import "ConnectionHelper.h"

@implementation ViewController{
    NSOperationQueue* _opQueue;
    TemporaryHost* _selectedHost;
    NSString* _uniqueId;
    NSData* _cert;
    StreamConfiguration* _streamConfig;
    NSArray* _sortedAppList;
    NSSet* _appList;
    NSString* _host;
    SettingsViewController *_settingsView;
    CGFloat settingsFrameHeight;
    bool showSettings;
    NSAlert* _alert;
    long error;
}

- (long)error {
    return error;
}

- (void)setError:(long)errorCode {
    error = errorCode;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    [CryptoManager generateKeyPairUsingSSl];
    _uniqueId = [IdManager getUniqueId];
    _cert = [CryptoManager readCertFromFile];
    
    _opQueue = [[NSOperationQueue alloc] init];
    
    // Do any additional setup after loading the view.
}

- (void)viewWillAppear {
    [super viewWillAppear];
    if ([[[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"]  isEqual: @"Dark"]) {
        [self.view.window setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameVibrantDark]];
    }
}

- (void)viewDidAppear {
    [super viewDidAppear];
    
    if (self.view.bounds.size.height == NSScreen.mainScreen.frame.size.height && self.view.bounds.size.width == NSScreen.mainScreen.frame.size.width)
    {
        [self.view.window toggleFullScreen:self];
        [self.view.window setStyleMask:[self.view.window styleMask] & ~NSWindowStyleMaskResizable];
        return;
    }
    [_buttonLaunch setEnabled:false];
    [_popupButtonSelection removeAllItems];
    _settingsView = [self.childViewControllers lastObject];
    
    if (_settingsView.getCurrentHost != nil)
        _textFieldHost.stringValue = _settingsView.getCurrentHost;
    settingsFrameHeight = _layoutConstraintSetupFrame.constant;
    _layoutConstraintSetupFrame.constant = 0;
    showSettings = false;
    
    if (error != 0) {
        [self showAlert:[NSString stringWithFormat: @"The connection terminated."]];
    }
}

-(void) showAlert:(NSString*) message {
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_alert = [NSAlert new];
        self->_alert.messageText = message;
        [self->_alert beginSheetModalForWindow:[self.view window] completionHandler:^(NSInteger result) {
            NSLog(@"Success");
        }];
    });
}


- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}

- (void) saveSettings {
    DataManager* dataMan = [[DataManager alloc] init];
    NSInteger framerate = [_settingsView getChosenFrameRate];
    NSInteger height = [_settingsView getChosenStreamHeight];
    NSInteger width = [_settingsView getChosenStreamWidth];
    NSInteger streamingRemotely = [_settingsView getRemoteOptions];
    NSInteger bitrate = [_settingsView getChosenBitrate];
    [dataMan saveSettingsWithBitrate:bitrate framerate:framerate height:height width:width
                    onscreenControls:0 remote:streamingRemotely];
}


- (void)controlTextDidChange:(NSNotification *)obj {
    
}

- (IBAction)buttonLaunchPressed:(id)sender {
    [self saveSettings];
    DataManager* dataMan = [[DataManager alloc] init];
    TemporarySettings* streamSettings = [dataMan getSettings];
    _streamConfig = [[StreamConfiguration alloc] init];
    _streamConfig.frameRate = [streamSettings.framerate intValue];
    _streamConfig.bitRate = [streamSettings.bitrate intValue];
    _streamConfig.height = [streamSettings.height intValue];
    _streamConfig.width = [streamSettings.width intValue];
    _streamConfig.streamingRemotely = [streamSettings.streamingRemotely intValue];
    _streamConfig.host = _textFieldHost.stringValue;
    _streamConfig.appID = [_sortedAppList[_popupButtonSelection.indexOfSelectedItem] id];
    [self transitionToStreamView];
}

- (IBAction)textFieldAction:(id)sender {
    [self buttonConnectPressed:self];
}

- (IBAction)buttonConnectPressed:(id)sender {
    _host = _textFieldHost.stringValue;
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_textFieldHost.stringValue
                                                 uniqueId:_uniqueId
                                               deviceName:deviceName
                                                     cert:_cert];
    
    ServerInfoResponse* serverInfoResp = [[ServerInfoResponse alloc] init];
    [hMan executeRequestSynchronously:[HttpRequest requestForResponse:serverInfoResp withUrlRequest:[hMan newServerInfoRequest]
                                                        fallbackError:401 fallbackRequest:[hMan newHttpServerInfoRequest]]];
    
    if ([[serverInfoResp getStringTag:@"PairStatus"] isEqualToString:@"1"]) {
        NSLog(@"alreadyPaired");
        [self alreadyPaired];
    } else {
        // Polling the server while pairing causes the server to screw up
        NSLog(@"Pairing");
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            HttpManager* hMan = [[HttpManager alloc] initWithHost:self->_host uniqueId:self->_uniqueId deviceName:deviceName cert:self->_cert];
            PairManager* pMan = [[PairManager alloc] initWithManager:hMan   andCert:self->_cert callback:self];
            [self->_opQueue addOperation:pMan];
    });
    }
}

- (IBAction)buttonSettingsPressed:(id)sender {
    showSettings = !showSettings;
    if(showSettings) {
        _layoutConstraintSetupFrame.constant = settingsFrameHeight;
    }
    else {
        _layoutConstraintSetupFrame.constant = 0;
    }
}

- (IBAction)popupButtonSelectionPressed:(id)sender {
}

- (void)alreadyPaired {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self->_popupButtonSelection setEnabled:true];
        [self->_popupButtonSelection setHidden:false];
        [self->_buttonConnect setEnabled:false];
        [self->_buttonConnect setHidden:true];
        [self->_buttonLaunch setEnabled:true];
        [self->_textFieldHost setEnabled:false];
    });
    [self searchForHost:_host];
    [self updateAppsForHost];
    [self populatePopupButton];
}

- (void)searchForHost:(NSString*) hostAddress {
    _appList = [ConnectionHelper getAppListForHostWithHostIP:_textFieldHost.stringValue deviceName:deviceName cert:_cert uniqueID:_uniqueId].getAppList;
}

- (void)populatePopupButton {
    for (int i = 0; i < _appList.count; i++) {
        [_popupButtonSelection addItemWithTitle:[_sortedAppList[i] name]];
    }
}

- (void)pairFailed:(NSString *)message {
    [self showAlert:[NSString stringWithFormat: @"%@", message]];
}

- (void)pairSuccessful {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.view.window endSheet:self->_alert.window];
        self->_alert = nil;
        [self alreadyPaired];
    });
}

- (void)showPIN:(NSString *)PIN {
    [self showAlert:[NSString stringWithFormat: @"PIN: %@", PIN]];
}

- (void) updateAppsForHost {
    _sortedAppList = [_appList allObjects];
    _sortedAppList = [_sortedAppList sortedArrayUsingSelector:@selector(compareName:)];
}

- (void)transitionToStreamView {
    NSStoryboard *storyBoard = [NSStoryboard storyboardWithName:@"Mac" bundle:nil];
    StreamFrameViewController* streamFrame = (StreamFrameViewController*)[storyBoard instantiateControllerWithIdentifier :@"streamFrameVC"];
    streamFrame.streamConfig = _streamConfig;
    [streamFrame setOrigin:self];
    self.view.window.contentViewController = streamFrame;
}

@end
