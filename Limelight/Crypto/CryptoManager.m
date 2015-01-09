//
//  CryptoManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "CryptoManager.h"
#import "mkcert.h"
#import <AdSupport/ASIdentifierManager.h>

#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

@implementation CryptoManager
static const int SHA1_DIGEST_LENGTH = 20;
static NSData* key = nil;
static NSData* cert = nil;
static NSData* p12 = nil;

- (NSData*) createAESKeyFromSalt:(NSData*)saltedPIN {
    return [[self SHA1HashData:saltedPIN] subdataWithRange:NSMakeRange(0, 16)];
}

- (NSData*) SHA1HashData:(NSData*)data {
    unsigned char sha1[SHA1_DIGEST_LENGTH];
    SHA1([data bytes], [data length], sha1);
    NSData* bytes = [NSData dataWithBytes:sha1 length:20];
    return bytes;
}

- (NSData*) aesEncrypt:(NSData*)data withKey:(NSData*)key {
    AES_KEY aesKey;
    AES_set_encrypt_key([key bytes], 128, &aesKey);
    int size = [self getEncryptSize:data];
    unsigned char* buffer = malloc(size);
    unsigned char* blockRoundedBuffer = calloc(1, size);
    memcpy(blockRoundedBuffer, [data bytes], [data length]);
    
    // AES_encrypt only encrypts the first 16 bytes so iterate the entire buffer
    int blockOffset = 0;
    while (blockOffset < size) {
        AES_encrypt(blockRoundedBuffer + blockOffset, buffer + blockOffset, &aesKey);
        blockOffset += 16;
    }
    
    NSData* encryptedData = [NSData dataWithBytes:buffer length:size];
    free(buffer);
    free(blockRoundedBuffer);
    return encryptedData;
}

- (NSData*) aesDecrypt:(NSData*)data withKey:(NSData*)key {
    AES_KEY aesKey;
    AES_set_decrypt_key([key bytes], 128, &aesKey);
    unsigned char* buffer = malloc([data length]);
    
    // AES_decrypt only decrypts the first 16 bytes so iterate the entire buffer
    int blockOffset = 0;
    while (blockOffset < [data length]) {
        AES_decrypt([data bytes] + blockOffset, buffer + blockOffset, &aesKey);
        blockOffset += 16;
    }
    
    NSData* decryptedData = [NSData dataWithBytes:buffer length:[data length]];
    free(buffer);
    return decryptedData;
}

- (int) getEncryptSize:(NSData*)data {
    // the size is the length of the data ceiling to the nearest 16 bytes
    return (((int)[data length] + 15) / 16) * 16;
}

- (bool) verifySignature:(NSData *)data withSignature:(NSData*)signature andCert:(NSData*)cert {
    const char* buffer = [cert bytes];
    X509* x509;
    BIO* bio = BIO_new(BIO_s_mem());
    BIO_puts(bio, buffer);
    x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    
    BIO_free(bio);
    
    if (!x509) {
        NSLog(@"ERROR: unable to parse certificate in memory");
        return NULL;
    }
    
    EVP_PKEY* pubKey = X509_get_pubkey(x509);
    EVP_MD_CTX *mdctx = NULL;
    mdctx = EVP_MD_CTX_create();
    EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pubKey);
    EVP_DigestVerifyUpdate(mdctx, [data bytes], [data length]);
    int result = EVP_DigestVerifyFinal(mdctx, (unsigned char*)[signature bytes], [signature length]);
    
    X509_free(x509);
    EVP_PKEY_free(pubKey);
    EVP_MD_CTX_destroy(mdctx);
    
    return result > 0;
}

