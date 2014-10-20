//
//  HttpManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HttpManager : NSObject <NSURLConnectionDelegate, NSURLConnectionDataDelegate>

+ (NSString*) getStringFromXML:(NSData*)xml tag:(NSString*)tag;

- (id) initWithHost:(NSString*) host uniqueId:(NSString*) uniqueId deviceName:(NSString*) deviceName cert:(NSData*) cert;
- (NSURLRequest*) newPairRequest:(NSData*)salt;
- (NSURLRequest*) newUnpairRequest;
- (NSURLRequest*) newChallengeRequest:(NSData*)challenge;
- (NSURLRequest*) newChallengeRespRequest:(NSData*)challengeResp;
- (NSURLRequest*) newClientSecretRespRequest:(NSString*)clientPairSecret;
- (NSURLRequest*) newPairChallenge;
- (NSURLRequest*) newAppListRequest;
- (NSURLRequest*) newServerInfoRequest;
- (NSURLRequest*) newLaunchRequest:(NSString*)appId width:(int)width height:(int)height refreshRate:(int)refreshRate rikey:(NSString*)rikey rikeyid:(int)rikeyid;
- (NSData*) executeRequestSynchronously:(NSURLRequest*)request;
@end


