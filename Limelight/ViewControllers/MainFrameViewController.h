//
//  MainFrameViewController.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/17/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "MDNSManager.h"
#import "PairManager.h"
#import "StreamConfiguration.h"

@interface MainFrameViewController : UIViewController <MDNSCallback, PairCallback, NSURLConnectionDelegate>

+ (StreamConfiguration*) getStreamConfiguration;

@end
