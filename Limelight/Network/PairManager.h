//
//  PairManager.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/19/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HttpManager.h"

@protocol PairCallback <NSObject>

- (void) showPIN:(NSString*)PIN;
- (void) pairSuccessful;
- (void) pairFailed:(NSString*)message;
- (void) alreadyPaired;

@end

@interface PairManager : NSOperation
- (id) initWithManager:(HttpManager*)httpManager andCert:(NSData*)cert callback:(id<PairCallback>)callback;
- (NSString*) generatePIN;
- (NSData*) saltPIN:(NSString*)PIN;
- (void) initiatePair:(int)serverMajorVersion;

@end