- (NSData *)signData:(NSData *)data withKey:(NSData *)key {
    const char* buffer = [key bytes];
    BIO* bio = BIO_new(BIO_s_mem());
    BIO_puts(bio, buffer);
    
    EVP_PKEY* pkey;
    pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    
    BIO_free(bio);
    
    if (!pkey) {
        NSLog(@"ERROR: unable to parse private key in memory!");
        return NULL;
    }
    
    EVP_MD_CTX *mdctx = NULL;
    mdctx = EVP_MD_CTX_create();
    EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, pkey);
    EVP_DigestSignUpdate(mdctx, [data bytes], [data length]);
    size_t slen;
    EVP_DigestSignFinal(mdctx, NULL, &slen);
    unsigned char* signature = malloc(slen);
    int result = EVP_DigestSignFinal(mdctx, signature, &slen);
    
    EVP_PKEY_free(pkey);
    EVP_MD_CTX_destroy(mdctx);
    
    if (result <= 0) {
        free(signature);
        return NULL;
    }
    
    NSData* signedData = [NSData dataWithBytes:signature length:slen];
    free(signature);
    
    return signedData;
}

// TODO: these three methods are almost identical, fix the copy-pasta
+ (NSData*) readCertFromFile {
    if (cert == nil) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        NSString *certFile = [documentsDirectory stringByAppendingPathComponent:@"client.crt"];
        cert = [NSData dataWithContentsOfFile:certFile];
    }
    return cert;
}

+ (NSData*) readP12FromFile {
    if (p12 == nil) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        NSString *p12File = [documentsDirectory stringByAppendingPathComponent:@"client.p12"];
        p12 = [NSData dataWithContentsOfFile:p12File];
    }
    return p12;
}

+ (NSData*) readKeyFromFile {
    if (key == nil) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        NSString *keyFile = [documentsDirectory stringByAppendingPathComponent:@"client.key"];
        key = [NSData dataWithContentsOfFile:keyFile];
    }
    return key;
}

+ (bool) keyPairExists {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *keyFile = [documentsDirectory stringByAppendingPathComponent:@"client.key"];
    NSString *p12File = [documentsDirectory stringByAppendingPathComponent:@"client.p12"];
    NSString *certFile = [documentsDirectory stringByAppendingPathComponent:@"client.crt"];
    
    bool keyFileExists = [[NSFileManager defaultManager] fileExistsAtPath:keyFile];
    bool p12FileExists = [[NSFileManager defaultManager] fileExistsAtPath:p12File];
    bool certFileExists = [[NSFileManager defaultManager] fileExistsAtPath:certFile];
    
    return keyFileExists && p12FileExists && certFileExists;
}

+ (NSData *)getSignatureFromCert:(NSData *)cert {
    const char* buffer = [cert bytes];
    X509* x509;
    BIO* bio = BIO_new(BIO_s_mem());
    BIO_puts(bio, buffer);
    x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    
    if (!x509) {
        NSLog(@"ERROR: unable to parse certificate in memory!");
        return NULL;
    }
    return [NSData dataWithBytes:x509->signature->data length:x509->signature->length];
}

+ (void) generateKeyPairUsingSSl {
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        if (![CryptoManager keyPairExists]) {
            
            NSLog(@"Generating Certificate... ");
            CertKeyPair certKeyPair = generateCertKeyPair();
            
            NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
            NSString* documentsDirectory = [paths objectAtIndex:0];
            NSString* certFile = [documentsDirectory stringByAppendingPathComponent:@"client.crt"];
            NSString* keyPairFile = [documentsDirectory stringByAppendingPathComponent:@"client.key"];
            NSString* p12File = [documentsDirectory stringByAppendingPathComponent:@"client.p12"];
            
            //NSLog(@"Writing cert and key to: \n%@\n%@", certFile, keyPairFile);
            saveCertKeyPair([certFile UTF8String], [p12File UTF8String], [keyPairFile UTF8String], certKeyPair);
            freeCertKeyPair(certKeyPair);
            NSLog(@"Certificate created");
        }
    });
}

+ (NSString*) getUniqueID {
    // generate a UUID
    NSUUID* uuid = [ASIdentifierManager sharedManager].advertisingIdentifier;
    NSString* idString = [NSString stringWithString:[uuid UUIDString]];
    
    // we need a 16byte hex-string so we take the last 17 characters
    // and remove the '-' to get a 16 character string
    NSMutableString* uniqueId = [NSMutableString stringWithString:[idString substringFromIndex:19]];
    [uniqueId deleteCharactersInRange:NSMakeRange(4, 1)];
    
    //NSLog(@"Unique ID: %@", uniqueId);
    return [NSString stringWithString:uniqueId];
}

@end
