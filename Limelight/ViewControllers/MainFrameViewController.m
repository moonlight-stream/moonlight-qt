
//  MainFrameViewController.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/17/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "MainFrameViewController.h"
#import "Computer.h"
#import "CryptoManager.h"
#import "HttpManager.h"
#import "Connection.h"
#import "VideoDecoderRenderer.h"
#import "StreamManager.h"
#import "Utils.h"
#import "UIComputerView.h"
#import "UIAppView.h"
#import "App.h"

@implementation MainFrameViewController {
    NSOperationQueue* _opQueue;
    MDNSManager* _mDNSManager;
    Computer* _selectedHost;
    UIAlertView* _pairAlert;
}
static StreamConfiguration* streamConfig;

+ (StreamConfiguration*) getStreamConfiguration {
    return streamConfig;
}

//TODO: no more pair button
/*
- (void)PairButton:(UIButton *)sender
{
    NSLog(@"Pair Button Pressed!");
    if ([self.hostTextField.text length] > 0) {
        _selectedHost = [[Computer alloc] initWithIp:self.hostTextField.text];
        NSLog(@"Using custom host: %@", self.hostTextField.text);
    }

    if (![self validatePcSelected]) {
        NSLog(@"No valid PC selected");
        return;
    }
    
    [CryptoManager generateKeyPairUsingSSl];
    NSString* uniqueId = [CryptoManager getUniqueID];
    NSData* cert = [CryptoManager readCertFromFile];
    
    if ([Utils resolveHost:_selectedHost.hostName] == 0) {
        [self displayDnsFailedDialog];
        return;
    }
    
    HttpManager* hMan = [[HttpManager alloc] initWithHost:_selectedHost.hostName uniqueId:uniqueId deviceName:@"roth" cert:cert];
    PairManager* pMan = [[PairManager alloc] initWithManager:hMan andCert:cert callback:self];

    [_opQueue addOperation:pMan];
}
*/

- (void)showPIN:(NSString *)PIN {
    dispatch_sync(dispatch_get_main_queue(), ^{
        _pairAlert = [[UIAlertView alloc] initWithTitle:@"Pairing" message:[NSString stringWithFormat:@"Enter the following PIN on the host machine: %@", PIN]delegate:self cancelButtonTitle:@"Ok" otherButtonTitles:nil, nil];
        [_pairAlert show];
    });
}

- (void)pairFailed:(NSString *)message {
    dispatch_sync(dispatch_get_main_queue(), ^{
        [_pairAlert dismissWithClickedButtonIndex:0 animated:NO];
        _pairAlert = [[UIAlertView alloc] initWithTitle:@"Pairing Failed" message:message delegate:self cancelButtonTitle:@"Ok" otherButtonTitles:nil, nil];
        [_pairAlert show];
    });
}

- (void)pairSuccessful {
    dispatch_sync(dispatch_get_main_queue(), ^{
        [_pairAlert dismissWithClickedButtonIndex:0 animated:NO];
        _pairAlert = [[UIAlertView alloc] initWithTitle:@"Pairing Succesful" message:@"Successfully paired to host" delegate:self cancelButtonTitle:@"Ok" otherButtonTitles:nil, nil];
        [_pairAlert show];
    });
}

- (void)displayDnsFailedDialog {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Network Error"
                                                                   message:@"Failed to resolve host."
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}

//TODO: No more stream button
/*
- (void)StreamButton:(UIButton *)sender
{
    NSLog(@"Stream Button Pressed!");
    
    if ([self.hostTextField.text length] > 0) {
        _selectedHost = [[Computer alloc] initWithIp:self.hostTextField.text];
        NSLog(@"Using custom host: %@", self.hostTextField.text);
    }
    
    if (![self validatePcSelected]) {
        NSLog(@"No valid PC selected");
        return;
    }
    
    streamConfig = [[StreamConfiguration alloc] init];
    streamConfig.host = _selectedHost.hostName;
    streamConfig.hostAddr = [Utils resolveHost:_selectedHost.hostName];
    if (streamConfig.hostAddr == 0) {
        [self displayDnsFailedDialog];
        return;
    }
    
    unsigned long selectedConf = [self.StreamConfigs selectedRowInComponent:0];
    NSLog(@"selectedConf: %ld", selectedConf);
    switch (selectedConf) {
        case 0:
            streamConfig.width = 1280;
            streamConfig.height = 720;
            streamConfig.frameRate = 30;
            streamConfig.bitRate = 5000;
            break;
        default:
        case 1:
            streamConfig.width = 1280;
            streamConfig.height = 720;
            streamConfig.frameRate = 60;
            streamConfig.bitRate = 10000;
            break;
        case 2:
            streamConfig.width = 1920;
            streamConfig.height = 1080;
            streamConfig.frameRate = 30;
            streamConfig.bitRate = 10000;
            break;
        case 3:
            streamConfig.width = 1920;
            streamConfig.height = 1080;
            streamConfig.frameRate = 60;
            streamConfig.bitRate = 20000;
            break;
    }
    NSLog(@"StreamConfig: %@, %d, %dx%dx%d at %d Mbps", streamConfig.host, streamConfig.hostAddr, streamConfig.width, streamConfig.height, streamConfig.frameRate, streamConfig.bitRate);
    [self performSegueWithIdentifier:@"createStreamFrame" sender:self];
}
*/

