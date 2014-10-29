//
//  DataManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "DataManager.h"

@implementation DataManager
static NSInteger DEFAULT_BITRATE = 10000;
static NSInteger DEFAULT_FRAMERATE = 60;
static NSInteger DEFAULT_HEIGHT = 720;
static NSInteger DEFAULT_WIDTH = 1280;

- (id) init {
    self = [super init];
    self.appDelegate = [[UIApplication sharedApplication] delegate];
    return self;
}

- (void) saveSettingsWithBitrate:(NSInteger)bitrate framerate:(NSInteger)framerate height:(NSInteger)height width:(NSInteger)width {
    Settings* settingsToSave = [self retrieveSettings];
    settingsToSave.framerate = [NSNumber numberWithInteger:framerate];
    settingsToSave.bitrate = [NSNumber numberWithInteger:bitrate];
    settingsToSave.height = [NSNumber numberWithInteger:height];
    settingsToSave.width = [NSNumber numberWithInteger:width];
    [self saveSettings:settingsToSave];
}

- (Settings*) retrieveSettings {
    NSFetchRequest* fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription* entity = [NSEntityDescription entityForName:@"Settings" inManagedObjectContext:[self.appDelegate managedObjectContext]];
    [fetchRequest setEntity:entity];
    [fetchRequest setAffectedStores:[NSArray arrayWithObjects:[[self.appDelegate persistentStoreCoordinator] persistentStoreForURL:[self.appDelegate getStoreURL]], nil]];
    
    NSError* error;
    NSArray* fetchedRecords = [[self.appDelegate managedObjectContext] executeFetchRequest:fetchRequest error:&error];
    
    if (fetchedRecords.count == 0) {
        // create a new settings object with the default values
        Settings* settings = [[Settings alloc] initWithEntity:entity insertIntoManagedObjectContext:[self.appDelegate managedObjectContext]];
        settings.framerate = [NSNumber numberWithInteger:DEFAULT_FRAMERATE];
        settings.bitrate = [NSNumber numberWithInteger:DEFAULT_BITRATE];
        settings.height = [NSNumber numberWithInteger:DEFAULT_HEIGHT];
        settings.width = [NSNumber numberWithInteger:DEFAULT_WIDTH];
        return settings;
    } else {
        // we should only ever have 1 settings object stored
        return [fetchedRecords objectAtIndex:0];
    }
}

- (void) saveSettings:(Settings*)settings {
    NSError* error;
    if (![[self.appDelegate managedObjectContext] save:&error]) {
        NSLog(@"ERROR: Unable to save settings to database");
    }
    
    [self.appDelegate saveContext];
}


@end
