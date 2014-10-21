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

- (id) initPlaceholder {
    self = [super init];
    
    self.hostName = NULL;
    self.displayName = @"No computers found";
    
    return self;
}

@end
