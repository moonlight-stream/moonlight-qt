//
//  HttpManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "HttpManager.h"
#import "CryptoManager.h"
#import "App.h"

#include <libxml2/libxml/xmlreader.h>
#include <string.h>

@implementation HttpManager {
    NSString* _baseURL;
    NSString* _host;
    NSString* _uniqueId;
    NSString* _deviceName;
    NSData* _cert;
    NSMutableData* _respData;
    NSData* _requestResp;
    dispatch_semaphore_t _requestLock;
}

static const NSString* PORT = @"47984";

+ (NSData*) fixXmlVersion:(NSData*) xmlData {
    NSString* dataString = [[NSString alloc] initWithData:xmlData encoding:NSUTF8StringEncoding];
    NSString* xmlString = [dataString stringByReplacingOccurrencesOfString:@"UTF-16" withString:@"UTF-8" options:NSCaseInsensitiveSearch range:NSMakeRange(0, [dataString length])];
    
    return [xmlString dataUsingEncoding:NSUTF8StringEncoding];
}

- (id) initWithHost:(NSString*) host uniqueId:(NSString*) uniqueId deviceName:(NSString*) deviceName cert:(NSData*) cert {
    self = [super init];
    _host = host;
    _uniqueId = uniqueId;
    _deviceName = deviceName;
    _cert = cert;
    _baseURL = [[NSString stringWithFormat:@"https://%@:%@", host, PORT]
                stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    _requestLock = dispatch_semaphore_create(0);
    _respData = [[NSMutableData alloc] init];
    return self;
}

- (HttpResponse*) executeRequestSynchronously:(NSURLRequest*)request {
    NSLog(@"Making Request: %@", request);
    [_respData setLength:0];
    dispatch_sync(dispatch_get_main_queue(), ^{
        [NSURLConnection connectionWithRequest:request delegate:self];
    });
    dispatch_semaphore_wait(_requestLock, DISPATCH_TIME_FOREVER);
    return [HttpResponse responseWithData:_requestResp];
}

- (void) executeRequest:(NSURLRequest*)request {
    [NSURLConnection connectionWithRequest:request delegate:self];
}

- (NSURLRequest*) createRequestFromString:(NSString*) urlString enableTimeout:(BOOL)normalTimeout {
    NSString* escapedUrl = [urlString stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    NSURL* url = [[NSURL alloc] initWithString:escapedUrl];
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
    if (normalTimeout) {
        // Timeout the request after 5 seconds
        [request setTimeoutInterval:5];
    }
    else {
        // Timeout the request after 60 seconds
        [request setTimeoutInterval:60];
    }
    return request;
}

- (NSURLRequest*) newPairRequest:(NSData*)salt {
    NSString* urlString = [NSString stringWithFormat:@"%@/pair?uniqueid=%@&devicename=%@&updateState=1&phrase=getservercert&salt=%@&clientcert=%@",
                           _baseURL, _uniqueId, _deviceName, [self bytesToHex:salt], [self bytesToHex:_cert]];
    // This call blocks while waiting for the user to input the PIN on the PC
    return [self createRequestFromString:urlString enableTimeout:FALSE];
}

- (NSURLRequest*) newUnpairRequest {
    NSString* urlString = [NSString stringWithFormat:@"%@/unpair?uniqueid=%@", _baseURL, _uniqueId];
    return [self createRequestFromString:urlString enableTimeout:TRUE];
}

- (NSURLRequest*) newChallengeRequest:(NSData*)challenge {
    NSString* urlString = [NSString stringWithFormat:@"%@/pair?uniqueid=%@&devicename=%@&updateState=1&clientchallenge=%@",
                           _baseURL, _uniqueId, _deviceName, [self bytesToHex:challenge]];
    return [self createRequestFromString:urlString enableTimeout:TRUE];
}

- (NSURLRequest*) newChallengeRespRequest:(NSData*)challengeResp {
    NSString* urlString = [NSString stringWithFormat:@"%@/pair?uniqueid=%@&devicename=%@&updateState=1&serverchallengeresp=%@",
                           _baseURL, _uniqueId, _deviceName, [self bytesToHex:challengeResp]];
    return [self createRequestFromString:urlString enableTimeout:TRUE];
}

- (NSURLRequest*) newClientSecretRespRequest:(NSString*)clientPairSecret {
    NSString* urlString = [NSString stringWithFormat:@"%@/pair?uniqueid=%@&devicename=%@&updateState=1&clientpairingsecret=%@", _baseURL, _uniqueId, _deviceName, clientPairSecret];
    return [self createRequestFromString:urlString enableTimeout:TRUE];
}

- (NSURLRequest*) newPairChallenge {
    NSString* urlString = [NSString stringWithFormat:@"%@/pair?uniqueid=%@&devicename=%@&updateState=1&phrase=pairchallenge", _baseURL, _uniqueId, _deviceName];
    return [self createRequestFromString:urlString enableTimeout:TRUE];
}

- (NSURLRequest *)newAppListRequest {
    NSString* urlString = [NSString stringWithFormat:@"%@/applist?uniqueid=%@", _baseURL, _uniqueId];
    return [self createRequestFromString:urlString enableTimeout:TRUE];
}

- (NSURLRequest *)newServerInfoRequest {
    NSString* urlString = [NSString stringWithFormat:@"%@/serverinfo?uniqueid=%@", _baseURL, _uniqueId];
    return [self createRequestFromString:urlString enableTimeout:TRUE];
}

- (NSURLRequest*) newLaunchRequest:(NSString*)appId width:(int)width height:(int)height refreshRate:(int)refreshRate rikey:(NSString*)rikey rikeyid:(int)rikeyid {
    NSString* urlString = [NSString stringWithFormat:@"%@/launch?uniqueid=%@&appid=%@&mode=%dx%dx%d&additionalStates=1&sops=1&rikey=%@&rikeyid=%d", _baseURL, _uniqueId, appId, width, height, refreshRate, rikey, rikeyid];
    // This blocks while the app is launching
    return [self createRequestFromString:urlString enableTimeout:FALSE];
}

- (NSURLRequest*) newResumeRequestWithRiKey:(NSString*)riKey riKeyId:(int)riKeyId {
    NSString* urlString = [NSString stringWithFormat:@"%@/resume?uniqueid=%@&rikey=%@&rikeyid=%d", _baseURL, _uniqueId, riKey, riKeyId];
    // This blocks while the app is resuming
    return [self createRequestFromString:urlString enableTimeout:FALSE];
}

- (NSURLRequest*) newQuitAppRequest {
    NSString* urlString = [NSString stringWithFormat:@"%@/cancel?uniqueid=%@", _baseURL, _uniqueId];
    return [self createRequestFromString:urlString enableTimeout:FALSE];
}

- (NSURLRequest*) newAppAssetRequestWithAppId:(NSString *)appId {
    NSString* urlString = [NSString stringWithFormat:@"%@/appasset?uniqueid=%@&appid=%@&AssetType=2&AssetIdx=0", _baseURL, _uniqueId, appId];
    return [self createRequestFromString:urlString enableTimeout:FALSE];
}

- (NSString*) bytesToHex:(NSData*)data {
    const unsigned char* bytes = [data bytes];
    NSMutableString *hex = [[NSMutableString alloc] init];
    for (int i = 0; i < [data length]; i++) {
        [hex appendFormat:@"%02X" , bytes[i]];
    }
    return hex;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    NSLog(@"Received response: %@", response);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
    NSLog(@"\n\nReceived data: %@\n\n", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    [_respData appendData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
    if ([[NSString alloc] initWithData:_respData encoding:NSUTF8StringEncoding] != nil) {
        _requestResp = [HttpManager fixXmlVersion:_respData];
    } else {
        _requestResp = _respData;
    }
    dispatch_semaphore_signal(_requestLock);
}

- (void)connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    SecIdentityRef identity = [self getClientCertificate];
    NSArray *certArray = [self getCertificate:identity];
    
    NSURLCredential *newCredential = [NSURLCredential credentialWithIdentity:identity certificates:certArray persistence:NSURLCredentialPersistencePermanent];
    
    [challenge.sender useCredential:newCredential forAuthenticationChallenge:challenge];
}

// Returns an array containing the certificate
- (NSArray*)getCertificate:(SecIdentityRef) identity {
    SecCertificateRef certificate = nil;
    
    SecIdentityCopyCertificate(identity, &certificate);
    
    return [[NSArray alloc] initWithObjects:(__bridge id)certificate, nil];
}

// Returns the identity
- (SecIdentityRef)getClientCertificate {
    SecIdentityRef identityApp = nil;
    CFDataRef p12Data = (__bridge CFDataRef)[CryptoManager readP12FromFile];

    CFStringRef password = CFSTR("limelight");
    const void *keys[] = { kSecImportExportPassphrase };
    const void *values[] = { password };
    CFDictionaryRef options = CFDictionaryCreate(NULL, keys, values, 1, NULL, NULL);
    CFArrayRef items = CFArrayCreate(NULL, 0, 0, NULL);
    OSStatus securityError = SecPKCS12Import(p12Data, options, &items);

    if (securityError == errSecSuccess) {
        //NSLog(@"Success opening p12 certificate. Items: %ld", CFArrayGetCount(items));
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
    dispatch_semaphore_signal(_requestLock);
}

@end
