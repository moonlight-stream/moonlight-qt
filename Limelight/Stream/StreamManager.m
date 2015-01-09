//
//  StreamManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/20/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "StreamManager.h"
#import "CryptoManager.h"
#import "HttpManager.h"
#import "Utils.h"
#import "OnScreenControls.h"

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
    NSString* uniqueId = [CryptoManager getUniqueID];
    NSData* cert = [CryptoManager readCertFromFile];
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_config.host
                                                 uniqueId:uniqueId
                                               deviceName:@"roth"
                                                     cert:cert];
    
    NSData* serverInfoResp = [hMan executeRequestSynchronously:[hMan newServerInfoRequest]];
    NSString* currentGame = [HttpManager getStringFromXML:serverInfoResp tag:@"currentgame"];
    NSString* pairStatus = [HttpManager getStringFromXML:serverInfoResp tag:@"PairStatus"];
    NSString* currentClient = [HttpManager getStringFromXML:serverInfoResp tag:@"CurrentClient"];
    if (currentGame == NULL || pairStatus == NULL) {
        [_callbacks launchFailed:@"Failed to connect to PC"];
        return;
    }
    
    if (![pairStatus isEqualToString:@"1"]) {
        // Not paired
        [_callbacks launchFailed:@"Device not paired to PC"];
        return;
    }
    
    // resumeApp and launchApp handle calling launchFailed
    if (![currentGame isEqualToString:@"0"]) {
        if (![currentClient isEqualToString:@"1"]) {
            // The server is streaming to someone else
            [_callbacks launchFailed:@"There is another stream in progress"];
            return;
        }
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
    
    VideoDecoderRenderer* renderer = [[VideoDecoderRenderer alloc]initWithView:_renderView];
    _connection = [[Connection alloc] initWithConfig:_config renderer:renderer connectionCallbacks:_callbacks];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:_connection];
}

// This should NEVER be called from within a thread
// owned by limelight-common
- (void) stopStream
{
    [_connection terminate];
}

// This should only be called from within a thread
// owned by limelight-common
- (void) stopStreamInternal
{
    [_connection terminateInternal];
}

- (BOOL) launchApp:(HttpManager*)hMan {
    NSData* launchResp = [hMan executeRequestSynchronously:
                          [hMan newLaunchRequest:_config.appID
                                           width:_config.width
                                          height:_config.height
                                     refreshRate:_config.frameRate
                                           rikey:[Utils bytesToHex:_config.riKey]
                                         rikeyid:_config.riKeyId]];
    NSString *gameSession = [HttpManager getStringFromXML:launchResp tag:@"gamesession"];
    if (launchResp == NULL) {
        [_callbacks launchFailed:@"Failed to launch app"];
        return FALSE;
    } else if (gameSession == NULL || [gameSession isEqualToString:@"0"]) {
        [_callbacks launchFailed:[HttpManager getStatusStringFromXML:launchResp]];
        return FALSE;
    }
    
    return TRUE;
}

- (BOOL) resumeApp:(HttpManager*)hMan {
    NSData* resumeResp = [hMan executeRequestSynchronously:
                          [hMan newResumeRequestWithRiKey:[Utils bytesToHex:_config.riKey]
                                                  riKeyId:_config.riKeyId]];
    NSString *resume = [HttpManager getStringFromXML:resumeResp tag:@"resume"];
    if (resumeResp == NULL) {
        [_callbacks launchFailed:@"Failed to resume app"];
        return FALSE;
    } else if (resume == NULL || [resume isEqualToString:@"0"]) {
        [_callbacks launchFailed:[HttpManager getStatusStringFromXML:resumeResp]];
        return FALSE;
    }
    
    return TRUE;
}

@end
