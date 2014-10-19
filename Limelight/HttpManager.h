//
//  HttpManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HttpManager : NSObject <NSURLConnectionDelegate, NSURLConnectionDataDelegate>
- (id) initWithHost:(NSString*) host uniqueId:(NSString*) uniqueId deviceName:(NSString*) deviceName cert:(NSData*) cert;
- (NSString*) generatePIN;
- (NSData*) saltPIN:(NSString*)PIN;
- (NSURL*) newPairRequest;
@end
