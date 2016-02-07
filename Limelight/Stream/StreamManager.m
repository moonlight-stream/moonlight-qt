//
//  StreamManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/20/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "StreamManager.h"
#import "CryptoManager.h"
#import "HttpManager.h"
#import "Utils.h"
#import "OnScreenControls.h"
#import "StreamView.h"
#import "ServerInfoResponse.h"
#import "HttpResponse.h"
#import "HttpRequest.h"
#import "IdManager.h"

@implementation StreamManager {
    StreamConfiguration* _config;
    UIView* _renderView;
    id<ConnectionCallbacks> _callbacks;
    Connection* _connection;
}

- (id) initWithConfig:(StreamConfiguration*)config renderView:(UIView*)view connectionCallbacks:(id<ConnectionCallbacks>)callbacks {
    self = [super init];
    _config = config;
    _renderView = view;
    _callbacks = callbacks;
    _config.riKey = [Utils randomBytes:16];
    _config.riKeyId = arc4random();
    return self;
}


- (void)main {
    [CryptoManager generateKeyPairUsingSSl];
    NSString* uniqueId = [IdManager getUniqueId];
    NSData* cert = [CryptoManager readCertFromFile];
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_config.host
                                                 uniqueId:uniqueId
                                               deviceName:@"roth"
                                                     cert:cert];
    
    ServerInfoResponse* serverInfoResp = [[ServerInfoResponse alloc] init];
    [hMan executeRequestSynchronously:[HttpRequest requestForResponse:serverInfoResp withUrlRequest:[hMan newServerInfoRequest]
                                       fallbackError:401 fallbackRequest:[hMan newHttpServerInfoRequest]]];
    NSString* pairStatus = [serverInfoResp getStringTag:@"PairStatus"];
    NSString* appversion = [serverInfoResp getStringTag:@"appversion"];
    NSString* serverState = [serverInfoResp getStringTag:@"state"];
    if (![serverInfoResp isStatusOk] || pairStatus == NULL || appversion == NULL || serverState == NULL) {
        [_callbacks launchFailed:@"Failed to connect to PC"];
        return;
    }
    
    if (![pairStatus isEqualToString:@"1"]) {
        // Not paired
        [_callbacks launchFailed:@"Device not paired to PC"];
        return;
    }
    
    // resumeApp and launchApp handle calling launchFailed
    if ([serverState hasSuffix:@"_SERVER_BUSY"]) {
        // App already running, resume it
        if (![self resumeApp:hMan]) {
            return;
        }
    } else {
        // Start app
        if (![self launchApp:hMan]) {
            return;
        }
    }
    
    // Set mouse delta factors from the screen resolution and stream size
    CGFloat screenScale = [[UIScreen mainScreen] scale];
    CGRect screenBounds = [[UIScreen mainScreen] bounds];
    CGSize screenSize = CGSizeMake(screenBounds.size.width * screenScale, screenBounds.size.height * screenScale);
    [((StreamView*)_renderView) setMouseDeltaFactors:_config.width / screenSize.width
                                                   y:_config.height / screenSize.height];
    
    int majorVersion = [[appversion substringToIndex:1] intValue];
    Log(LOG_I, @"Server is generation %d", majorVersion);
    
    VideoDecoderRenderer* renderer = [[VideoDecoderRenderer alloc]initWithView:_renderView];
    _connection = [[Connection alloc] initWithConfig:_config renderer:renderer connectionCallbacks:_callbacks serverMajorVersion:majorVersion];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:_connection];
}

// This should NEVER be called from within a thread
// owned by moonlight-common
- (void) stopStream
{
    [_connection terminate];
}

// This should only be called from within a thread
// owned by moonlight-common
- (void) stopStreamInternal
{
    [_connection terminateInternal];
}

- (BOOL) launchApp:(HttpManager*)hMan {
    HttpResponse* launchResp = [[HttpResponse alloc] init];
    [hMan executeRequestSynchronously:[HttpRequest requestForResponse:launchResp withUrlRequest:
                          [hMan newLaunchRequest:_config.appID
                                           width:_config.width
                                          height:_config.height
                                     refreshRate:_config.frameRate
                                           rikey:[Utils bytesToHex:_config.riKey]
                                         rikeyid:_config.riKeyId]]];
    NSString *gameSession = [launchResp getStringTag:@"gamesession"];
    if (launchResp == NULL || ![launchResp isStatusOk]) {
        [_callbacks launchFailed:@"Failed to launch app"];
        Log(LOG_E, @"Failed Launch Response: %@", launchResp.statusMessage);
        return FALSE;
    } else if (gameSession == NULL || [gameSession isEqualToString:@"0"]) {
        [_callbacks launchFailed:launchResp.statusMessage];
        Log(LOG_E, @"Failed to parse game session. Code: %ld Response: %@", (long)launchResp.statusCode, launchResp.statusMessage);
        return FALSE;
    }
    
    return TRUE;
}

- (BOOL) resumeApp:(HttpManager*)hMan {
    HttpResponse* resumeResp = [[HttpResponse alloc] init];
    [hMan executeRequestSynchronously:[HttpRequest requestForResponse:resumeResp withUrlRequest:
                          [hMan newResumeRequestWithRiKey:[Utils bytesToHex:_config.riKey]
                                                  riKeyId:_config.riKeyId]]];
    NSString* resume = [resumeResp getStringTag:@"resume"];
    if (resumeResp == NULL || ![resumeResp isStatusOk]) {
        [_callbacks launchFailed:@"Failed to resume app"];
        Log(LOG_E, @"Failed Resume Response: %@", resumeResp.statusMessage);
        return FALSE;
    } else if (resume == NULL || [resume isEqualToString:@"0"]) {
        [_callbacks launchFailed:resumeResp.statusMessage];
        Log(LOG_E, @"Failed to parse resume response. Code: %ld Response: %@", (long)resumeResp.statusCode, resumeResp.statusMessage);
        return FALSE;
    }
    
    return TRUE;
}

@end
