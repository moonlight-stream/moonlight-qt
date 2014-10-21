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

@implementation StreamManager {
    StreamConfiguration* _config;
    UIView* _renderView;
    id<ConTermCallback> _callback;
    Connection* _connection;
}

- (id) initWithConfig:(StreamConfiguration*)config renderView:(UIView*)view connectionTerminatedCallback:(id<ConTermCallback>)callback {
    self = [super init];
    _config = config;
    _renderView = view;
    _callback = callback;
    _config.riKey = [Utils randomBytes:16];
    _config.riKeyId = arc4random();
    _config.bitRate = 10000;
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
    if (![[HttpManager getStringFromXML:serverInfoResp tag:@"currentgame"] isEqualToString:@"0"]) {
        // App already running, resume it
        [self resumeApp:hMan];
    } else {
        // Start app
        [self launchApp:hMan];
    }
    
    VideoDecoderRenderer* renderer = [[VideoDecoderRenderer alloc]initWithView:_renderView];
    _connection = [[Connection alloc] initWithConfig:_config renderer:renderer connectionTerminatedCallback:_callback];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:_connection];
}

- (void) stopStream
{
    [_connection terminate];
}

- (void) launchApp:(HttpManager*)hMan {
    NSData* launchResp = [hMan executeRequestSynchronously:
                          [hMan newLaunchRequest:@"67339056"
                                           width:_config.width
                                          height:_config.height
                                     refreshRate:_config.frameRate
                                           rikey:[Utils bytesToHex:_config.riKey]
                                         rikeyid:_config.riKeyId]];
    [HttpManager getStringFromXML:launchResp tag:@"gamesession"];
}

- (void) resumeApp:(HttpManager*)hMan {
    NSData* resumeResp = [hMan executeRequestSynchronously:
                          [hMan newResumeRequestWithRiKey:[Utils bytesToHex:_config.riKey]
                                                  riKeyId:_config.riKeyId]];
    [HttpManager getStringFromXML:resumeResp tag:@"gamesession"];
}

@end
