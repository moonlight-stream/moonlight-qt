//
//  Connection.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VideoDecoderRenderer.h"

@interface Connection : NSOperation <NSStreamDelegate>

-(id) initWithHost:(int)ipaddr width:(int)width height:(int)height renderer:(VideoDecoderRenderer*)renderer;
-(void) main;

@end
