//
//  mkcert.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#ifndef Limelight_mkcert_h
#define Limelight_mkcert_h

#include <openssl/x509v3.h>

typedef struct CertKeyPair {
    X509 *x509;
    EVP_PKEY *pkey;
} CertKeyPair;

struct CertKeyPair generateCertKeyPair();
void freeCertKeyPair(CertKeyPair);
void saveCertKeyPair(const char* certFile, const char* keyPairFile, CertKeyPair certKeyPair);
#endif
