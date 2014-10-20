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
#import "StreamConfiguration.h"

@implementation StreamManager {
    NSString* _host;
    UIView* _renderView;
}

- (id) initWithHost:(NSString*)host renderView:(UIView*)view {
    self = [super init];
    _host = host;
    _renderView = view;
    return self;
}

- (void)main {
    [CryptoManager generateKeyPairUsingSSl];
    NSString* uniqueId = [CryptoManager getUniqueID];
    NSData* cert = [CryptoManager readCertFromFile];
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_host uniqueId:uniqueId deviceName:@"roth" cert:cert];
    NSData* riKey = [Utils randomBytes:16];
    int riKeyId = arc4random();
    
    NSData* launchResp = [hMan executeRequestSynchronously:[hMan newLaunchRequest:@"67339056" width:1920 height:1080 refreshRate:30 rikey:[Utils bytesToHex:riKey] rikeyid:riKeyId]];
    [HttpManager getStringFromXML:launchResp tag:@"gamesession"];
    
    VideoDecoderRenderer* renderer = [[VideoDecoderRenderer alloc]initWithView:_renderView];
    
    StreamConfiguration* config = [[StreamConfiguration alloc] init];
    config.host = [Utils resolveHost:_host];
    config.width = 1920;
    config.height = 1080;
    config.frameRate = 30;
    config.bitRate = 10000;
    config.riKey = riKey;
    config.riKeyId = riKeyId;
    
    Connection* conn = [[Connection alloc] initWithConfig:config renderer:renderer];
    
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:conn];
}

@end
