//
//  AppAssetRetriever.m
//  Moonlight
//
//  Created by Diego Waxemberg on 1/31/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "AppAssetRetriever.h"
#import "HttpManager.h"
#import "CryptoManager.h"
#import "AppAssetResponse.h"
#import "HttpRequest.h"

@implementation AppAssetRetriever
static const double RETRY_DELAY = 2; // seconds
static const int MAX_ATTEMPTS = 5;


- (void) main {
    UIImage* appImage = nil;
    int attempts = 0;
    while (![self isCancelled] && appImage == nil && attempts++ < MAX_ATTEMPTS) {
        if (self.useCache) {
            @synchronized(self.cache) {
                UIImage* cachedImage = [self.cache objectForKey:self.app.appId];
                if (cachedImage != nil) {
                    appImage = cachedImage;
                    self.app.appImage = appImage;
                }
            }
        }
        if (appImage == nil) {
            HttpManager* hMan = [[HttpManager alloc] initWithHost:_host.address uniqueId:[CryptoManager getUniqueID] deviceName:deviceName cert:[CryptoManager readCertFromFile]];
            AppAssetResponse* appAssetResp = [[AppAssetResponse alloc] init];
            [hMan executeRequestSynchronously:[HttpRequest requestForResponse:appAssetResp withUrlRequest:[hMan newAppAssetRequestWithAppId:self.app.appId]]];
            
            appImage = [UIImage imageWithData:appAssetResp.data];
            self.app.appImage = appImage;
            if (appImage != nil) {
                @synchronized(self.cache) {
                    [self.cache setObject:appImage forKey:self.app.appId];
                }
            }
        }

        [NSThread sleepForTimeInterval:RETRY_DELAY];
    }
    [self performSelectorOnMainThread:@selector(sendCallbackForApp:) withObject:self.app waitUntilDone:NO];
}

- (void) sendCallbackForApp:(App*)app {
    [self.callback receivedAssetForApp:app];
}

@end
