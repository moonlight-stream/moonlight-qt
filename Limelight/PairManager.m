//
//  PairManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/19/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "PairManager.h"
#import "CryptoManager.h"

#include <dispatch/dispatch.h>

@implementation PairManager {
    HttpManager* _httpManager;
    NSData* _cert;
}

- (id) initWithManager:(HttpManager*)httpManager andCert:(NSData*)cert {
    self = [super init];
    _httpManager = httpManager;
    _cert = cert;
    return self;
}

- (void) main {
    [self initiatePair];
}

- (void) initiatePair {
    NSString* PIN = [self generatePIN];
    NSData* salt = [self saltPIN:PIN];
    NSLog(@"PIN: %@, saltedPIN: %@", PIN, salt);
    
    NSData* pairResp = [_httpManager executeRequestSynchronously:[_httpManager newPairRequest:salt]];
    if ([[HttpManager getStringFromXML:pairResp tag:@"paired"] intValue] != 1) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        return;
    }
    
    NSString* plainCert = [HttpManager getStringFromXML:pairResp tag:@"plaincert"];
    
    CryptoManager* cryptoMan = [[CryptoManager alloc] init];
    NSData* aesKey = [cryptoMan createAESKeyFromSalt:salt];
    
    NSData* randomChallenge = [self randomBytes:16];
    NSData* encryptedChallenge = [cryptoMan aesEncrypt:randomChallenge withKey:aesKey];
    
    NSData* challengeResp = [_httpManager executeRequestSynchronously:[_httpManager newChallengeRequest:encryptedChallenge]];
    if ([[HttpManager getStringFromXML:challengeResp tag:@"paired"] intValue] != 1) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        return;
    }
    
    NSData* encServerChallengeResp = [self hexToBytes:[HttpManager getStringFromXML:challengeResp tag:@"challengeresponse"]];
    NSData* decServerChallengeResp = [cryptoMan aesDecrypt:encServerChallengeResp withKey:aesKey];
    
    NSData* serverResponse = [decServerChallengeResp subdataWithRange:NSMakeRange(0, 20)];
    NSData* serverChallenge = [decServerChallengeResp subdataWithRange:NSMakeRange(20, 16)];
    
    NSData* clientSecret = [self randomBytes:16];
    NSData* challengeRespHash = [cryptoMan SHA1HashData:[self concatData:[self concatData:serverChallenge with:[CryptoManager getSignatureFromCert:_cert]] with:clientSecret]];
    NSData* challengeRespEncrypted = [cryptoMan aesEncrypt:challengeRespHash withKey:aesKey];
    
    NSData* secretResp = [_httpManager executeRequestSynchronously:[_httpManager newChallengeRespRequest:challengeRespEncrypted]];
    if ([[HttpManager getStringFromXML:secretResp tag:@"paired"] intValue] != 1) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        return;
    }
    
    NSData* serverSecretResp = [self hexToBytes:[HttpManager getStringFromXML:secretResp tag:@"pairingsecret"]];
    NSData* serverSecret = [serverSecretResp subdataWithRange:NSMakeRange(0, 16)];
    NSData* serverSignature = [serverSecretResp subdataWithRange:NSMakeRange(16, 256)];
    
    if (![cryptoMan verifySignature:serverSecret withSignature:serverSignature andCert:[self hexToBytes:plainCert]]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        return;
    }
    
    NSData* serverChallengeRespHash = [cryptoMan SHA1HashData:[self concatData:[self concatData:randomChallenge with:[CryptoManager getSignatureFromCert:[self hexToBytes:plainCert]]] with:serverSecret]];
    if (![serverChallengeRespHash isEqual:serverResponse]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        return;
    }
    
    NSData* clientPairingSecret = [self concatData:clientSecret with:[cryptoMan signData:clientSecret withKey:[CryptoManager readKeyFromFile]]];
    NSData* clientSecretResp = [_httpManager executeRequestSynchronously:[_httpManager newClientSecretRespRequest:[self bytesToHex:clientPairingSecret]]];
    if (![[HttpManager getStringFromXML:clientSecretResp tag:@"paired"] isEqual:@"1"]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        return;
    }
    
    NSData* clientPairChallenge = [_httpManager executeRequestSynchronously:[_httpManager newPairChallenge]];
    if (![[HttpManager getStringFromXML:clientPairChallenge tag:@"paired"] isEqual:@"1"]) {
        [_httpManager executeRequestSynchronously:[_httpManager newUnpairRequest]];
        return;
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
    NSLog(@"PIN: %@", PIN);
    return PIN;
}

- (NSData*) saltPIN:(NSString*)PIN {
    NSMutableData* saltedPIN = [[NSMutableData alloc] initWithCapacity:20];
    [saltedPIN appendData:[self randomBytes:16]];
    [saltedPIN appendBytes:[PIN UTF8String] length:4];
    
    //NSLog(@"Salted PIN: %@", [saltedPIN description]);
    return saltedPIN;
}

- (NSData*) randomBytes:(NSInteger)length {
    char* bytes = malloc(length);
    arc4random_buf(bytes, length);
    NSData* randomData = [NSData dataWithBytes:bytes length:length];
    free(bytes);
    return randomData;
}

- (NSData*) hexToBytes:(NSString*) hex {
    int len = [hex length];
    NSMutableData* data = [NSMutableData dataWithCapacity:len / 2];
    char byteChars[3] = {'\0','\0','\0'};
    unsigned long wholeByte;
    
    const char *chars = [hex UTF8String];
    int i = 0;
    while (i < len) {
        byteChars[0] = chars[i++];
        byteChars[1] = chars[i++];
        wholeByte = strtoul(byteChars, NULL, 16);
        [data appendBytes:&wholeByte length:1];
    }
    
    return data;
}

- (NSString*) bytesToHex:(NSData*)data {
    const unsigned char* bytes = [data bytes];
    NSMutableString *hex = [[NSMutableString alloc] init];
    for (int i = 0; i < [data length]; i++) {
        [hex appendFormat:@"%02X" , bytes[i]];
    }
    return hex;
}

@end
