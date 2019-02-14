//
//  TemporarySettings.m
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporarySettings.h"
#import "OnScreenControls.h"

@implementation TemporarySettings

- (id) initFromSettings:(Settings*)settings {
    self = [self init];
    
    self.parent = settings;
    
#if TARGET_OS_TV
    NSInteger _bitrate = [[NSUserDefaults standardUserDefaults] integerForKey:@"bitrate"];
    NSInteger _framerate = [[NSUserDefaults standardUserDefaults] integerForKey:@"framerate"];

    if (_bitrate) {
        self.bitrate = [NSNumber numberWithInteger:_bitrate];
    } else {
        self.bitrate = [NSNumber numberWithInteger:20000];
    }

    if (_framerate) {
        self.framerate = [NSNumber numberWithInteger:_framerate];
    } else {
        self.framerate = [NSNumber numberWithInteger:60];
    }

    self.useHevc = [[NSUserDefaults standardUserDefaults] boolForKey:@"useHevc"] || NO;
    self.playAudioOnPC = [[NSUserDefaults standardUserDefaults] boolForKey:@"audioOnPC"] || NO;
    self.enableHdr = [[NSUserDefaults standardUserDefaults] boolForKey:@"enableHdr"] || NO;
    self.optimizeGames = [[NSUserDefaults standardUserDefaults] boolForKey:@"optimizeGames"] || YES;
    self.multiController = [[NSUserDefaults standardUserDefaults] boolForKey:@"multipleControllers"] || YES;
    
    NSInteger _screenSize = [[NSUserDefaults standardUserDefaults] integerForKey:@"streamResolution"];
    switch (_screenSize) {
        case 0:
            self.height = [NSNumber numberWithInteger:720];
            self.width = [NSNumber numberWithInteger:1280];
            break;
        case 2:
            self.height = [NSNumber numberWithInteger:2160];
            self.width = [NSNumber numberWithInteger:3840];
            break;
        case 1:
        default:
            self.height = [NSNumber numberWithInteger:1080];
            self.width = [NSNumber numberWithInteger:1920];
            break;
    }
    self.onscreenControls = (NSInteger)OnScreenControlsLevelOff;
#else
    self.bitrate = settings.bitrate;
    self.framerate = settings.framerate;
    self.height = settings.height;
    self.width = settings.width;
    self.useHevc = settings.useHevc;
    self.playAudioOnPC = settings.playAudioOnPC;
    self.enableHdr = settings.enableHdr;
    self.optimizeGames = settings.optimizeGames;
    self.multiController = settings.multiController;
    self.onscreenControls = settings.onscreenControls;
#endif
    self.uniqueId = settings.uniqueId;
    self.streamingRemotely = settings.streamingRemotely;
    
    return self;
}

@end
