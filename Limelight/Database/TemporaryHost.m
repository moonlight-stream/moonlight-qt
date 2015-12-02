//
//  TemporaryHost.m
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporaryHost.h"

@implementation TemporaryHost

- (NSComparisonResult)compareName:(TemporaryHost *)other {
    return [self.name caseInsensitiveCompare:other.name];
}

@end
