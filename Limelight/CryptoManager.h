//
//  CryptoManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface CryptoManager : NSObject

+ (void) generateKeyPairUsingSSl;
+ (NSString*) getUniqueID;
+ (NSData*) readCertFromFile;
+ (NSData*) readKeyFromFile;
+ (NSData*) readP12FromFile;
+ (NSData*) getSignatureFromCert:(NSData*)cert;

- (NSData*) createAESKeyFromSalt:(NSData*)saltedPIN;
- (NSData*) SHA1HashData:(NSData*)data;
- (NSData*) aesEncrypt:(NSData*)data withKey:(NSData*)key;
- (NSData*) aesDecrypt:(NSData*)data withKey:(NSData*)key;
- (bool) verifySignature:(NSData *)data withSignature:(NSData*)signature andCert:(NSData*)cert;
- (NSData*) signData:(NSData*)data withKey:(NSData*)key;

@end
