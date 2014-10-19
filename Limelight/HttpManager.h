//
//  HttpManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/16/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HttpManager : NSObject
- (id) initWithHost:(NSString*) host uniqueId:(NSString*) uniqueId deviceName:(NSString*) deviceName;
- (NSString*) generatePIN;
- (NSData*) saltPIN:(NSString*)PIN;
- (NSURL*) newPairRequestWithSalt:(NSData*)salt andCert:(NSData*)cert;
@end
