//
//  TemporaryApp.m
//  Moonlight
//
//  Created by Cameron Gutman on 9/30/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporaryApp.h"

@implementation TemporaryApp

- (NSComparisonResult)compareName:(TemporaryApp *)other {
    return [self.name caseInsensitiveCompare:other.name];
}

@end
