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

@implementation CryptoManager

// TODO: these three methods are almost identical, fix the copy-pasta
+ (NSData*) readCertFromFile {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *certFile = [documentsDirectory stringByAppendingPathComponent:@"client.crt"];
    return [NSData dataWithContentsOfFile:certFile];
}

+ (NSData*) readP12FromFile {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *p12File = [documentsDirectory stringByAppendingPathComponent:@"client.p12"];
    return [NSData dataWithContentsOfFile:p12File];
}

+ (NSData*) readKeyFromFile {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *keyFile = [documentsDirectory stringByAppendingPathComponent:@"client.key"];
    return [NSData dataWithContentsOfFile:keyFile];
}

+ (void) generateKeyPairUsingSSl {
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
