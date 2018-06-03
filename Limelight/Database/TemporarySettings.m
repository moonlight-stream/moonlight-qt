//
//  TemporarySettings.m
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporarySettings.h"

@implementation TemporarySettings

- (id) initFromSettings:(Settings*)settings {
    self = [self init];
    
    self.parent = settings;
    
    self.bitrate = settings.bitrate;
    self.framerate = settings.framerate;
    self.height = settings.height;
    self.width = settings.width;
    self.onscreenControls = settings.onscreenControls;
    self.uniqueId = settings.uniqueId;
    self.streamingRemotely = settings.streamingRemotely;
    self.useHevc = settings.useHevc;
    self.multiController = settings.multiController;
    self.playAudioOnPC = settings.playAudioOnPC;
    self.enableHdr = settings.enableHdr;
    self.optimizeGames = settings.optimizeGames;
    
    return self;
}

@end
