//
//  TemporaryHost.h
//  Moonlight
//
//  Created by Cameron Gutman on 12/1/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "Utils.h"
#import "Host+CoreDataClass.h"

@interface TemporaryHost : NSObject

@property (nonatomic) BOOL online;
@property (nonatomic) PairState pairState;
@property (nonatomic, nullable) NSString * activeAddress;
@property (nonatomic, nullable) NSString * currentGame;

NS_ASSUME_NONNULL_BEGIN

@property (nonatomic, retain) NSString *address;
@property (nonatomic, retain) NSString *externalAddress;
@property (nonatomic, retain) NSString *localAddress;
@property (nonatomic, retain) NSString *mac;
@property (nonatomic, retain) NSString *name;
@property (nonatomic, retain) NSString *uuid;
@property (nonatomic, retain) NSMutableSet *appList;

- (id) initFromHost:(Host*)host;

- (NSComparisonResult)compareName:(TemporaryHost *)other;

- (void) propagateChangesToParent:(Host*)host;

NS_ASSUME_NONNULL_END

@end
