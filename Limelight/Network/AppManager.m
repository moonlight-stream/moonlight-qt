//
//  AppManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/25/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "AppManager.h"
#import "CryptoManager.h"
#import "Utils.h"

@implementation AppManager {
    NSOperationQueue* opQueue;
    id<AppAssetCallback> _callback;
    Host* _host;
    NSString* _uniqueId;
    NSData* _cert;
}

- (id) initWithHost:(Host*)host andCallback:(id<AppAssetCallback>)callback {
    self = [super init];
    _callback = callback;
    _host = host;
    opQueue = [[NSOperationQueue alloc] init];
    return self;
}

- (void) retrieveAssets:(NSArray*)appList {
    for (App* app in appList) {
        [opQueue addOperationWithBlock:^{
            [self retrieveAssetForApp:app];
        }];
    }
}

- (void) stopRetrieving {
    [opQueue cancelAllOperations];
}

- (void) retrieveAssetForApp:(App*)app {
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_host.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
    NSData* appAsset = [hMan executeRequestSynchronously:[hMan newAppAssetRequestWithAppId:app.appId]];
    UIImage* appImage = [UIImage imageWithData:appAsset];
    app.appImage = appImage;
    NSLog(@"App Name: %@ id:%@ image: %@", app.appName, app.appId, app.appImage);
    [self performSelectorOnMainThread:@selector(sendCallBackForApp:) withObject:app waitUntilDone:NO];
}

- (void) sendCallBackForApp:(App*)app {
    [_callback receivedAssetForApp:app];
}

@end
