//
//  TemporaryApp.m
//  Moonlight
//
//  Created by Cameron Gutman on 9/30/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporaryApp.h"
#import "App.h"

@implementation TemporaryApp

- (id) initFromApp:(App*)app withTempHost:(TemporaryHost*)tempHost {
    self = [self init];
    
    self.parent = app;
    
    self.id = app.id;
    self.image = app.image;
    self.name = app.name;
    self.host = tempHost;
    
    return self;
}

- (void) propagateChangesToParent {
    self.parent.id = self.id;
    self.parent.image = self.image;
    self.parent.name = self.name;
    self.parent.host = self.host.parent;
}

- (NSComparisonResult)compareName:(TemporaryApp *)other {
    return [self.name caseInsensitiveCompare:other.name];
}

@end