/*
- (void)setSelectedHost:(NSInteger)selectedIndex
{
    _selectedHost = (Computer*)([self.hostPickerVals objectAtIndex:selectedIndex]);
    if (_selectedHost.hostName == NULL) {
        // This must be the placeholder computer
        _selectedHost = NULL;
    }
}
*/


- (void)viewDidLoad
{
    [super viewDidLoad];

    NSArray* streamConfigVals = [[NSArray alloc] initWithObjects:@"1280x720 (30Hz)", @"1280x720 (60Hz)", @"1920x1080 (30Hz)", @"1920x1080 (60Hz)",nil];
    
    _opQueue = [[NSOperationQueue alloc] init];
    
    // Initialize the host picker list
    //[self updateHosts:[[NSArray alloc] init]];
    
    Computer* test = [[Computer alloc] initWithIp:@"CEMENT-TRUCK"];
    
    
    UIScrollView* hostScrollView = [[UIScrollView alloc] init];
    hostScrollView.frame = CGRectMake(0, 0, self.view.frame.size.width, self.view.frame.size.height / 2);
    [hostScrollView setShowsHorizontalScrollIndicator:NO];
    UIComputerView* compView;
    for (int i = 0; i < 5; i++) {
        compView = [[UIComputerView alloc] initWithComputer:test];
        [hostScrollView addSubview:compView];
        [compView sizeToFit];
        compView.center = CGPointMake((compView.frame.size.width + 20) * i + compView.frame.size.width, hostScrollView.frame.size.height / 2);
         
    }
    
    [hostScrollView setContentSize:CGSizeMake(compView.frame.size.width * 5 + compView.frame.size.width, hostScrollView.frame.size.height)];
    

    UIScrollView* appScrollView = [[UIScrollView alloc] init];
    appScrollView.frame = CGRectMake(0, hostScrollView.frame.size.height, self.view.frame.size.width, self.view.frame.size.height / 2);
    [appScrollView setShowsHorizontalScrollIndicator:NO];
    
    
    App* testApp = [[App alloc] init];
    testApp.displayName = @"Left 4 Dead 2";
    UIAppView* appView;
    for (int i = 0; i < 5; i++) {
        appView = [[UIAppView alloc] initWithApp:testApp];
        [appScrollView addSubview:appView];
        [appView sizeToFit];
        appView.center = CGPointMake((appView.frame.size.width + 20) * i + compView.frame.size.width, appScrollView.frame.size.height / 2);
    }
    
    [appScrollView setContentSize:CGSizeMake(appView.frame.size.width * 5 + appView.frame.size.width, appScrollView.frame.size.height)];
   
    [self.view addSubview:hostScrollView];
    [self.view addSubview:appScrollView];
    
}

- (void)viewDidAppear:(BOOL)animated
{
	[super viewDidDisappear:animated];
    //_mDNSManager = [[MDNSManager alloc] initWithCallback:self];
   // [_mDNSManager searchForHosts];
}

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
 //   [_mDNSManager stopSearching];
}

- (void)updateHosts:(NSArray *)hosts {
    NSMutableArray *hostPickerValues = [[NSMutableArray alloc] initWithArray:hosts];
    
    if ([hostPickerValues count] == 0) {
        [hostPickerValues addObject:[[Computer alloc] initPlaceholder]];
    }
}

- (BOOL)validatePcSelected {
    if (_selectedHost == NULL) {
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"No PC Selected"
                                                                       message:@"You must select a PC to continue."
                                                                preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:nil]];
        [self presentViewController:alert animated:YES completion:nil];
        return FALSE;
    }
    
    return TRUE;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    [self.view endEditing:YES];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    [textField resignFirstResponder];
    return YES;
}

- (BOOL)shouldAutorotate {
    return YES;
}
@end
