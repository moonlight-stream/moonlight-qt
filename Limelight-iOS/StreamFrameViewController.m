//
//  StreamFrameViewController.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "StreamFrameViewController.h"
#import "VideoDepacketizer.h"

@interface StreamFrameViewController ()

@end

@implementation StreamFrameViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self.renderView setNeedsDisplay];
    
	// Do any additional setup after loading the view.
    VideoDepacketizer* depacketizer = [[VideoDepacketizer alloc] init];
    
    NSString* path = [[NSBundle mainBundle] pathForResource:@"notpadded"
                                                     ofType:@"h264"];
    NSLog(@"Path: %@", path);
    [depacketizer readFile:path];
    
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
