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
}

static const int MAX_REQUEST_COUNT = 4;

- (id) initWithCallback:(id<AppAssetCallback>)callback {
    self = [super init];
    _callback = callback;
    _opQueue = [[NSOperationQueue alloc] init];
    [_opQueue setMaxConcurrentOperationCount:MAX_REQUEST_COUNT];
    
    return self;
}

- (void) retrieveAssetsFromHost:(Host*)host {
    for (App* app in host.appList) {
        if (app.image == nil) {
            AppAssetRetriever* retriever = [[AppAssetRetriever alloc] init];
            retriever.app = app;
            retriever.host = host;
            retriever.callback = _callback;
            
            [_opQueue addOperation:retriever];
        }
    }
}

- (void) stopRetrieving {
    [_opQueue cancelAllOperations];
}

- (void) sendCallBackForApp:(App*)app {
    [_callback receivedAssetForApp:app];
}

@end
