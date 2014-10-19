//
//  HttpManager.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "HttpManager.h"

@implementation HttpManager {
    NSString* _baseURL;
    NSString* _host;
    NSString* _uniqueId;
    NSString* _deviceName;
}

static const NSString* PORT = @"47984";


- (id) initWithHost:(NSString*) host uniqueId:(NSString*) uniqueId deviceName:(NSString*) deviceName {
    self = [super init];
    _host = host;
    _uniqueId = uniqueId;
    _deviceName = deviceName;
    _baseURL = [[NSString stringWithFormat:@"https://%@:%@", host, PORT]
                stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    return self;
}

- (NSURL*) newPairRequestWithSalt:(NSString*)salt andCert:(NSString*)cert {
    NSURL* url = [[NSURL alloc] initWithString:
                  [NSString stringWithFormat:@"http://%@:%@/pair?uniqueid=%@&devicename=%@&updateState=1&phrase=getservercert&salt=%@&clientcert=%@",
                                                _host, PORT, _uniqueId, _deviceName, salt, cert]];
    return url;
}

- (void) initiatePairing {
    
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
    
    NSLog(@"Salted PIN: %@", [saltedPIN description]);

    return saltedPIN;
}

- (NSData*) randomBytes:(NSInteger)length {
    char* bytes = malloc(length);
    arc4random_buf(bytes, length);
    NSData* randomData = [NSData dataWithBytes:bytes length:length];
    free(bytes);
    return randomData;
}

@end
