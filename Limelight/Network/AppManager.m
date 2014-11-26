//
//  AppManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/25/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "AppManager.h"

@implementation AppManager {
    App* _app;
    HttpManager* _hMan;
    id<AppAssetCallback> _callback;
}

+ (void) retrieveAppAssets:(NSArray*)apps withManager:(HttpManager*)hMan andCallback:(id<AppAssetCallback>)callback {
    for (App* app in apps) {
        AppManager* manager = [[AppManager alloc] initWithApp:app httpManager:hMan andCallback:callback];
        [manager retrieveAsset];
    }
    
}

- (id) initWithApp:(App*)app httpManager:(HttpManager*)hMan andCallback:(id<AppAssetCallback>)callback {
    self = [super init];
    _app = app;
    _hMan = hMan;
    _callback = callback;
    return self;
}

- (void) retrieveAsset {
    NSData* appAsset = [_hMan executeRequestSynchronously:[_hMan newAppAssetRequestWithAppId:_app.appId]];
    UIImage* appImage = [UIImage imageWithData:appAsset];
    _app.appImage = appImage;
    NSLog(@"App Name: %@ id:%@ image: %@", _app.appName, _app.appId, _app.appImage);
    [self performSelectorOnMainThread:@selector(sendCallBack) withObject:self waitUntilDone:NO];
}

- (void) sendCallBack {
    [_callback receivedAssetForApp:_app];
}

@end
