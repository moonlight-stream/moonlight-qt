
#include "mkcert.h"

#include <stdio.h>
#include <stdlib.h>

#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/pkcs12.h>

#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

static const int NUM_BITS = 2048;
static const int SERIAL = 0;
static const int NUM_YEARS = 10;

int mkcert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int years);
int add_ext(X509 *cert, int nid, char *value);

struct CertKeyPair generateCertKeyPair() {
    BIO *bio_err;
    X509 *x509 = NULL;
    EVP_PKEY *pkey = NULL;
    PKCS12 *p12 = NULL;
   
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
    bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
    
    SSLeay_add_all_algorithms();
    ERR_load_crypto_strings();
    
    mkcert(&x509, &pkey, NUM_BITS, SERIAL, NUM_YEARS);

    p12 = PKCS12_create("limelight", "GameStream", pkey, x509, NULL, 0, 0, 0, 0, 0);
    if (p12 == NULL) {
        printf("Error generating a valid PKCS12 certificate.\n");
    }

    // Debug Print statements
    //RSA_print_fp(stdout, pkey->pkey.rsa, 0);
    //X509_print_fp(stdout, x509);
    //PEM_write_PUBKEY(stdout, pkey);
    //PEM_write_PrivateKey(stdout, pkey, NULL, NULL, 0, NULL, NULL);
    //PEM_write_X509(stdout, x509);
    
#ifndef OPENSSL_NO_ENGINE
    ENGINE_cleanup();
#endif
    CRYPTO_cleanup_all_ex_data();
    
    CRYPTO_mem_leaks(bio_err);
    BIO_free(bio_err);
    
    return (CertKeyPair){x509, pkey, p12};
}

void freeCertKeyPair(struct CertKeyPair certKeyPair) {
    X509_free(certKeyPair.x509);
    EVP_PKEY_free(certKeyPair.pkey);
    PKCS12_free(certKeyPair.p12);
}

void saveCertKeyPair(const char* certFile, const char* p12File, const char* keyPairFile, CertKeyPair certKeyPair) {
    FILE* certFilePtr = fopen(certFile, "w");
    FILE* keyPairFilePtr = fopen(keyPairFile, "w");
    FILE* p12FilePtr = fopen(p12File, "wb");
    
    //TODO: error check
    PEM_write_PrivateKey(keyPairFilePtr, certKeyPair.pkey, NULL, NULL, 0, NULL, NULL);
    PEM_write_X509(certFilePtr, certKeyPair.x509);
    i2d_PKCS12_fp(p12FilePtr, certKeyPair.p12);
    
    fclose(p12FilePtr);
    fclose(certFilePtr);
    fclose(keyPairFilePtr);
}

int mkcert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int years) {
    X509 *x;
    EVP_PKEY *pk;
    RSA *rsa;
    X509_NAME *name = NULL;
    
    if ((pkeyp == NULL) || (*pkeyp == NULL)) {
        if ((pk=EVP_PKEY_new()) == NULL) {
            abort();
            return(0);
        }
    } else {
        pk = *pkeyp;
    }
    
    if ((x509p == NULL) || (*x509p == NULL)) {
        if ((x = X509_new()) == NULL) {
            goto err;
        }
    } else {
        x = *x509p;
    }
    
    rsa = RSA_generate_key(bits, RSA_F4, NULL, NULL);
    if (!EVP_PKEY_assign_RSA(pk, rsa)) {
        abort();
        goto err;
    }
    
    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), serial);
    X509_gmtime_adj(X509_get_notBefore(x), 0);
    X509_gmtime_adj(X509_get_notAfter(x), (long)60*60*24*365*years);
    X509_set_pubkey(x, pk);
    
    name = X509_get_subject_name(x);
    
    /* This function creates and adds the entry, working out the
     * correct string type and performing checks on its length.
     */
    X509_NAME_add_entry_by_txt(name,"CN", MBSTRING_ASC, (unsigned char*)"NVIDIA GameStream Client", -1, -1, 0);
    
    /* Its self signed so set the issuer name to be the same as the
     * subject.
     */
    X509_set_issuer_name(x, name);
    
    /* Add various extensions: standard extensions */
    add_ext(x, NID_basic_constraints, "critical,CA:TRUE");
    add_ext(x, NID_key_usage, "critical,keyCertSign,cRLSign");
    
    add_ext(x, NID_subject_key_identifier, "hash");
    
    if (!X509_sign(x, pk, EVP_sha1())) {
        goto err;
    }
    
    *x509p = x;
    *pkeyp = pk;
    
    return(1);
err:
    return(0);
}

/* Add extension using V3 code: we can set the config file as NULL
 * because we wont reference any other sections.
 */

int add_ext(X509 *cert, int nid, char *value)
{
    X509_EXTENSION *ex;
    X509V3_CTX ctx;
    /* This sets the 'context' of the extensions. */
    /* No configuration database */
    X509V3_set_ctx_nodb(&ctx);
    /* Issuer and subject certs: both the target since it is self signed,
     * no request and no CRL
     */
    X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
    ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);
    if (!ex) {
        return 0;
    }
    
    X509_add_ext(cert, ex, -1);
    X509_EXTENSION_free(ex);
    return 1;
}

