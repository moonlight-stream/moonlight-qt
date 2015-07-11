//
//  AppManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/25/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "App.h"
#import "HttpManager.h"
#import "Host.h"

@protocol AppAssetCallback <NSObject>

- (void) receivedAssetForApp:(App*)app;

@end

@interface AppAssetManager : NSObject

- (id) initWithCallback:(id<AppAssetCallback>)callback;
- (void) retrieveAssetsFromHost:(Host*)host;
- (void) stopRetrieving;

@end
