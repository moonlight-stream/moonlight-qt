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
#import "PairManager.h"
#import "Connection.h"
#import "VideoDecoderRenderer.h"

@implementation MainFrameViewController {
    NSOperationQueue* _opQueue;
    MDNSManager* _mDNSManager;
    Computer* _selectedHost;
}

- (int)getHostAddr {
    return [_selectedHost resolveHost];
}

- (void)PairButton:(UIButton *)sender
{
    NSLog(@"Pair Button Pressed!");
    [CryptoManager generateKeyPairUsingSSl];
    NSString* uniqueId = [CryptoManager getUniqueID];
    NSData* cert = [CryptoManager readCertFromFile];
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_selectedHost.hostName uniqueId:uniqueId deviceName:@"roth" cert:cert];
    PairManager* pMan = [[PairManager alloc] initWithManager:hMan andCert:cert];

    [_opQueue addOperation:pMan];
}

- (void)StreamButton:(UIButton *)sender
{
    NSLog(@"Stream Button Pressed!");
    [self segueIntoStream];
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
    if (pickerView == self.HostPicker) {
        _selectedHost = (Computer*)([self.hostPickerVals objectAtIndex:[self.HostPicker selectedRowInComponent:0]]);
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
    
    
    _mDNSManager = [[MDNSManager alloc] initWithCallback:self];
    [_mDNSManager searchForHosts];
    _opQueue = [[NSOperationQueue alloc] init];
}

- (void)updateHosts:(NSArray *)hosts {
    self.hostPickerVals = hosts;
    [self.HostPicker reloadAllComponents];
    _selectedHost = (Computer*)([self.hostPickerVals objectAtIndex:[self.HostPicker selectedRowInComponent:0]]);
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
@end
