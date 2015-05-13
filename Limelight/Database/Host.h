//
//  Host.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import "Utils.h"

@interface Host : NSManagedObject

@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) NSString * address;
@property (nonatomic, retain) NSString * localAddress;
@property (nonatomic, retain) NSString * externalAddress;
@property (nonatomic, retain) NSString * uuid;
@property (nonatomic, retain) NSString * mac;
@property (nonatomic) BOOL online;
@property (nonatomic) PairState pairState;

@end
