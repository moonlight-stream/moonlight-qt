//
//  DataManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "DataManager.h"
#import "TemporaryApp.h"

@implementation DataManager

+ (NSObject *)databaseLock {
    static NSObject *lock = nil;
    if (lock == nil) {
        lock = [[NSObject alloc] init];
    }
    return lock;
}

- (id) init {
    self = [super init];
    self.appDelegate = [[UIApplication sharedApplication] delegate];
    return self;
}

- (void) saveSettingsWithBitrate:(NSInteger)bitrate framerate:(NSInteger)framerate height:(NSInteger)height width:(NSInteger)width onscreenControls:(NSInteger)onscreenControls {
    Settings* settingsToSave = [self retrieveSettings];
    settingsToSave.framerate = [NSNumber numberWithInteger:framerate];
    // Bitrate is persisted in kbps
    settingsToSave.bitrate = [NSNumber numberWithInteger:bitrate];
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
    return [[Host alloc] initWithEntity:entity insertIntoManagedObjectContext:[self.appDelegate managedObjectContext]];
}

- (void) removeHost:(Host*)host {
    [[self.appDelegate managedObjectContext] deleteObject:host];
    [self saveData];
}

- (void) saveData {
    @synchronized([DataManager databaseLock]) {
        NSError* error;
        if (![[self.appDelegate managedObjectContext] save:&error]) {
            Log(LOG_E, @"Unable to save hosts to database: %@", error);
        }
        [self.appDelegate saveContext];
    }
}

- (NSArray*) retrieveHosts {
    return [self fetchRecords:@"Host"];
}

- (App*) addAppFromTemporaryApp:(TemporaryApp*)tempApp {
    
    App* managedApp;
    
    @synchronized([DataManager databaseLock]) {
        NSEntityDescription* entity = [NSEntityDescription entityForName:@"App" inManagedObjectContext:[self.appDelegate managedObjectContext]];
        managedApp = [[App alloc] initWithEntity:entity insertIntoManagedObjectContext:[self.appDelegate managedObjectContext]];
        
        assert(tempApp.host != nil);
        
        managedApp.id = tempApp.id;
        managedApp.image = tempApp.image;
        managedApp.name = tempApp.name;
        managedApp.isRunning = tempApp.isRunning;
        managedApp.host = tempApp.host;
        
        [managedApp.host addAppListObject:managedApp];
    }
    
    return managedApp;
}

- (void) removeAppFromHost:(App*)app {
    @synchronized([DataManager databaseLock]) {
        assert(app.host != nil);
        
        [app.host removeAppListObject:app];
        [[self.appDelegate managedObjectContext] deleteObject:app];
    }
}

- (NSArray*) fetchRecords:(NSString*)entityName {
    NSArray* fetchedRecords;
    
    @synchronized([DataManager databaseLock]) {
        NSFetchRequest* fetchRequest = [[NSFetchRequest alloc] init];
        NSEntityDescription* entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:[self.appDelegate managedObjectContext]];
        [fetchRequest setEntity:entity];
        [fetchRequest setAffectedStores:[NSArray arrayWithObjects:[[self.appDelegate persistentStoreCoordinator] persistentStoreForURL:[self.appDelegate getStoreURL]], nil]];
        
        NSError* error;
        fetchedRecords = [[self.appDelegate managedObjectContext] executeFetchRequest:fetchRequest error:&error];
        //TODO: handle errors
    }
    
    return fetchedRecords;
}

@end

