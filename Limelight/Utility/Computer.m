//
//  Computer.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "Computer.h"

@implementation Computer

- (id) initWithHost:(NSNetService *)host {
    self = [super init];
    
    self.hostName = [host hostName];
    self.displayName = [host name];
    
    return self;
}

- (id) initWithIp:(NSString*)host {
    self = [super init];
    
    self.hostName = host;
    self.displayName = host;
    
    return self;
}

- (BOOL)isEqual:(id)object {
    if ([object isKindOfClass:[Computer class]]) {
        return [self.hostName isEqual:[object valueForKey:@"hostName"]];
    } else {
        return NO;
    }
}

- (NSUInteger)hash {
    NSUInteger prime = 31;
    NSUInteger result = 1;
    result = prime * result + (self.hostName == nil ? 0 : [self.hostName hash]);
    
    return result;
}

@end
