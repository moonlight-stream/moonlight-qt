//
//  CryptoManager.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "CryptoManager.h"
#import "mkcert.h"

#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

@implementation CryptoManager
static const int SHA1_HASH_LENGTH = 20;
static const int SHA256_HASH_LENGTH = 32;
static NSData* key = nil;
static NSData* cert = nil;
static NSData* p12 = nil;

- (NSData*) createAESKeyFromSaltSHA1:(NSData*)saltedPIN {
    return [[self SHA1HashData:saltedPIN] subdataWithRange:NSMakeRange(0, 16)];
}

- (NSData*) createAESKeyFromSaltSHA256:(NSData*)saltedPIN {
    return [[self SHA256HashData:saltedPIN] subdataWithRange:NSMakeRange(0, 16)];
}

- (NSData*) SHA1HashData:(NSData*)data {
    unsigned char sha1[SHA1_HASH_LENGTH];
    SHA1([data bytes], [data length], sha1);
    NSData* bytes = [NSData dataWithBytes:sha1 length:sizeof(sha1)];
    return bytes;
}

- (NSData*) SHA256HashData:(NSData*)data {
    unsigned char sha256[SHA256_HASH_LENGTH];
    SHA256([data bytes], [data length], sha256);
    NSData* bytes = [NSData dataWithBytes:sha256 length:sizeof(sha256)];
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

+ (NSData*) pemToDer:(NSData*)pemCertBytes {
    X509* x509;
    
    BIO* bio = BIO_new_mem_buf([pemCertBytes bytes], (int)[pemCertBytes length]);
    x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    BIO_free(bio);
    
    bio = BIO_new(BIO_s_mem());
    i2d_X509_bio(bio, x509);

    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    
    NSData* ret = [[NSData alloc] initWithBytes:mem->data length:mem->length];
    BIO_free(bio);
    
    return ret;
}

- (bool) verifySignature:(NSData *)data withSignature:(NSData*)signature andCert:(NSData*)cert {
    X509* x509;
    BIO* bio = BIO_new_mem_buf([cert bytes], (int)[cert length]);
    x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    
    BIO_free(bio);
    
    if (!x509) {
        Log(LOG_E, @"Unable to parse certificate in memory");
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
    BIO* bio = BIO_new_mem_buf([key bytes], (int)[key length]);
    
    EVP_PKEY* pkey;
    pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    
    BIO_free(bio);
    
    if (!pkey) {
        Log(LOG_E, @"Unable to parse private key in memory!");
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

+ (NSData*) readCryptoObject:(NSString*)item {
#if TARGET_OS_TV
    return [[NSUserDefaults standardUserDefaults] dataForKey:item];
#else
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *file = [documentsDirectory stringByAppendingPathComponent:item];
    return [NSData dataWithContentsOfFile:file];
#endif
}

+ (void) writeCryptoObject:(NSString*)item data:(NSData*)data {
#if TARGET_OS_TV
    [[NSUserDefaults standardUserDefaults] setObject:data forKey:item];
#else
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *file = [documentsDirectory stringByAppendingPathComponent:item];
    [data writeToFile:file atomically:NO];
#endif
}

+ (NSData*) readCertFromFile {
    if (cert == nil) {
        cert = [CryptoManager readCryptoObject:@"client.crt"];
    }
    return cert;
}

+ (NSData*) readP12FromFile {
    if (p12 == nil) {
        p12 = [CryptoManager readCryptoObject:@"client.p12"];
    }
    return p12;
}

+ (NSData*) readKeyFromFile {
    if (key == nil) {
        key = [CryptoManager readCryptoObject:@"client.key"];
    }
    return key;
}

+ (bool) keyPairExists {
    bool keyFileExists = [CryptoManager readCryptoObject:@"client.key"] != nil;
    bool p12FileExists = [CryptoManager readCryptoObject:@"client.p12"] != nil;
    bool certFileExists = [CryptoManager readCryptoObject:@"client.crt"] != nil;
    
    return keyFileExists && p12FileExists && certFileExists;
}

+ (NSData *)getSignatureFromCert:(NSData *)cert {
    BIO* bio = BIO_new_mem_buf([cert bytes], (int)[cert length]);
    X509* x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    
    if (!x509) {
        Log(LOG_E, @"Unable to parse certificate in memory!");
        return NULL;
    }
    return [NSData dataWithBytes:x509->signature->data length:x509->signature->length];
}

+ (NSData*)getKeyFromCertKeyPair:(CertKeyPair*)certKeyPair {
    BIO* bio = BIO_new(BIO_s_mem());
    
    PEM_write_bio_PrivateKey(bio, certKeyPair->pkey, NULL, NULL, 0, NULL, NULL);
    
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    NSData* data = [NSData dataWithBytes:mem->data length:mem->length];
    BIO_free(bio);
    return data;
}

+ (NSData*)getP12FromCertKeyPair:(CertKeyPair*)certKeyPair {
    BIO* bio = BIO_new(BIO_s_mem());
    
    i2d_PKCS12_bio(bio, certKeyPair->p12);
    
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    NSData* data = [NSData dataWithBytes:mem->data length:mem->length];
    BIO_free(bio);
    return data;
}

+ (NSData*)getCertFromCertKeyPair:(CertKeyPair*)certKeyPair {
    BIO* bio = BIO_new(BIO_s_mem());
    
    PEM_write_bio_X509(bio, certKeyPair->x509);
    
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    NSData* data = [NSData dataWithBytes:mem->data length:mem->length];
    BIO_free(bio);
    return data;
}

+ (void) generateKeyPairUsingSSL {
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        if (![CryptoManager keyPairExists]) {
            Log(LOG_I, @"Generating Certificate... ");
            CertKeyPair certKeyPair = generateCertKeyPair();
            
            NSData* certData = [CryptoManager getCertFromCertKeyPair:&certKeyPair];
            NSData* p12Data = [CryptoManager getP12FromCertKeyPair:&certKeyPair];
            NSData* keyData = [CryptoManager getKeyFromCertKeyPair:&certKeyPair];
            
            freeCertKeyPair(certKeyPair);
            
            [CryptoManager writeCryptoObject:@"client.crt" data:certData];
            [CryptoManager writeCryptoObject:@"client.p12" data:p12Data];
            [CryptoManager writeCryptoObject:@"client.key" data:keyData];
            
            Log(LOG_I, @"Certificate created");
        }
    });
}

@end
