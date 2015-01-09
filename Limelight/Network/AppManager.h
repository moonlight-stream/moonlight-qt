//
//  AppManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/25/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "App.h"
#import "HttpManager.h"
#import "Host.h"

@protocol AppAssetCallback <NSObject>

- (void) receivedAssetForApp:(App*)app;

@end

@interface AppManager : NSObject

- (id) initWithHost:(Host*)host andCallback:(id<AppAssetCallback>)callback;
- (void) retrieveAssets:(NSArray*)appList;
- (void) stopRetrieving;

@end
