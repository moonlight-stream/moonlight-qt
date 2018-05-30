//
//  DataManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "DataManager.h"
#import "TemporaryApp.h"
#import "TemporarySettings.h"

@implementation DataManager {
    NSManagedObjectContext *_managedObjectContext;
    AppDelegate *_appDelegate;
}

- (id) init {
    self = [super init];
    
    // HACK: Avoid calling [UIApplication delegate] off the UI thread to keep
    // Main Thread Checker happy.
    if ([NSThread isMainThread]) {
        _appDelegate = (AppDelegate *)[[OSApplication sharedApplication] delegate];
        
    }
    else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            self->_appDelegate = (AppDelegate *)[[OSApplication sharedApplication] delegate];
        });
    }
    
    _managedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
    [_managedObjectContext setParentContext:[_appDelegate managedObjectContext]];
    
    return self;
}

- (void) updateUniqueId:(NSString*)uniqueId {
    [_managedObjectContext performBlockAndWait:^{
        [self retrieveSettings].uniqueId = uniqueId;
        [self saveData];
    }];
}

- (NSString*) getUniqueId {
    __block NSString *uid;
    
    [_managedObjectContext performBlockAndWait:^{
        uid = [self retrieveSettings].uniqueId;
    }];

    return uid;
}

- (void) saveSettingsWithBitrate:(NSInteger)bitrate framerate:(NSInteger)framerate height:(NSInteger)height width:(NSInteger)width onscreenControls:(NSInteger)onscreenControls remote:
    (NSInteger) streamingRemotely {
    
    [_managedObjectContext performBlockAndWait:^{
        Settings* settingsToSave = [self retrieveSettings];
        settingsToSave.framerate = [NSNumber numberWithInteger:framerate];
        // Bitrate is persisted in kbps
        settingsToSave.bitrate = [NSNumber numberWithInteger:bitrate];
        settingsToSave.height = [NSNumber numberWithInteger:height];
        settingsToSave.width = [NSNumber numberWithInteger:width];
        settingsToSave.onscreenControls = [NSNumber numberWithInteger:onscreenControls];
        settingsToSave.streamingRemotely = [NSNumber numberWithInteger:streamingRemotely];
        
        [self saveData];
    }];
}

- (void) updateHost:(TemporaryHost *)host {
    [_managedObjectContext performBlockAndWait:^{
        // Add a new persistent managed object if one doesn't exist
        Host* parent = [self getHostForTemporaryHost:host withHostRecords:[self fetchRecords:@"Host"]];
        if (parent == nil) {
            NSEntityDescription* entity = [NSEntityDescription entityForName:@"Host" inManagedObjectContext:self->_managedObjectContext];
            parent = [[Host alloc] initWithEntity:entity insertIntoManagedObjectContext:self->_managedObjectContext];
        }
        
        // Push changes from the temp host to the persistent one
        [host propagateChangesToParent:parent];
        
        [self saveData];
    }];
}

- (void) updateAppsForExistingHost:(TemporaryHost *)host {
    [_managedObjectContext performBlockAndWait:^{
        Host* parent = [self getHostForTemporaryHost:host withHostRecords:[self fetchRecords:@"Host"]];
        if (parent == nil) {
            // The host must exist to be updated
            return;
        }
        
        NSMutableSet *applist = [[NSMutableSet alloc] init];
        NSArray *appRecords = [self fetchRecords:@"App"];
        for (TemporaryApp* app in host.appList) {
            // Add a new persistent managed object if one doesn't exist
            App* parentApp = [self getAppForTemporaryApp:app withAppRecords:appRecords];
            if (parentApp == nil) {
                NSEntityDescription* entity = [NSEntityDescription entityForName:@"App" inManagedObjectContext:self->_managedObjectContext];
                parentApp = [[App alloc] initWithEntity:entity insertIntoManagedObjectContext:self->_managedObjectContext];
            }
            
            [app propagateChangesToParent:parentApp withHost:parent];
            
            [applist addObject:parentApp];
        }
        
        parent.appList = applist;
        
        [self saveData];
    }];
}

