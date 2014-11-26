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
#import "UIComputerView.h"
#import "UIAppView.h"
#import "AppManager.h"
#import "SWRevealViewController.h"

@interface MainFrameViewController : UIViewController <MDNSCallback, PairCallback, HostCallback, AppCallback, AppAssetCallback, NSURLConnectionDelegate, SWRevealViewControllerDelegate>
@property (strong, nonatomic) IBOutlet UIBarButtonItem *settingsSidebarButton;

+ (StreamConfiguration*) getStreamConfiguration;

@end
