//
//  AppManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/25/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "AppAssetManager.h"
#import "CryptoManager.h"
#import "Utils.h"
#import "HttpResponse.h"
#import "AppAssetRetriever.h"

@implementation AppAssetManager {
    NSOperationQueue* _opQueue;
    id<AppAssetCallback> _callback;
    Host* _host;
    NSMutableDictionary* _imageCache;
}

static const int MAX_REQUEST_COUNT = 4;

- (id) initWithCallback:(id<AppAssetCallback>)callback {
    self = [super init];
    _callback = callback;
    _imageCache = [[NSMutableDictionary alloc] init];
    _opQueue = [[NSOperationQueue alloc] init];
    [_opQueue setMaxConcurrentOperationCount:MAX_REQUEST_COUNT];

    return self;
}

- (void) retrieveAssets:(NSArray*)appList fromHost:(Host*)host {
    Host* oldHost = _host;
    _host = host;
    BOOL useCache = [oldHost.uuid isEqualToString:_host.uuid];
    Log(LOG_I, @"Using cached app images: %d", useCache);
    if (!useCache) {
        [_imageCache removeAllObjects];
    }
    for (App* app in appList) {
        AppAssetRetriever* retriever = [[AppAssetRetriever alloc] init];
        retriever.app = app;
        retriever.host = _host;
        retriever.callback = _callback;
        retriever.cache = _imageCache;
        retriever.useCache = useCache;
        [_opQueue addOperation:retriever];
    }
}

- (void) stopRetrieving {
    [_opQueue cancelAllOperations];
}

- (void) sendCallBackForApp:(App*)app {
    [_callback receivedAssetForApp:app];
}

@end
