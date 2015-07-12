//
//  Host+CoreDataProperties.h
//  Moonlight
//
//  Created by Diego Waxemberg on 7/10/15.
//  Copyright © 2015 Limelight Stream. All rights reserved.
//
//  Delete this file and regenerate it using "Create NSManagedObject Subclass…"
//  to keep your implementation up to date with your model.
//

#import "Host.h"

NS_ASSUME_NONNULL_BEGIN

@interface Host (CoreDataProperties)

@property (nullable, nonatomic, retain) NSString *address;
@property (nullable, nonatomic, retain) NSString *externalAddress;
@property (nullable, nonatomic, retain) NSString *localAddress;
@property (nullable, nonatomic, retain) NSString *mac;
@property (nullable, nonatomic, retain) NSString *name;
@property (nullable, nonatomic, retain) NSNumber *pairState;
@property (nullable, nonatomic, retain) NSString *uuid;
@property (nullable, nonatomic, retain) NSSet *appList;

@end

@interface Host (CoreDataGeneratedAccessors)

- (void)addAppListObject:(NSManagedObject *)value;
- (void)removeAppListObject:(NSManagedObject *)value;
- (void)addAppList:(NSSet *)values;
- (void)removeAppList:(NSSet *)values;

@end

NS_ASSUME_NONNULL_END
