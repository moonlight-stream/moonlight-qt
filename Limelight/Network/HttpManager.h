//
//  HttpManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "HttpResponse.h"
#import "HttpRequest.h"
#import "StreamConfiguration.h"

@interface HttpManager : NSObject <NSURLSessionDelegate>

- (id) initWithHost:(NSString*) host uniqueId:(NSString*) uniqueId serverCert:(NSData*) serverCert;
- (void) setServerCert:(NSData*) serverCert;
- (NSURLRequest*) newPairRequest:(NSData*)salt clientCert:(NSData*)clientCert;
- (NSURLRequest*) newUnpairRequest;
- (NSURLRequest*) newChallengeRequest:(NSData*)challenge;
- (NSURLRequest*) newChallengeRespRequest:(NSData*)challengeResp;
- (NSURLRequest*) newClientSecretRespRequest:(NSString*)clientPairSecret;
- (NSURLRequest*) newPairChallenge;
- (NSURLRequest*) newAppListRequest;
- (NSURLRequest*) newServerInfoRequest:(bool)fastFail;
- (NSURLRequest*) newHttpServerInfoRequest:(bool)fastFail;
- (NSURLRequest*) newHttpServerInfoRequest;
- (NSURLRequest*) newLaunchRequest:(StreamConfiguration*)config;
- (NSURLRequest*) newResumeRequest:(StreamConfiguration*)config;
- (NSURLRequest*) newQuitAppRequest;
- (NSURLRequest*) newAppAssetRequestWithAppId:(NSString*)appId;
- (void) executeRequestSynchronously:(HttpRequest*)request;

@end


