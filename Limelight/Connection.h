//
//  Connection.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VideoDecoderRenderer.h"
#import "StreamConfiguration.h"

@protocol ConTermCallback <NSObject>

- (void) connectionTerminated;

@end

@interface Connection : NSOperation <NSStreamDelegate>

-(id) initWithConfig:(StreamConfiguration*)config renderer:(VideoDecoderRenderer*)myRenderer connectionTerminatedCallback:(id<ConTermCallback>)callback;
-(void) terminate;
-(void) main;

@end
