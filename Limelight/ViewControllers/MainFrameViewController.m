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
    NSString* _uniqueId;
    NSData* _cert;
    
    UIAlertView* _pairAlert;
    UIScrollView* hostScrollView;
    UIScrollView* appScrollView;
}
static NSString* deviceName = @"roth";
static NSMutableSet* hostList;
static StreamConfiguration* streamConfig;

+ (StreamConfiguration*) getStreamConfiguration {
    return streamConfig;
}

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

- (void)alreadyPaired {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        HttpManager* hMan = [[HttpManager alloc] initWithHost:_selectedHost.hostName uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* appListResp = [hMan executeRequestSynchronously:[hMan newAppListRequest]];
        NSArray* appList = [HttpManager getAppListFromXML:appListResp];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateApps:appList];
        });
        [AppManager retrieveAppAssets:appList withManager:hMan andCallback:self];
    });
}

- (void) receivedAssetForApp:(App*)app {
    NSArray* subviews = [appScrollView subviews];
    for (UIAppView* appView in subviews) {
        [appView updateAppImage];
    }
}

- (void)displayDnsFailedDialog {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Network Error"
                                                                   message:@"Failed to resolve host."
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void) hostClicked:(Computer *)computer {
    NSLog(@"Clicked host: %@", computer.displayName);
    _selectedHost = computer;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        HttpManager* hMan = [[HttpManager alloc] initWithHost:computer.hostName uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* serverInfoResp = [hMan executeRequestSynchronously:[hMan newServerInfoRequest]];
        if ([[HttpManager getStringFromXML:serverInfoResp tag:@"PairStatus"] isEqualToString:@"1"]) {
            NSLog(@"Already Paired");
            [self alreadyPaired];
        } else {
            NSLog(@"Trying to pair");
            PairManager* pMan = [[PairManager alloc] initWithManager:hMan andCert:_cert callback:self];
            [_opQueue addOperation:pMan];
        }
    });
}

- (void) addHostClicked {
    NSLog(@"Clicked add host");
    UIAlertController* alertController = [UIAlertController alertControllerWithTitle:@"Host Address" message:@"Please enter a hostname or IP address" preferredStyle:UIAlertControllerStyleAlert];
    [alertController addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [alertController addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
        NSString* host = ((UITextField*)[[alertController textFields] objectAtIndex:0]).text;
        Computer* newHost = [[Computer alloc] initWithIp:host];
        [hostList addObject:newHost];
        [self updateHosts:[hostList allObjects]];
        
        
        //TODO: get pair state
        
        
    }]];
    [alertController addTextFieldWithConfigurationHandler:nil];
    [self presentViewController:alertController animated:YES completion:nil];
}

- (void) appClicked:(App *)app {
    NSLog(@"Clicked app: %@", app.appName);
    streamConfig = [[StreamConfiguration alloc] init];
    streamConfig.host = _selectedHost.hostName;
    streamConfig.hostAddr = [Utils resolveHost:_selectedHost.hostName];
    streamConfig.appID = app.appId;
    if (streamConfig.hostAddr == 0) {
        [self displayDnsFailedDialog];
        return;
    }
    
    // TODO: actually allow the user to choose the config
    unsigned long selectedConf = 1;
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

- (void)viewDidLoad
{
    [super viewDidLoad];

    NSArray* streamConfigVals = [[NSArray alloc] initWithObjects:@"1280x720 (30Hz)", @"1280x720 (60Hz)", @"1920x1080 (30Hz)", @"1920x1080 (60Hz)",nil];
    
    _opQueue = [[NSOperationQueue alloc] init];
    [CryptoManager generateKeyPairUsingSSl];
    _uniqueId = [CryptoManager getUniqueID];
    _cert = [CryptoManager readCertFromFile];
    
    // Initialize the host picker list
    if (hostList == nil) {
        hostList = [[NSMutableSet alloc] init];
    }
    
    hostScrollView = [[UIScrollView alloc] init];
    hostScrollView.frame = CGRectMake(0, 0, self.view.frame.size.width, self.view.frame.size.height / 2);
    [hostScrollView setShowsHorizontalScrollIndicator:NO];

    appScrollView = [[UIScrollView alloc] init];
    appScrollView.frame = CGRectMake(0, hostScrollView.frame.size.height, self.view.frame.size.width, self.view.frame.size.height / 2);
    [appScrollView setShowsHorizontalScrollIndicator:NO];

    [self updateHosts:[hostList allObjects]];
    [self.view addSubview:hostScrollView];
    [self.view addSubview:appScrollView];
}

- (void)viewDidAppear:(BOOL)animated
{
	[super viewDidDisappear:animated];
    _mDNSManager = [[MDNSManager alloc] initWithCallback:self];
    [_mDNSManager searchForHosts];
}

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
    [_mDNSManager stopSearching];
}

- (void)updateHosts:(NSArray *)hosts {
    [hostList addObjectsFromArray:hosts];
    [[hostScrollView subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    UIComputerView* addComp = [[UIComputerView alloc] initForAddWithCallback:self];
    UIComputerView* compView;
    float prevEdge = -1;
    for (Computer* comp in hostList) {
        compView = [[UIComputerView alloc] initWithComputer:comp andCallback:self];
        compView.center = CGPointMake([self getCompViewX:compView addComp:addComp prevEdge:prevEdge], hostScrollView.frame.size.height / 2);
        prevEdge = compView.frame.origin.x + compView.frame.size.width;
        [hostScrollView addSubview:compView];
    }
    
    prevEdge = [self getCompViewX:addComp addComp:addComp prevEdge:prevEdge];
    addComp.center = CGPointMake(prevEdge, hostScrollView.frame.size.height / 2);
    
    [hostScrollView addSubview:addComp];
    [hostScrollView setContentSize:CGSizeMake(prevEdge + addComp.frame.size.width, hostScrollView.frame.size.height)];
}

- (float) getCompViewX:(UIComputerView*)comp addComp:(UIComputerView*)addComp prevEdge:(float)prevEdge {
    if (prevEdge == -1) {
        return hostScrollView.frame.origin.x + comp.frame.size.width / 2 + addComp.frame.size.width / 2;
    } else {
        return prevEdge + addComp.frame.size.width / 2  + comp.frame.size.width / 2;
    }
}

- (void) updateApps:(NSArray*)apps {
    [[appScrollView subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    App* fakeApp = [[App alloc] init];
    fakeApp.appName = @"No App Name";
    UIAppView* noAppImage = [[UIAppView alloc] initWithApp:fakeApp andCallback:nil];
    float prevEdge = -1;
    UIAppView* appView;
    for (App* app in apps) {
        appView = [[UIAppView alloc] initWithApp:app andCallback:self];
        prevEdge = [self getAppViewX:appView noApp:noAppImage prevEdge:prevEdge];
        appView.center = CGPointMake(prevEdge, appScrollView.frame.size.height / 2);
        prevEdge = appView.frame.origin.x + appView.frame.size.width;
        [appScrollView addSubview:appView];
    }
    [appScrollView setContentSize:CGSizeMake(prevEdge + noAppImage.frame.size.width, appScrollView.frame.size.height)];
}

- (float) getAppViewX:(UIAppView*)app noApp:(UIAppView*)noAppImage prevEdge:(float)prevEdge {
    if (prevEdge == -1) {
        return appScrollView.frame.origin.x + app.frame.size.width / 2 + noAppImage.frame.size.width / 2;
    } else {
        return prevEdge + app.frame.size.width / 2 + noAppImage.frame.size.width / 2;
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
