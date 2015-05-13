//
//  PairManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/19/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "PairManager.h"
#import "CryptoManager.h"
#import "Utils.h"
#import "HttpResponse.h"
#import "HttpRequest.h"
#import "ServerInfoResponse.h"

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
    ServerInfoResponse* serverInfoResp = [[ServerInfoResponse alloc] init];
    [_httpManager executeRequestSynchronously:[HttpRequest requestForResponse:serverInfoResp withUrlRequest:[_httpManager newServerInfoRequest]]];
    if (serverInfoResp == nil) {
        [_callback pairFailed:@"Unable to connect to PC"];
        return;
    }
    if ([serverInfoResp isStatusOk]) {
        if (![[serverInfoResp getStringTag:@"currentgame"] isEqual:@"0"]) {
            [_callback pairFailed:@"You must stop streaming before attempting to pair."];
        } else if (![[serverInfoResp getStringTag:@"PairStatus"] isEqual:@"1"]) {
            [self initiatePair];
        } else {
            [_callback alreadyPaired];
        }
    }
}

- (void) initiatePair {
    NSString* PIN = [self generatePIN];
    NSData* salt = [self saltPIN:PIN];
    Log(LOG_I, @"PIN: %@, saltedPIN: %@", PIN, salt);
    [_callback showPIN:PIN];
    
    HttpResponse* pairResp = [[HttpResponse alloc] init];
    [_httpManager executeRequestSynchronously:[HttpRequest requestForResponse:pairResp withUrlRequest:[_httpManager newPairRequest:salt]]];
    if (![self verifyResponseStatus:pairResp]) {
        return;
    }
    NSInteger pairedStatus;
    if (![pairResp getIntTag:@"paired" value:&pairedStatus] || !pairedStatus) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Pairing was declined by the target."];
        return;
    }
    
    NSString* plainCert = [pairResp getStringTag:@"plaincert"];
    
    CryptoManager* cryptoMan = [[CryptoManager alloc] init];
    NSData* aesKey = [cryptoMan createAESKeyFromSalt:salt];
    
    NSData* randomChallenge = [Utils randomBytes:16];
    NSData* encryptedChallenge = [cryptoMan aesEncrypt:randomChallenge withKey:aesKey];
    
    HttpResponse* challengeResp = [[HttpResponse alloc] init];
    [_httpManager executeRequestSynchronously:[HttpRequest requestForResponse:challengeResp withUrlRequest:[_httpManager newChallengeRequest:encryptedChallenge]]];
    if (![self verifyResponseStatus:challengeResp]) {
        return;
    }
    if (![challengeResp getIntTag:@"paired" value:&pairedStatus] || !pairedStatus) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Pairing stage #2 failed"];
        return;
    }
    
    NSData* encServerChallengeResp = [Utils hexToBytes:[challengeResp getStringTag:@"challengeresponse"]];
    NSData* decServerChallengeResp = [cryptoMan aesDecrypt:encServerChallengeResp withKey:aesKey];
    
    NSData* serverResponse = [decServerChallengeResp subdataWithRange:NSMakeRange(0, 20)];
    NSData* serverChallenge = [decServerChallengeResp subdataWithRange:NSMakeRange(20, 16)];
    
    NSData* clientSecret = [Utils randomBytes:16];
    NSData* challengeRespHash = [cryptoMan SHA1HashData:[self concatData:[self concatData:serverChallenge with:[CryptoManager getSignatureFromCert:_cert]] with:clientSecret]];
    NSData* challengeRespEncrypted = [cryptoMan aesEncrypt:challengeRespHash withKey:aesKey];
    
    HttpResponse* secretResp = [[HttpResponse alloc] init];
    [_httpManager executeRequestSynchronously:[HttpRequest requestForResponse:secretResp withUrlRequest:[_httpManager newChallengeRespRequest:challengeRespEncrypted]]];
    if (![self verifyResponseStatus:secretResp]) {
        return;
    }
    if (![secretResp getIntTag:@"paired" value:&pairedStatus] || !pairedStatus) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Pairing stage #3 failed"];
        return;
    }
    
    NSData* serverSecretResp = [Utils hexToBytes:[secretResp getStringTag:@"pairingsecret"]];
    NSData* serverSecret = [serverSecretResp subdataWithRange:NSMakeRange(0, 16)];
    NSData* serverSignature = [serverSecretResp subdataWithRange:NSMakeRange(16, 256)];
    
    if (![cryptoMan verifySignature:serverSecret withSignature:serverSignature andCert:[Utils hexToBytes:plainCert]]) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Server certificate invalid"];
        return;
    }
    
    NSData* serverChallengeRespHash = [cryptoMan SHA1HashData:[self concatData:[self concatData:randomChallenge with:[CryptoManager getSignatureFromCert:[Utils hexToBytes:plainCert]]] with:serverSecret]];
    if (![serverChallengeRespHash isEqual:serverResponse]) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Incorrect PIN"];
        return;
    }
    
    NSData* clientPairingSecret = [self concatData:clientSecret with:[cryptoMan signData:clientSecret withKey:[CryptoManager readKeyFromFile]]];
    HttpResponse* clientSecretResp = [[HttpResponse alloc] init];
    [_httpManager executeRequestSynchronously:[HttpRequest requestForResponse:clientSecretResp withUrlRequest:[_httpManager newClientSecretRespRequest:[Utils bytesToHex:clientPairingSecret]]]];
    if (![self verifyResponseStatus:clientSecretResp]) {
        return;
    }
    if (![clientSecretResp getIntTag:@"paired" value:&pairedStatus] || !pairedStatus) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Pairing stage #4 failed"];
        return;
    }
    
    HttpResponse* clientPairChallengeResp = [[HttpResponse alloc] init];
    [_httpManager executeRequestSynchronously:[HttpRequest requestForResponse:clientPairChallengeResp withUrlRequest:[_httpManager newPairChallenge]]];
    if (![self verifyResponseStatus:clientPairChallengeResp]) {
        return;
    }
    if (![clientPairChallengeResp getIntTag:@"paired" value:&pairedStatus] || !pairedStatus) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Pairing stage #5 failed"];
        return;
    }
    [_callback pairSuccessful];
}

- (BOOL) verifyResponseStatus:(HttpResponse*)resp {
    if (resp == nil) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:@"Network error occured."];
        return false;
    } else if (![resp isStatusOk]) {
        [_httpManager executeRequest:[HttpRequest requestWithUrlRequest:[_httpManager newUnpairRequest]]];
        [_callback pairFailed:resp.statusMessage];
        return false;
    } else {
        return true;
    }
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
