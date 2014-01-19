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
    
    
    
    CGAffineTransform transform = CGAffineTransformMakeTranslation((streamView.frame.size.height/2) - (streamView.frame.size.width/2), (streamView.frame.size.width/2) - (streamView.frame.size.height/2));
    transform = CGAffineTransformRotate(transform, M_PI_2);
    transform = CGAffineTransformScale(transform, -1, -1);
    streamView.transform = transform;
    
    // Repositions and resizes the view.
    CGRect contentRect = CGRectMake(0,0, self.view.frame.size.width, self.view.frame.size.height);
    streamView.bounds = contentRect;
	
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
