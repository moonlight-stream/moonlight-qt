//
//  MainFrameViewController.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/17/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "MainFrameViewController.h"
#import "VideoDepacketizer.h"

@interface MainFrameViewController ()

@end

@implementation MainFrameViewController
static NSString* hostAddr;

+ (const char*)getHostAddr
{
    return [hostAddr UTF8String];
}

- (void)PairButton:(UIButton *)sender
{
    NSLog(@"Pair Button Pressed!");
}

- (void)StreamButton:(UIButton *)sender
{
    NSLog(@"Stream Button Pressed!");
    //67339056
    hostAddr = self.HostField.text;
    NSString* host = [NSString stringWithFormat:@"http://%@:47989/launch?uniqueid=0&appid=67339056", self.HostField.text];
    NSLog(@"host: %@", host);
    
    NSURL* url = [[NSURL alloc] initWithString:host];
    NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
    [request setHTTPMethod:@"GET"];
    NSURLResponse* response = nil;
    NSData *response1 = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:NULL];
    NSLog(@"url response: %@", response1);
    
    [self performSegueWithIdentifier:@"createStreamFrame" sender:self];
}

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    return [self.streamConfigVals objectAtIndex:row];
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    //TODO: figure out how to save this info!!
}

// returns the number of 'columns' to display.
- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

// returns the # of rows in each component..
- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
    return 4;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view.
    self.streamConfigVals = [[NSArray alloc] initWithObjects:@"1280x720 (30Hz)",@"1280x720 (60Hz)",@"1920x1080 (30Hz)",@"1920x1080 (60Hz)",nil];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
@end
