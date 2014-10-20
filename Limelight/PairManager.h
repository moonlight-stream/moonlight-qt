//
//  PairManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/19/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HttpManager.h"

@interface PairManager : NSOperation
- (id) initWithManager:(HttpManager*)httpManager andCert:(NSData*)cert;
- (NSString*) generatePIN;
- (NSData*) saltPIN:(NSString*)PIN;
- (void) initiatePair;
@end
