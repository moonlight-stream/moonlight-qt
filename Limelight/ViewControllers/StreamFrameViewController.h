//
//  StreamFrameViewController.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "Connection.h"
#import "StreamConfiguration.h"

#import <UIKit/UIKit.h>

@interface StreamFrameViewController : UIViewController <ConnectionCallbacks>
@property (strong, nonatomic) IBOutlet UILabel *stageLabel;
@property (strong, nonatomic) IBOutlet UIActivityIndicatorView *spinner;
@property (nonatomic) StreamConfiguration* streamConfig;

@end
