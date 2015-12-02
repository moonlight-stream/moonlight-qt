//
//  AppManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/25/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TemporaryApp.h"
#import "HttpManager.h"
#import "TemporaryHost.h"

@protocol AppAssetCallback <NSObject>

- (void) receivedAssetForApp:(TemporaryApp*)app;

@end

@interface AppAssetManager : NSObject

- (id) initWithCallback:(id<AppAssetCallback>)callback;
- (void) retrieveAssetsFromHost:(TemporaryHost*)host;
- (void) stopRetrieving;

@end
