//
//  Host.m
//  Moonlight
//
//  Created by Diego Waxemberg on 7/10/15.
//  Copyright © 2015 Limelight Stream. All rights reserved.
//

#import "Host.h"

@implementation Host

@dynamic pairState;
@synthesize online;
@synthesize activeAddress;

- (NSComparisonResult)compareName:(Host *)other {
    return [self.name caseInsensitiveCompare:other.name];
}

@end
