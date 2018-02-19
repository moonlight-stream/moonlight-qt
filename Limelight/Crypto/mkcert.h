//
//  mkcert.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#ifndef Limelight_mkcert_h
#define Limelight_mkcert_h

#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>

typedef struct CertKeyPair {
    X509 *x509;
    EVP_PKEY *pkey;
    PKCS12 *p12;
} CertKeyPair;

struct CertKeyPair generateCertKeyPair(void);
void freeCertKeyPair(CertKeyPair);
void saveCertKeyPair(const char* certFile, const char* p12File, const char* keyPairFile, CertKeyPair certKeyPair);
#endif

