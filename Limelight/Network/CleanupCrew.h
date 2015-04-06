//
//  CleanupCrew.h
//  Limelight
//
//  Created by Diego Waxemberg on 4/5/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Host.h"

@protocol CleanupCallback <NSObject>

- (void) cleanedUpHosts:(NSSet*) host;

@end

@interface CleanupCrew : NSOperation

- (id) initWithHostList:(NSArray*) hostList andCallback:(id<CleanupCallback>)callback;

@end
