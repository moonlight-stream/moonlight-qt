//
//  AppAssetRetriever.h
//  Moonlight
//
//  Created by Diego Waxemberg on 1/31/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Host.h"
#import "App.h"
#import "AppAssetManager.h"

@interface AppAssetRetriever : NSOperation

@property (nonatomic) Host* host;
@property (nonatomic) App* app;
@property (nonatomic) NSMutableDictionary* cache;
@property (nonatomic) id<AppAssetCallback> callback;
@property (nonatomic) BOOL useCache;

@end
