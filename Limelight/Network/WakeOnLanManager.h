//
//  WakeOnLanManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 1/2/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TemporaryHost.h"

@interface WakeOnLanManager : NSObject

+ (void) wakeHost:(TemporaryHost*)host;

@end
