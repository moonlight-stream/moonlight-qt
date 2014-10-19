//
//  HttpManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "HttpManager.h"
#import "CryptoManager.h"

@implementation HttpManager {
    NSString* _baseURL;
    NSString* _host;
    NSString* _uniqueId;
    NSString* _deviceName;
    NSData* _salt;
    NSData* _cert;
}

static const NSString* PORT = @"47984";


- (id) initWithHost:(NSString*) host uniqueId:(NSString*) uniqueId deviceName:(NSString*) deviceName cert:(NSData*) cert {
    self = [super init];
    _host = host;
    _uniqueId = uniqueId;
    _deviceName = deviceName;
    _cert = cert;
    _baseURL = [[NSString stringWithFormat:@"https://%@:%@", host, PORT]
                stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    return self;
}

- (NSURL*) newPairRequest {
    //NSLog(@"host: %@ \nport: %@ \nuniqueID: %@ \ndeviceName: %@ \nsalt: %@ \ncert: %@", _host, PORT, _uniqueId, _deviceName, salt, cert);
    NSString* urlString = [[NSString stringWithFormat:@"%@/pair?uniqueid=%@&devicename=%@&updateState=1&phrase=getservercert&salt=%@&clientcert=%@", _baseURL, _uniqueId, _deviceName, [self bytesToHex:_salt], [self bytesToHex:_cert]] stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    NSURL* url = [[NSURL alloc] initWithString:urlString];
    //NSLog(@"pair url: %@", url);
    return url;
}

- (NSString*) bytesToHex:(NSData*)data {
    const unsigned char* bytes = [data bytes];
    NSMutableString *hex = [[NSMutableString alloc] init];
    for (int i = 0; i < [data length]; i++) {
        [hex appendFormat:@"%02X" , bytes[i]];
    }
    return hex;
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
    _salt = saltedPIN;
    return saltedPIN;
}

- (NSData*) randomBytes:(NSInteger)length {
    char* bytes = malloc(length);
    arc4random_buf(bytes, length);
    NSData* randomData = [NSData dataWithBytes:bytes length:length];
    free(bytes);
    return randomData;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    NSLog(@"Received response: %@", response);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
    NSLog(@"Received data: %@", data);
}

- (void)connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    SecIdentityRef identity = [self getClientCertificate];  // Go get a SecIdentityRef
    CFArrayRef certs = [self getCertificate:identity]; // Get an array of certificates
    // Convert the CFArrayRef to a NSArray
    NSArray *myArray = (__bridge NSArray *)certs;
    
    // Create the NSURLCredential
    NSURLCredential *newCredential = [NSURLCredential credentialWithIdentity:identity certificates:myArray persistence:NSURLCredentialPersistencePermanent];
    
    // Send
    [challenge.sender useCredential:newCredential forAuthenticationChallenge:challenge];
}

// Returns an array containing the certificate
- (CFArrayRef)getCertificate:(SecIdentityRef) identity {
    SecCertificateRef certificate = nil;
    
    SecIdentityCopyCertificate(identity, &certificate);
    SecCertificateRef certs[1] = { certificate };
    
    CFArrayRef array = CFArrayCreate(NULL, (const void **) certs, 1, NULL);
    
    SecPolicyRef myPolicy   = SecPolicyCreateBasicX509();
    SecTrustRef myTrust;
    
    OSStatus status = SecTrustCreateWithCertificates(array, myPolicy, &myTrust);
    if (status == noErr) {
        NSLog(@"No Err creating certificate");
    } else {
        NSLog(@"Possible Err Creating certificate");
    }
    return array;
}

// Returns the identity
- (SecIdentityRef)getClientCertificate {
    SecIdentityRef identityApp = nil;
    NSData *PKCS12Data = [CryptoManager readP12FromFile];
    CFDataRef inPKCS12Data = (__bridge CFDataRef)PKCS12Data;
   
    CFStringRef password = CFSTR("limelight"); // no password
    const void *keys[] = { kSecImportExportPassphrase };//kSecImportExportPassphrase };
    const void *values[] = { password };
    CFDictionaryRef options = CFDictionaryCreate(NULL, keys, values, 1, NULL, NULL);
    CFArrayRef items = CFArrayCreate(NULL, 0, 0, NULL);
    OSStatus securityError = SecPKCS12Import(inPKCS12Data, options, &items);

    if (securityError == errSecSuccess) {
        NSLog(@"Success opening p12 certificate. Items: %ld", CFArrayGetCount(items));
        CFDictionaryRef identityDict = CFArrayGetValueAtIndex(items, 0);
        identityApp = (SecIdentityRef)CFDictionaryGetValue(identityDict, kSecImportItemIdentity);
    } else {
        NSLog(@"Error opening Certificate.");
    }
    
    CFRelease(options);
    CFRelease(password);
    
    return identityApp;
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error {
    NSLog(@"connection error: %@", error);
}

@end
