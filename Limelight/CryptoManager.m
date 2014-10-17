//
//  CryptoManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "CryptoManager.h"
#import "mkcert.h"


@implementation CryptoManager

- (void)connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    
    
}

- (void) generateKeyPairUsingSSl {
    NSLog(@"Generating Certificate: ");
    CertKeyPair certKeyPair = generateCertKeyPair();
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *certFile = [documentsDirectory stringByAppendingPathComponent:@"client.crt"];
    NSString *keyPairFile = [documentsDirectory stringByAppendingPathComponent:@"client.key"];
    
    NSLog(@"Writing cert and key to: \n%@\n%@", certFile, keyPairFile);
    saveCertKeyPair([certFile UTF8String], [keyPairFile UTF8String], certKeyPair);
    freeCertKeyPair(certKeyPair);
}

@end
