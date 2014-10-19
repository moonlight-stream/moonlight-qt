//
//  MainFrameViewController.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/17/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "MainFrameViewController.h"
#import "ConnectionHandler.h"
#import "Computer.h"
#import "CryptoManager.h"
#import "HttpManager.h"

@implementation MainFrameViewController
NSString* hostAddr = @"cement-truck.case.edu";
MDNSManager* mDNSManager;

+ (const char*)getHostAddr
{
    return [hostAddr UTF8String];
}

- (void)PairButton:(UIButton *)sender
{
    NSLog(@"Pair Button Pressed!");
    [ConnectionHandler pairWithHost:hostAddr];
}

- (void)StreamButton:(UIButton *)sender
{
    NSLog(@"Stream Button Pressed!");
    [ConnectionHandler streamWithHost:hostAddr viewController:self];
}

- (void) segueIntoStream {
    [self performSegueWithIdentifier:@"createStreamFrame" sender:self];
}

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    if (pickerView == self.StreamConfigs) {
        return [self.streamConfigVals objectAtIndex:row];
    } else if (pickerView == self.HostPicker) {
        return ((Computer*)([self.hostPickerVals objectAtIndex:row])).displayName;
    } else {
        return nil;
    }
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    //self.hostPickerVals = [mDNSManager getFoundHosts];
    
    //[self.HostPicker reloadAllComponents];
    if (pickerView == self.HostPicker) {
        hostAddr = ((Computer*)([self.hostPickerVals objectAtIndex:[self.HostPicker selectedRowInComponent:0]])).hostName;
    }
    
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
    if (pickerView == self.StreamConfigs) {
        return self.streamConfigVals.count;
    } else if (pickerView == self.HostPicker) {
        return self.hostPickerVals.count;
    } else {
        return 0;
    }
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.streamConfigVals = [[NSArray alloc] initWithObjects:@"1280x720 (30Hz)", @"1280x720 (60Hz)", @"1920x1080 (30Hz)", @"1920x1080 (60Hz)",nil];
    self.hostPickerVals = [[NSArray alloc] init];
    
    mDNSManager = [[MDNSManager alloc] initWithCallback:self];
    //[mDNSManager searchForHosts];
    
    [CryptoManager generateKeyPairUsingSSl];
    NSString* uniqueId = [CryptoManager getUniqueID];
    NSData* cert = [CryptoManager readCertFromFile];
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:hostAddr uniqueId:uniqueId deviceName:@"roth" cert:cert];
    NSString* PIN = [hMan generatePIN];
    NSData* saltedPIN = [hMan saltPIN:PIN];
    NSLog(@"PIN: %@, saltedPIN: %@", PIN, saltedPIN);
    NSURL* pairUrl = [hMan newPairRequest];
    NSURLRequest* pairRequest = [[NSURLRequest alloc] initWithURL:pairUrl];
    // NSLog(@"making pair request: %@", [pairRequest description]);
    [NSURLConnection connectionWithRequest:pairRequest delegate:hMan];
}

- (void)updateHosts:(NSArray *)hosts {
    self.hostPickerVals = hosts;
    [self.HostPicker reloadAllComponents];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
@end
