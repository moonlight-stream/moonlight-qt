//
//  Hosts.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/28/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>


@interface Hosts : NSManagedObject

@property (nonatomic, retain) NSString * address;
@property (nonatomic, retain) NSString * name;

@end
