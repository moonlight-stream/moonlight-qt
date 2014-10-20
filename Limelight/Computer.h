//
//  Computer.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Computer : NSObject
@property NSString* displayName;
@property NSString* hostName;
@property BOOL paired;

- (id) initWithHost:(NSNetService*)host;
- (int) resolveHost;

@end
