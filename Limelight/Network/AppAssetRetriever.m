//
//  AppAssetRetriever.m
//  Limelight
//
//  Created by Diego Waxemberg on 1/31/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "AppAssetRetriever.h"
#import "HttpManager.h"
#import "CryptoManager.h"
#import "HttpResponse.h"

@implementation AppAssetRetriever
static const double RETRY_DELAY = 1; // seconds


- (void) main {
    UIImage* appImage = nil;
    while (![self isCancelled] && appImage == nil) {
        if (self.cache) {
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
            HttpResponse* appAssetResp = [hMan executeRequestSynchronously:[hMan newAppAssetRequestWithAppId:self.app.appId]];
            
            appImage = [UIImage imageWithData:appAssetResp.responseData];
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
