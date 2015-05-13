//
//  Connection.h
//  Moonlight
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VideoDecoderRenderer.h"
#import "StreamConfiguration.h"

@protocol ConnectionCallbacks <NSObject>

- (void) connectionStarted;
- (void) connectionTerminated:(long)errorCode;
- (void) stageStarting:(char*)stageName;
- (void) stageComplete:(char*)stageName;
- (void) stageFailed:(char*)stageName withError:(long)errorCode;
- (void) launchFailed:(NSString*)message;
- (void) displayMessage:(char*)message;
- (void) displayTransientMessage:(char*)message;

@end

@interface Connection : NSOperation <NSStreamDelegate>

-(id) initWithConfig:(StreamConfiguration*)config renderer:(VideoDecoderRenderer*)myRenderer connectionCallbacks:(id<ConnectionCallbacks>)callbacks serverMajorVersion:(int)serverMajorVersion;
-(void) terminate;
-(void) terminateInternal;
-(void) main;

@end