- (void) updateIconForExistingApp:(TemporaryApp*)app {
    [_managedObjectContext performBlockAndWait:^{
        App* parentApp = [self getAppForTemporaryApp:app withAppRecords:[self fetchRecords:@"App"]];
        if (parentApp == nil) {
            // The app must exist to be updated
            return;
        }
        
        parentApp.image = app.image;
        
        [self saveData];
    }];
}

- (TemporarySettings*) getSettings {
    __block TemporarySettings *tempSettings;
    
    [_managedObjectContext performBlockAndWait:^{
        tempSettings = [[TemporarySettings alloc] initFromSettings:[self retrieveSettings]];
    }];
    
    return tempSettings;
}

- (Settings*) retrieveSettings {
    NSArray* fetchedRecords = [self fetchRecords:@"Settings"];
    if (fetchedRecords.count == 0) {
        // create a new settings object with the default values
        NSEntityDescription* entity = [NSEntityDescription entityForName:@"Settings" inManagedObjectContext:_managedObjectContext];
        Settings* settings = [[Settings alloc] initWithEntity:entity insertIntoManagedObjectContext:_managedObjectContext];
        
        return settings;
    } else {
        // we should only ever have 1 settings object stored
        return [fetchedRecords objectAtIndex:0];
    }
}

- (void) removeApp:(TemporaryApp*)app {
    [_managedObjectContext performBlockAndWait:^{
        App* managedApp = [self getAppForTemporaryApp:app withAppRecords:[self fetchRecords:@"App"]];
        if (managedApp != nil) {
            [self->_managedObjectContext deleteObject:managedApp];
            [self saveData];
        }
    }];
}

- (void) removeHost:(TemporaryHost*)host {
    [_managedObjectContext performBlockAndWait:^{
        Host* managedHost = [self getHostForTemporaryHost:host withHostRecords:[self fetchRecords:@"Host"]];
        if (managedHost != nil) {
            [self->_managedObjectContext deleteObject:managedHost];
            [self saveData];
        }
    }];
}

- (void) saveData {
    NSError* error;
    if ([_managedObjectContext hasChanges] && ![_managedObjectContext save:&error]) {
        Log(LOG_E, @"Unable to save hosts to database: %@", error);
    }

    [_appDelegate saveContext];
}

- (NSArray*) getHosts {
    __block NSMutableArray *tempHosts = [[NSMutableArray alloc] init];
    
    [_managedObjectContext performBlockAndWait:^{
        NSArray *hosts = [self fetchRecords:@"Host"];
        
        for (Host* host in hosts) {
            [tempHosts addObject:[[TemporaryHost alloc] initFromHost:host]];
        }
    }];
    
    return tempHosts;
}

// Only call from within performBlockAndWait!!!
- (Host*) getHostForTemporaryHost:(TemporaryHost*)tempHost withHostRecords:(NSArray*)hosts {
    for (Host* host in hosts) {
        if ([tempHost.uuid isEqualToString:host.uuid]) {
            return host;
        }
    }
    
    return nil;
}

// Only call from within performBlockAndWait!!!
- (App*) getAppForTemporaryApp:(TemporaryApp*)tempApp withAppRecords:(NSArray*)apps {
    for (App* app in apps) {
        if ([app.id isEqualToString:tempApp.id] &&
            [app.host.uuid isEqualToString:app.host.uuid]) {
            return app;
        }
    }
    
    return nil;
}

- (NSArray*) fetchRecords:(NSString*)entityName {
    NSArray* fetchedRecords;
    
    NSFetchRequest* fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription* entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:_managedObjectContext];
    [fetchRequest setEntity:entity];
    
    NSError* error;
    fetchedRecords = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];
    //TODO: handle errors
    
    return fetchedRecords;
}

@end

