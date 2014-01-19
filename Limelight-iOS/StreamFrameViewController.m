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
    StreamView* streamView = [[StreamView alloc] initWithFrame:self.view.frame];
    streamView.backgroundColor = [UIColor blackColor];
    
    [self.view addSubview:streamView];
    
    [streamView setNeedsDisplay];

	// Do any additional setup after loading the view.
    NSString* path = [[NSBundle mainBundle] pathForResource:@"notpadded"
                                                     ofType:@"h264"];
    NSLog(@"Path: %@", path);
    VideoDepacketizer* depacketizer = [[VideoDepacketizer alloc] initWithFile:path renderTarget:streamView];
    NSOperationQueue* opQueue = [[NSOperationQueue alloc] init];
    [opQueue addOperation:depacketizer];

    
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
