//
//  MainFrameViewController.h
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/17/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface MainFrameViewController : UIViewController <UIPickerViewDataSource,UIPickerViewDelegate>
- (IBAction)StreamButton:(UIButton *)sender;
- (IBAction)PairButton:(UIButton *)sender;
@property (strong, nonatomic) IBOutlet UITextField *HostField;
@property (strong, nonatomic) IBOutlet UIPickerView *StreamConfigs;
@property (strong, nonatomic) NSArray* streamConfigVals;

+ (const char*)getHostAddr;

@end
