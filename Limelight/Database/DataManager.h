//
//  DataManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Settings.h"
#import "AppDelegate.h"
#import "Host.h"
#import "App.h"
#import "TemporaryApp.h"

@interface DataManager : NSObject

@property (strong, nonatomic) AppDelegate* appDelegate;

- (void) saveSettingsWithBitrate:(NSInteger)bitrate framerate:(NSInteger)framerate height:(NSInteger)height width:(NSInteger)width onscreenControls:(NSInteger)onscreenControls;
- (Settings*) retrieveSettings;
- (NSArray*) retrieveHosts;
- (void) saveData;
- (Host*) createHost;
- (void) removeHost:(Host*)host;
- (App*) addAppFromTemporaryApp:(TemporaryApp*)tempApp;
- (void) removeAppFromHost:(App*)app;

@end
