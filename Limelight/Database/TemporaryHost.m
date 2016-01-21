//
//  TemporaryHost.m
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "DataManager.h"
#import "TemporaryHost.h"
#import "Host.h"
#import "TemporaryApp.h"
#import "App.h"

@implementation TemporaryHost

- (id) initFromHost:(Host*)host {
    self = [self init];
    
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

- (void) propagateChangesToParent:(Host*)parentHost withDm:(DataManager*)dm {
    parentHost.address = self.address;
    parentHost.externalAddress = self.externalAddress;
    parentHost.localAddress = self.localAddress;
    parentHost.mac = self.mac;
    parentHost.name = self.name;
    parentHost.uuid = self.uuid;
    
    NSMutableSet *applist = [[NSMutableSet alloc] init];
    for (TemporaryApp* app in self.appList) {
        // Add a new persistent managed object if one doesn't exist
        App* parentApp = [dm getAppForTemporaryApp:app];
        if (parentApp == nil) {
            NSEntityDescription* entity = [NSEntityDescription entityForName:@"App" inManagedObjectContext:parentHost.managedObjectContext];
            parentApp = [[App alloc] initWithEntity:entity insertIntoManagedObjectContext:parentHost.managedObjectContext];
        }
        
        [app propagateChangesToParent:parentApp withHost:parentHost];
        
        [applist addObject:parentApp];
    }
    
    parentHost.appList = applist;
}

- (NSComparisonResult)compareName:(TemporaryHost *)other {
    return [self.name caseInsensitiveCompare:other.name];
}

@end
