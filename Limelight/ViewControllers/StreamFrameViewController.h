//
//  StreamFrameViewController.h
//  Moonlight
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "Connection.h"
#import "StreamConfiguration.h"
#import "StreamView.h"

#import <UIKit/UIKit.h>

#if TARGET_OS_TV
@import GameController;

@interface StreamFrameViewController : GCEventViewController <ConnectionCallbacks, EdgeDetectionDelegate>
#else
@interface StreamFrameViewController : UIViewController <ConnectionCallbacks, EdgeDetectionDelegate>
#endif
@property (strong, nonatomic) IBOutlet UILabel *stageLabel;
@property (strong, nonatomic) IBOutlet UIActivityIndicatorView *spinner;
@property (nonatomic) StreamConfiguration* streamConfig;

@end
