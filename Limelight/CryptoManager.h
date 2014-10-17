//
//  CryptoManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface CryptoManager : NSObject <NSURLConnectionDelegate>

- (void) generateKeyPairUsingSSl;

@end
