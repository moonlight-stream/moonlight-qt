//
//  ConnectionHelper.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 22.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import "ConnectionHelper.h"
#import "ServerInfoResponse.h"
#import "HttpManager.h"
#import "PairManager.h"
#import "DiscoveryManager.h"

@implementation ConnectionHelper

+(AppListResponse*) getAppListForHostWithHostIP:(NSString*) hostIP deviceName:(NSString*)deviceName cert:(NSData*) cert uniqueID:(NSString*) uniqueId {
    HttpManager* hMan = [[HttpManager alloc] initWithHost:hostIP uniqueId:uniqueId deviceName:deviceName cert:cert];
    
    // Try up to 5 times to get the app list
    AppListResponse* appListResp;
    for (int i = 0; i < 5; i++) {
        appListResp = [[AppListResponse alloc] init];
        [hMan executeRequestSynchronously:[HttpRequest requestForResponse:appListResp withUrlRequest:[hMan newAppListRequest]]];
        if (appListResp == nil || ![appListResp isStatusOk] || [appListResp getAppList] == nil) {
            Log(LOG_W, @"Failed to get applist on try %d: %@", i, appListResp.statusMessage);
            
            // Wait for one second then retry
            [NSThread sleepForTimeInterval:1];
        }
        else {
            Log(LOG_I, @"App list successfully retreived - took %d tries", i);
            return appListResp;
        }
    }
    return nil;
}

@end

