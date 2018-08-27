//
//  TemporaryApp.m
//  Moonlight
//
//  Created by Cameron Gutman on 9/30/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporaryApp.h"

@implementation TemporaryApp

- (id) initFromApp:(App*)app withTempHost:(TemporaryHost*)tempHost {
    self = [self init];
    
    self.id = app.id;
    self.name = app.name;
    self.hdrSupported = app.hdrSupported;
    self.host = tempHost;
    
    return self;
}

- (void) propagateChangesToParent:(App*)parent withHost:(Host*)host {
    parent.id = self.id;
    parent.name = self.name;
    parent.hdrSupported = self.hdrSupported;
    parent.host = host;
}

- (NSComparisonResult)compareName:(TemporaryApp *)other {
    return [self.name caseInsensitiveCompare:other.name];
}

- (NSUInteger)hash {
    return [self.host.uuid hash] * 31 + [self.id intValue];
}

- (BOOL)isEqual:(id)object {
    if (self == object) {
        return YES;
    }
    
    if (![object isKindOfClass:[App class]]) {
        return NO;
    }
    
    return [self.host.uuid isEqualToString:((App*)object).host.uuid] &&
    [self.id isEqualToString:((App*)object).id];
}

@end
