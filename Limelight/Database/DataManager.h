//
//  DataManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Settings.h"
#import "AppDelegate.h"
#import "Host.h"

@interface DataManager : NSObject

@property (strong, nonatomic) AppDelegate* appDelegate;

- (void) saveSettingsWithBitrate:(NSInteger)bitrate framerate:(NSInteger)framerate height:(NSInteger)height width:(NSInteger)width onscreenControls:(NSInteger)onscreenControls;
- (Settings*) retrieveSettings;
- (NSArray*) retrieveHosts;
- (void) saveHosts;
- (Host*) createHost:(NSString*)name  hostname:(NSString*)address;

@end
