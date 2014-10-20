//
//  StreamManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/20/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface StreamManager : NSOperation

- (id) initWithHost:(NSString*)host renderView:(UIView*)view;

@end
