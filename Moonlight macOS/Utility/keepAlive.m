//
//  keepAlive.m
//  LittleHelper
//
//  Created by Felix Kratz on 31.10.17.
//  Copyright © 2017 Felix Kratz. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import "keepAlive.h"


@implementation keepAlive

CFStringRef reasonForActivity= CFSTR("Moonlight keeps the system awake");
IOPMAssertionID assertionID;

+(void) keepSystemAlive
{
    
    IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
                                                   kIOPMAssertionLevelOn, reasonForActivity, &assertionID);
}

+(void) allowSleep
{
    IOPMAssertionRelease(assertionID);
}
@end
