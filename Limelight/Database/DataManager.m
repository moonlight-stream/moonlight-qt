//
//  DataManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "DataManager.h"

@implementation DataManager

- (id) init {
    self = [super init];
    self.appDelegate = [[UIApplication sharedApplication] delegate];
    return self;
}

- (void) saveSettingsWithBitrate:(NSInteger)bitrate framerate:(NSInteger)framerate height:(NSInteger)height width:(NSInteger)width onscreenControls:(NSInteger)onscreenControls {
    Settings* settingsToSave = [self retrieveSettings];
    settingsToSave.framerate = [NSNumber numberWithInteger:framerate];
    // Bitrate is persisted in kbps
    settingsToSave.bitrate = [NSNumber numberWithInteger:bitrate * 1000];
    settingsToSave.height = [NSNumber numberWithInteger:height];
    settingsToSave.width = [NSNumber numberWithInteger:width];
    settingsToSave.onscreenControls = [NSNumber numberWithInteger:onscreenControls];

    NSError* error;
    if (![[self.appDelegate managedObjectContext] save:&error]) {
        Log(LOG_E, @"Unable to save settings to database: %@", error);
    }
    [self.appDelegate saveContext];
}

- (Settings*) retrieveSettings {
    NSArray* fetchedRecords = [self fetchRecords:@"Settings"];
    if (fetchedRecords.count == 0) {
        // create a new settings object with the default values
        NSEntityDescription* entity = [NSEntityDescription entityForName:@"Settings" inManagedObjectContext:[self.appDelegate managedObjectContext]];
        Settings* settings = [[Settings alloc] initWithEntity:entity insertIntoManagedObjectContext:[self.appDelegate managedObjectContext]];
        
        return settings;
    } else {
        // we should only ever have 1 settings object stored
        return [fetchedRecords objectAtIndex:0];
    }
}

- (Host*) createHost {
    NSEntityDescription* entity = [NSEntityDescription entityForName:@"Host" inManagedObjectContext:[self.appDelegate managedObjectContext]];
    Host* host = [[Host alloc] initWithEntity:entity insertIntoManagedObjectContext:[self.appDelegate managedObjectContext]];
    return host;
}

- (void) removeHost:(Host*)host {
    [[self.appDelegate managedObjectContext] deleteObject:host];
}

- (void) saveHosts {
    NSError* error;
    if (![[self.appDelegate managedObjectContext] save:&error]) {
        Log(LOG_E, @"Unable to save hosts to database: %@", error);
    }
    [self.appDelegate saveContext];
}

- (NSArray*) retrieveHosts {
    return [self fetchRecords:@"Host"];
}

- (NSArray*) fetchRecords:(NSString*)entityName {
    NSFetchRequest* fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription* entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:[self.appDelegate managedObjectContext]];
    [fetchRequest setEntity:entity];
    [fetchRequest setAffectedStores:[NSArray arrayWithObjects:[[self.appDelegate persistentStoreCoordinator] persistentStoreForURL:[self.appDelegate getStoreURL]], nil]];
    
    NSError* error;
    NSArray* fetchedRecords = [[self.appDelegate managedObjectContext] executeFetchRequest:fetchRequest error:&error];
    //TODO: handle errors
    
    return fetchedRecords;
    
}

@end

