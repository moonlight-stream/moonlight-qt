//
//  TemporaryHost.h
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Utils.h"

@interface TemporaryHost : NSObject

@property (nonatomic) BOOL online;
@property (nonatomic) PairState pairState;
@property (nonatomic, nullable) NSString * activeAddress;

@property (nullable, nonatomic, retain) NSString *address;
@property (nullable, nonatomic, retain) NSString *externalAddress;
@property (nullable, nonatomic, retain) NSString *localAddress;
@property (nullable, nonatomic, retain) NSString *mac;
@property (nullable, nonatomic, retain) NSString *name;
@property (nullable, nonatomic, retain) NSString *uuid;
@property (nullable, nonatomic, retain) NSSet *appList;

NS_ASSUME_NONNULL_BEGIN

- (NSComparisonResult)compareName:(TemporaryHost *)other;

NS_ASSUME_NONNULL_END

@end
