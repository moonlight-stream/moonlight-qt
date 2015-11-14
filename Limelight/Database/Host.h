//
//  Host.h
//  Moonlight
//
//  Created by Diego Waxemberg on 7/10/15.
//  Copyright © 2015 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import "Utils.h"

NS_ASSUME_NONNULL_BEGIN

@interface Host : NSManagedObject

@property (nonatomic) BOOL online;
@property (nonatomic) PairState pairState;
@property (nonatomic) NSString * activeAddress;

- (NSComparisonResult)compareName:(Host *)other;

@end

NS_ASSUME_NONNULL_END

#import "Host+CoreDataProperties.h"
