//
//  WakeOnLanManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 1/2/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Host.h"

@interface WakeOnLanManager : NSObject

+ (void) wakeHost:(Host*)host;

@end
