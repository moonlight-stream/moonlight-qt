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
#import "PairManager.h"

@implementation StreamManager {
    MainFrameViewController* _viewCont;
    NSString* _host;
    NSData* _riKey;
    int _riKeyId;
}

- (id) initWithHost:(NSString*)host andViewController:(MainFrameViewController *)viewCont {
    self = [super init];
    _host = host;
    _viewCont = viewCont;
    return self;
}

- (NSData*) getRiKey {
    return _riKey;
}

- (int) getRiKeyId {
    return _riKeyId;
}

- (void)main {
    [CryptoManager generateKeyPairUsingSSl];
    NSString* uniqueId = [CryptoManager getUniqueID];
    NSData* cert = [CryptoManager readCertFromFile];
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_host uniqueId:uniqueId deviceName:@"roth" cert:cert];
    _riKey = [PairManager randomBytes:16];
    _riKeyId = arc4random();
    
    NSData* launchResp = [hMan executeRequestSynchronously:[hMan newLaunchRequest:@"67339056" width:1280 height:720 refreshRate:60 rikey:[PairManager bytesToHex:_riKey] rikeyid:_riKeyId]];
    [HttpManager getStringFromXML:launchResp tag:@"gamesession"];
    [_viewCont segueIntoStream];
}

@end
