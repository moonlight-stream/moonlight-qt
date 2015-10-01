//
//  TemporaryApp.h
//  Moonlight
//
//  Created by Cameron Gutman on 9/30/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Host.h"

@interface TemporaryApp : NSObject

@property (nullable, nonatomic, retain) NSString *id;
@property (nullable, nonatomic, retain) NSData *image;
@property (nullable, nonatomic, retain) NSString *name;
@property (nonatomic) BOOL isRunning;
@property (nullable, nonatomic, retain) Host *host;

@end
