//
//  TemporaryHost.m
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporaryHost.h"
#import "Host.h"
#import "TemporaryApp.h"
#import "App.h"

@implementation TemporaryHost

- (id) initFromHost:(Host*)host {
    self = [self init];
    
    self.parent = host;
    
    self.address = host.address;
    self.externalAddress = host.externalAddress;
    self.localAddress = host.localAddress;
    self.mac = host.mac;
    self.name = host.name;
    self.uuid = host.uuid;
    
    NSMutableSet *appList = [[NSMutableSet alloc] init];

    for (App* app in host.appList) {
        TemporaryApp *tempApp = [[TemporaryApp alloc] initFromApp:app withTempHost:self];
        [appList addObject:tempApp];
    }
    
    self.appList = appList;
    
    return self;
}

- (void) propagateChangesToParent {
    self.parent.address = self.address;
    self.parent.externalAddress = self.externalAddress;
    self.parent.localAddress = self.localAddress;
    self.parent.mac = self.mac;
    self.parent.name = self.name;
    self.parent.uuid = self.uuid;
    
    NSMutableSet *applist = [[NSMutableSet alloc] init];
    for (TemporaryApp* app in self.appList) {
        // Add a new persistent managed object if one doesn't exist
        if (app.parent == nil) {
            NSEntityDescription* entity = [NSEntityDescription entityForName:@"App" inManagedObjectContext:self.parent.managedObjectContext];
            app.parent = [[App alloc] initWithEntity:entity insertIntoManagedObjectContext:self.parent.managedObjectContext];
        }
        
        [app propagateChangesToParent];
        
        [applist addObject:app.parent];
    }
    
    self.parent.appList = applist;
}

- (NSComparisonResult)compareName:(TemporaryHost *)other {
    return [self.name caseInsensitiveCompare:other.name];
}

@end
