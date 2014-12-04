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
#import "SettingsViewController.h"
#import "DataManager.h"
#import "Settings.h"

@implementation MainFrameViewController {
    NSOperationQueue* _opQueue;
    MDNSManager* _mDNSManager;
    Computer* _selectedHost;
    NSString* _uniqueId;
    NSData* _cert;
    
    UIAlertView* _pairAlert;
    UIScrollView* hostScrollView;
}
static NSString* deviceName = @"roth";
static NSMutableSet* hostList;
static NSArray* appList;
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
        appList = [HttpManager getAppListFromXML:appListResp];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateApps];
            _computerNameButton.title = _selectedHost.displayName;
        });
        [AppManager retrieveAppAssets:appList withManager:hMan andCallback:self];
    });
}

- (void)showHostSelectionView {
    appList = [[NSArray alloc] init];
    _computerNameButton.title = @"";
    [self.collectionView reloadData];
    [self.view addSubview:hostScrollView];
}

- (void) receivedAssetForApp:(App*)app {
    [self.collectionView reloadData];
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
        DataManager* dataMan = [[DataManager alloc] init];
        [dataMan createHost:newHost.displayName hostname:newHost.hostName];
        [dataMan saveHosts];
        
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
    
    DataManager* dataMan = [[DataManager alloc] init];
    Settings* streamSettings = [dataMan retrieveSettings];
    
    streamConfig.frameRate = [streamSettings.framerate intValue];
    streamConfig.bitRate = [streamSettings.bitrate intValue];
    streamConfig.height = [streamSettings.height intValue];
    streamConfig.width = [streamSettings.width intValue];
    
    [self performSegueWithIdentifier:@"createStreamFrame" sender:nil];
}

- (void)revealController:(SWRevealViewController *)revealController didMoveToPosition:(FrontViewPosition)position {
    // If we moved back to the center position, we should save the settings
    if (position == FrontViewPositionLeft) {
        [(SettingsViewController*)[revealController rearViewController] saveSettings];
    }
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // Set the side bar button action. When it's tapped, it'll show up the sidebar.
    [_limelightLogoButton addTarget:self.revealViewController action:@selector(revealToggle:) forControlEvents:UIControlEventTouchDown];
    
    // Set the host name button action. When it's tapped, it'll show up the host selection view.
    [_computerNameButton setTarget:self];
    [_computerNameButton setAction:@selector(showHostSelectionView)];
    
    // Set the gesture
    [self.view addGestureRecognizer:self.revealViewController.panGestureRecognizer];
    
    [self.revealViewController setDelegate:self];
    
    //NSArray* streamConfigVals = [[NSArray alloc] initWithObjects:@"1280x720 (30Hz)", @"1280x720 (60Hz)", @"1920x1080 (30Hz)", @"1920x1080 (60Hz)",nil];
    
    _opQueue = [[NSOperationQueue alloc] init];
    [CryptoManager generateKeyPairUsingSSl];
    _uniqueId = [CryptoManager getUniqueID];
    _cert = [CryptoManager readCertFromFile];
    
    // Only initialize the host picker list once
    if (hostList == nil) {
        hostList = [[NSMutableSet alloc] init];
    }
    
    [self setAutomaticallyAdjustsScrollViewInsets:NO];
    
    hostScrollView = [[UIScrollView alloc] init];
    hostScrollView.frame = CGRectMake(0, self.navigationController.navigationBar.frame.origin.y + self.navigationController.navigationBar.frame.size.height, self.view.frame.size.width, self.view.frame.size.height / 2);
    [hostScrollView setShowsHorizontalScrollIndicator:NO];
    
    [self retrieveSavedHosts];
    [self updateHosts:[hostList allObjects]];
    [self.view addSubview:hostScrollView];
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    [self.navigationController setNavigationBarHidden:NO animated:YES];
    
    // Hide 1px border line
    UIImage* fakeImage = [[UIImage alloc] init];
    [self.navigationController.navigationBar setShadowImage:fakeImage];
    [self.navigationController.navigationBar setBackgroundImage:fakeImage forBarPosition:UIBarPositionAny barMetrics:UIBarMetricsDefault];
    
    _mDNSManager = [[MDNSManager alloc] initWithCallback:self];
    [_mDNSManager searchForHosts];
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
    [_mDNSManager stopSearching];
}

- (void) retrieveSavedHosts {
    //TODO: Get rid of Computer and only use Host
    
    DataManager* dataMan = [[DataManager alloc] init];
    NSArray* hosts = [dataMan retrieveHosts];
    for (Host* host in hosts) {
        Computer* comp = [[Computer alloc] initWithIp:host.address];
        comp.displayName = host.name;
        [hostList addObject:comp];
    }
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

- (void) updateApps {
    [hostScrollView removeFromSuperview];
    [self.collectionView reloadData];
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    UICollectionViewCell* cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"AppCell" forIndexPath:indexPath];
    
    App* app = appList[indexPath.row];
    UIAppView* appView = [[UIAppView alloc] initWithApp:app andCallback:self];
    [appView updateAppImage];

    if (appView.bounds.size.width > 10.0) {
        CGFloat scale = cell.bounds.size.width / appView.bounds.size.width;
        [appView setCenter:CGPointMake(appView.bounds.size.width / 2 * scale, appView.bounds.size.height / 2 * scale)];
        appView.transform = CGAffineTransformMakeScale(scale, scale);
    }

    [cell.subviews.firstObject removeFromSuperview]; // Remove a view that was previously added
    [cell addSubview:appView];

    return cell;
}

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView {
    return 1; // App collection only
}

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section {
    return appList.count;
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
