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
    settingsToSave.bitrate = [NSNumber numberWithInteger:bitrate];
    settingsToSave.height = [NSNumber numberWithInteger:height];
    settingsToSave.width = [NSNumber numberWithInteger:width];
    settingsToSave.onscreenControls = [NSNumber numberWithInteger:onscreenControls];

    NSError* error;
    if (![[self.appDelegate managedObjectContext] save:&error]) {
        NSLog(@"ERROR: Unable to save settings to database");
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

- (Host*) createHost:(NSString*)name  hostname:(NSString*)address {
    NSEntityDescription* entity = [NSEntityDescription entityForName:@"Host" inManagedObjectContext:[self.appDelegate managedObjectContext]];
    Host* host = [[Host alloc] initWithEntity:entity insertIntoManagedObjectContext:[self.appDelegate managedObjectContext]];
    
    host.name = name;
    host.address = address;
    return host;
}

- (void) saveHosts {
    NSError* error;
    if (![[self.appDelegate managedObjectContext] save:&error]) {
        NSLog(@"ERROR: Unable to save hosts to database");
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

