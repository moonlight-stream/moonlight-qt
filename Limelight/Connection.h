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

-(id) initWithHost:(int)ipaddr key:(NSData*)rikey keyId:(int)rikeyid width:(int)width height:(int)height refreshRate:(int)refreshRate renderer:(VideoDecoderRenderer*)myRenderer;
-(void) main;

@end
