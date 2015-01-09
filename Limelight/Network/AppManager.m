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
    NSOperationQueue* _opQueue;
    id<AppAssetCallback> _callback;
    Host* _host;
    NSString* _uniqueId;
    NSData* _cert;
    NSMutableDictionary* _imageCache;
}

- (id) initWithCallback:(id<AppAssetCallback>)callback {
    self = [super init];
    _callback = callback;
    _opQueue = [[NSOperationQueue alloc] init];
    _imageCache = [[NSMutableDictionary alloc] init];
    _uniqueId = [CryptoManager getUniqueID];
    return self;
}

- (void) retrieveAssets:(NSArray*)appList fromHost:(Host*)host {
    Host* oldHost = _host;
    _host = host;
    BOOL useCache = [oldHost.uuid isEqualToString:_host.uuid];
    NSLog(@"Using cached app images: %d", useCache);
    if (!useCache) {
        [_imageCache removeAllObjects];
    }
    for (App* app in appList) {
        [_opQueue addOperationWithBlock:^{
            [self retrieveAssetForApp:app useCache:useCache];
        }];
    }
}

- (void) stopRetrieving {
    [_opQueue cancelAllOperations];
}

- (void) retrieveAssetForApp:(App*)app useCache:(BOOL)useCache {
    UIImage* appImage = nil;
    if (useCache) {
        @synchronized(_imageCache) {
            UIImage* cachedImage = [_imageCache objectForKey:app.appId];
            if (cachedImage != nil) {
                appImage = cachedImage;
                app.appImage = appImage;
            }
        }
    }
    if (appImage == nil) {
        HttpManager* hMan = [[HttpManager alloc] initWithHost:_host.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* appAsset = [hMan executeRequestSynchronously:[hMan newAppAssetRequestWithAppId:app.appId]];
        appImage = [UIImage imageWithData:appAsset];
        app.appImage = appImage;
        if (appImage != nil) {
            @synchronized(_imageCache) {
                [_imageCache setObject:appImage forKey:app.appId];
            }
        }
    }
    [self performSelectorOnMainThread:@selector(sendCallBackForApp:) withObject:app waitUntilDone:NO];
}

- (void) sendCallBackForApp:(App*)app {
    [_callback receivedAssetForApp:app];
}

@end
