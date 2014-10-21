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
#import "Connection.h"

@implementation StreamManager {
    StreamConfiguration* _config;
    UIView* _renderView;
}

- (id) initWithConfig:(StreamConfiguration*)config renderView:(UIView*)view {
    self = [super init];
    _config = config;
    _renderView = view;
    return self;
}


- (void)main {
    [CryptoManager generateKeyPairUsingSSl];
    NSString* uniqueId = [CryptoManager getUniqueID];
    NSData* cert = [CryptoManager readCertFromFile];
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_config.host uniqueId:uniqueId deviceName:@"roth" cert:cert];
    NSData* riKey = [Utils randomBytes:16];
    int riKeyId = arc4random();
    
    NSData* launchResp = [hMan executeRequestSynchronously:[hMan newLaunchRequest:@"67339056" width:_config.width height:_config.height refreshRate:_config.frameRate rikey:[Utils bytesToHex:riKey] rikeyid:riKeyId]];
    [HttpManager getStringFromXML:launchResp tag:@"gamesession"];
    
    VideoDecoderRenderer* renderer = [[VideoDecoderRenderer alloc]initWithView:_renderView];
    
    _config.bitRate = 10000;
    _config.riKey = riKey;
    _config.riKeyId = riKeyId;
    
    Connection* conn = [[Connection alloc] initWithConfig:_config renderer:renderer];
    
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:conn];
}

@end
