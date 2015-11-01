//
//  IdManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/31/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "IdManager.h"
#import "DataManager.h"

@implementation IdManager

+ (NSString*) getUniqueId {
    DataManager* dataMan = [[DataManager alloc] init];
    Settings* prefs = [dataMan retrieveSettings];
    
    NSString* uniqueId = prefs.uniqueId;
    if (uniqueId == nil) {
        uniqueId = [IdManager generateUniqueId];
        prefs.uniqueId = uniqueId;
        [dataMan saveData];
        Log(LOG_I, @"No UUID found. Generated new UUID: %@", uniqueId);
    }
    return uniqueId;
}

+ (NSString*) generateUniqueId {
    UInt64 uuidLong = ((UInt64) arc4random() << 32) | arc4random();
    return [NSString stringWithFormat:@"%016llx", uuidLong];
}

@end
