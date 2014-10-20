//
//  PairManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/19/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "PairManager.h"
#import "CryptoManager.h"
#import "Utils.h"

#include <dispatch/dispatch.h>

@implementation PairManager {
    HttpManager* _httpManager;
    NSData* _cert;
    id<PairCallback> _callback;
}

- (id) initWithManager:(HttpManager*)httpManager andCert:(NSData*)cert callback:(id<PairCallback>)callback {
    self = [super init];
    _httpManager = httpManager;
    _cert = cert;
    _callback = callback;
    return self;
}

- (void) main {
    NSData* serverInfo = [_httpManager executeRequestSynchronously:[_httpManager newServerInfoRequest]];
    if (![[HttpManager getStringFromXML:serverInfo tag:@"PairStatus"] isEqual:@"1"]) {
        [self initiatePair];
    } else {
        [_callback pairFailed:@"Already Paired"];
    }
    [_httpManager executeRequestSynchronously:[_httpManager newAppListRequest]];
}

- (void) initiatePair {
    NSString* PIN = [self generatePIN];
    NSData* salt = [self saltPIN:PIN];
    NSLog(@"PIN: %@, saltedPIN: %@", PIN, salt);
    [_callback showPIN:PIN];
    
    NSData* pairResp = [_httpManager executeRequestSynchronously:[_httpManager newPairRequest:salt]];
    if ([[HttpManager getStringFromXML:pairResp tag:@"paired"] intValue] != 1) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        //TODO: better message
        [_callback pairFailed:@"pairResp failed"];
        return;
    }
    
    NSString* plainCert = [HttpManager getStringFromXML:pairResp tag:@"plaincert"];
    
    CryptoManager* cryptoMan = [[CryptoManager alloc] init];
    NSData* aesKey = [cryptoMan createAESKeyFromSalt:salt];
    
    NSData* randomChallenge = [Utils randomBytes:16];
    NSData* encryptedChallenge = [cryptoMan aesEncrypt:randomChallenge withKey:aesKey];
    
    NSData* challengeResp = [_httpManager executeRequestSynchronously:[_httpManager newChallengeRequest:encryptedChallenge]];
    if ([[HttpManager getStringFromXML:challengeResp tag:@"paired"] intValue] != 1) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        //TODO: better message
        [_callback pairFailed:@"challengeResp failed"];
        return;
    }
    
    NSData* encServerChallengeResp = [Utils hexToBytes:[HttpManager getStringFromXML:challengeResp tag:@"challengeresponse"]];
    NSData* decServerChallengeResp = [cryptoMan aesDecrypt:encServerChallengeResp withKey:aesKey];
    
    NSData* serverResponse = [decServerChallengeResp subdataWithRange:NSMakeRange(0, 20)];
    NSData* serverChallenge = [decServerChallengeResp subdataWithRange:NSMakeRange(20, 16)];
    
    NSData* clientSecret = [Utils randomBytes:16];
    NSData* challengeRespHash = [cryptoMan SHA1HashData:[self concatData:[self concatData:serverChallenge with:[CryptoManager getSignatureFromCert:_cert]] with:clientSecret]];
    NSData* challengeRespEncrypted = [cryptoMan aesEncrypt:challengeRespHash withKey:aesKey];
    
    NSData* secretResp = [_httpManager executeRequestSynchronously:[_httpManager newChallengeRespRequest:challengeRespEncrypted]];
    if ([[HttpManager getStringFromXML:secretResp tag:@"paired"] intValue] != 1) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        //TODO: better message
        [_callback pairFailed:@"secretResp failed"];
        return;
    }
    
    NSData* serverSecretResp = [Utils hexToBytes:[HttpManager getStringFromXML:secretResp tag:@"pairingsecret"]];
    NSData* serverSecret = [serverSecretResp subdataWithRange:NSMakeRange(0, 16)];
    NSData* serverSignature = [serverSecretResp subdataWithRange:NSMakeRange(16, 256)];
    
    if (![cryptoMan verifySignature:serverSecret withSignature:serverSignature andCert:[Utils hexToBytes:plainCert]]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        //TODO: better message
        [_callback pairFailed:@"verifySignature failed"];
        return;
    }
    
    NSData* serverChallengeRespHash = [cryptoMan SHA1HashData:[self concatData:[self concatData:randomChallenge with:[CryptoManager getSignatureFromCert:[Utils hexToBytes:plainCert]]] with:serverSecret]];
    if (![serverChallengeRespHash isEqual:serverResponse]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        //TODO: better message
        [_callback pairFailed:@"serverChallengeResp failed"];
        return;
    }
    
    NSData* clientPairingSecret = [self concatData:clientSecret with:[cryptoMan signData:clientSecret withKey:[CryptoManager readKeyFromFile]]];
    NSData* clientSecretResp = [_httpManager executeRequestSynchronously:[_httpManager newClientSecretRespRequest:[Utils bytesToHex:clientPairingSecret]]];
    if (![[HttpManager getStringFromXML:clientSecretResp tag:@"paired"] isEqual:@"1"]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        //TODO: better message
        [_callback pairFailed:@"clientSecretResp failed"];
        return;
    }
    
    NSData* clientPairChallenge = [_httpManager executeRequestSynchronously:[_httpManager newPairChallenge]];
    if (![[HttpManager getStringFromXML:clientPairChallenge tag:@"paired"] isEqual:@"1"]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        //TODO: better message
        [_callback pairFailed:@"clientPairChallenge failed"];
        return;
    }
    [_callback pairSuccessful];
}

- (NSData*) concatData:(NSData*)data with:(NSData*)moreData {
    NSMutableData* concatData = [[NSMutableData alloc] initWithData:data];
    [concatData appendData:moreData];
    return concatData;
}

- (NSString*) generatePIN {
    NSString* PIN = [NSString stringWithFormat:@"%d%d%d%d",
                     arc4random() % 10, arc4random() % 10,
                     arc4random() % 10, arc4random() % 10];
    return PIN;
}

- (NSData*) saltPIN:(NSString*)PIN {
    NSMutableData* saltedPIN = [[NSMutableData alloc] initWithCapacity:20];
    [saltedPIN appendData:[Utils randomBytes:16]];
    [saltedPIN appendBytes:[PIN UTF8String] length:4];
    return saltedPIN;
}

@end
