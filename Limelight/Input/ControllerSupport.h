//
//  ControllerSupport.h
//  Limelight
//
//  Created by Cameron Gutman on 10/20/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface ControllerSupport : NSObject

-(id) init;

-(void) cleanup;

@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@end
