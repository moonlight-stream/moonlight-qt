//
//  ConnectionHandler.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/27/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface ConnectionHandler : NSOperation
@property NSString* hostName;
@property int mode;

+ (void) pairWithHost:(NSString*)host;
+ (void) streamWithHost:(NSString*)host viewController:(UIViewController*)viewCont;
+ (NSString*) resolveHost:(NSString*)host;

@end
