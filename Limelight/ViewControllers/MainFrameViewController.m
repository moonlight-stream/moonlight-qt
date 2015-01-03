//  MainFrameViewController.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/17/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "MainFrameViewController.h"
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
#import "WakeOnLanManager.h"

@implementation MainFrameViewController {
    NSOperationQueue* _opQueue;
    Host* _selectedHost;
    NSString* _uniqueId;
    NSData* _cert;
    NSString* _currentGame;
    DiscoveryManager* _discMan;
    UIAlertView* _pairAlert;
    UIScrollView* hostScrollView;
    int currentPosition;
}
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
        [_discMan startDiscovery];
    });
}

- (void)pairSuccessful {
    dispatch_sync(dispatch_get_main_queue(), ^{
        [_pairAlert dismissWithClickedButtonIndex:0 animated:NO];
        _pairAlert = [[UIAlertView alloc] initWithTitle:@"Pairing Succesful" message:@"Successfully paired to host" delegate:self cancelButtonTitle:@"Ok" otherButtonTitles:nil, nil];
        [_pairAlert show];
        [_discMan startDiscovery];
    });
}

- (void)alreadyPaired {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateApps];
            NSLog(@"Setting _computerNameButton.title: %@", _selectedHost.name);
            _computerNameButton.title = _selectedHost.name;
        });
        HttpManager* hMan = [[HttpManager alloc] initWithHost:_selectedHost.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* appListResp = [hMan executeRequestSynchronously:[hMan newAppListRequest]];
        appList = [HttpManager getAppListFromXML:appListResp];
        
        [AppManager retrieveAppAssets:appList withManager:hMan andCallback:self];
    });
}

- (void)showHostSelectionView {
    appList = [[NSArray alloc] init];
    _computerNameButton.title = @"No Host Selected";
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

- (void) hostClicked:(Host *)host {
    NSLog(@"Clicked host: %@", host.name);
    _selectedHost = host;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        HttpManager* hMan = [[HttpManager alloc] initWithHost:host.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
        NSData* serverInfoResp = [hMan executeRequestSynchronously:[hMan newServerInfoRequest]];
        if ([[HttpManager getStringFromXML:serverInfoResp tag:@"PairStatus"] isEqualToString:@"1"]) {
            NSLog(@"Already Paired");
            [self alreadyPaired];
        } else {
            NSLog(@"Trying to pair");
            // Polling the server while pairing causes the server to screw up
            [_discMan stopDiscoveryBlocking];
            PairManager* pMan = [[PairManager alloc] initWithManager:hMan andCert:_cert callback:self];
            [_opQueue addOperation:pMan];
        }
    });
}

- (void)hostLongClicked:(Host *)host {
    NSLog(@"Long clicked host: %@", host.name);
    UIAlertController* longClickAlert = [UIAlertController alertControllerWithTitle:host.name message:@"" preferredStyle:UIAlertControllerStyleActionSheet];
    if (host.online) {
        [longClickAlert addAction:[UIAlertAction actionWithTitle:@"Unpair" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                HttpManager* hMan = [[HttpManager alloc] initWithHost:host.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
                [hMan executeRequestSynchronously:[hMan newUnpairRequest]];
            });
        }]];
    } else {
        [longClickAlert addAction:[UIAlertAction actionWithTitle:@"Wake" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
            UIAlertController* wolAlert = [UIAlertController alertControllerWithTitle:@"Wake On Lan" message:@"" preferredStyle:UIAlertControllerStyleAlert];
            [wolAlert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
            if (host.pairState != PairStatePaired) {
                wolAlert.message = @"Cannot wake host because you are not paired";
            } else if (host.mac == nil || [host.mac isEqualToString:@"00:00:00:00:00:00"]) {
                wolAlert.message = @"Host MAC unknown, unable to send WOL Packet";
            } else {
                dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                    [WakeOnLanManager wakeHost:host];
                });
                wolAlert.message = @"Sent WOL Packet";
            }
            [self presentViewController:wolAlert animated:YES completion:nil];
        }]];
    }
    [longClickAlert addAction:[UIAlertAction actionWithTitle:@"Remove Host" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action) {
        @synchronized(hostList) {
            [hostList removeObject:host];
        }
        [_discMan removeHostFromDiscovery:host];
        DataManager* dataMan = [[DataManager alloc] init];
        [dataMan removeHost:host];
    }]];
    [longClickAlert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [self presentViewController:longClickAlert animated:YES completion:^{
        [self updateHosts];
    }];
}

- (void) addHostClicked {
    NSLog(@"Clicked add host");
    UIAlertController* alertController = [UIAlertController alertControllerWithTitle:@"Host Address" message:@"Please enter a hostname or IP address" preferredStyle:UIAlertControllerStyleAlert];
    [alertController addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [alertController addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
        NSString* hostAddress = ((UITextField*)[[alertController textFields] objectAtIndex:0]).text;
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            [_discMan discoverHost:hostAddress withCallback:^(Host* host){
                if (host != nil) {
                    DataManager* dataMan = [[DataManager alloc] init];
                    [dataMan saveHosts];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        @synchronized(hostList) {
                            [hostList addObject:host];
                        }
                        [self updateHosts];
                    });
                } else {
                    UIAlertController* hostNotFoundAlert = [UIAlertController alertControllerWithTitle:@"Host Not Found" message:[NSString stringWithFormat:@"Unable to connect to host: \n%@", hostAddress] preferredStyle:UIAlertControllerStyleAlert];
                    [hostNotFoundAlert addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleDestructive handler:nil]];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self presentViewController:hostNotFoundAlert animated:YES completion:nil];
                    });
                }
            }];});
    }]];
    [alertController addTextFieldWithConfigurationHandler:nil];
    [self presentViewController:alertController animated:YES completion:nil];
}

- (void) appClicked:(App *)app {
    NSLog(@"Clicked app: %@", app.appName);
    streamConfig = [[StreamConfiguration alloc] init];
    streamConfig.host = _selectedHost.address;
    streamConfig.hostAddr = [Utils resolveHost:_selectedHost.address];
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
    
    
    if (currentPosition != FrontViewPositionLeft) {
        [[self revealViewController] revealToggle:self];
    }
    
    App* currentApp = [self findRunningApp];
    if (currentApp != nil) {
        UIAlertController* alertController = [UIAlertController alertControllerWithTitle:@"App Already Running" message:[NSString stringWithFormat:@"%@ is currently running", currentApp.appName] preferredStyle:UIAlertControllerStyleAlert];
        [alertController addAction:[UIAlertAction actionWithTitle:@"Resume App" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
            NSLog(@"Resuming application: %@", currentApp.appName);
            [self performSegueWithIdentifier:@"createStreamFrame" sender:nil];
        }]];
        [alertController addAction:[UIAlertAction actionWithTitle:@"Quit App" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){
            NSLog(@"Quitting application: %@", currentApp.appName);
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                HttpManager* hMan = [[HttpManager alloc] initWithHost:_selectedHost.address uniqueId:_uniqueId deviceName:deviceName cert:_cert];
                [hMan executeRequestSynchronously:[hMan newQuitAppRequest]];
                // TODO: handle failure to quit app
                currentApp.isRunning = NO;
                
                if (![app.appId isEqualToString:currentApp.appId]) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self performSegueWithIdentifier:@"createStreamFrame" sender:nil];
                    });
                }
            });
        }]];
        [alertController addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:alertController animated:YES completion:nil];
    } else {
        [self performSegueWithIdentifier:@"createStreamFrame" sender:nil];
    }
}


- (App*) findRunningApp {
    for (App* app in appList) {
        if (app.isRunning) {
            return app;
        }
    }
    return nil;
}

- (void)revealController:(SWRevealViewController *)revealController didMoveToPosition:(FrontViewPosition)position {
    // If we moved back to the center position, we should save the settings
    if (position == FrontViewPositionLeft) {
        [(SettingsViewController*)[revealController rearViewController] saveSettings];
    }
    currentPosition = position;
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
    
    // Get callbacks associated with the viewController
    [self.revealViewController setDelegate:self];
    
    // Set the current position to the center
    currentPosition = FrontViewPositionLeft;
    
    // Set up crypto
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
    _discMan = [[DiscoveryManager alloc] initWithHosts:[hostList allObjects] andCallback:self];
    
    [self updateHosts];
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
    
    [_discMan startDiscovery];
    
    // This will refresh the applist
    if (_selectedHost != nil) {
        [self hostClicked:_selectedHost];
    }
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
    // when discovery stops, we must create a new instance because you cannot restart an NSOperation when it is finished
    [_discMan stopDiscovery];

    // In case the host objects were updated in the background
    [[[DataManager alloc] init] saveHosts];
}

- (void) retrieveSavedHosts {
    DataManager* dataMan = [[DataManager alloc] init];
    NSArray* hosts = [dataMan retrieveHosts];
    @synchronized(hostList) {
        [hostList addObjectsFromArray:hosts];
    }
}

- (void) updateAllHosts:(NSArray *)hosts {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"New host list:");
        for (Host* host in hosts) {
            NSLog(@"Host: \n{\n\t name:%@ \n\t address:%@ \n\t localAddress:%@ \n\t externalAddress:%@ \n\t uuid:%@ \n\t mac:%@ \n\t pairState:%d \n\t online:%d \n}", host.name, host.address, host.localAddress, host.externalAddress, host.uuid, host.mac, host.pairState, host.online);
        }
        @synchronized(hostList) {
            [hostList removeAllObjects];
            [hostList addObjectsFromArray:hosts];
        }
        [self updateHosts];
    });
}

- (void)updateHosts {
    NSLog(@"Updating hosts...");
    [[hostScrollView subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    UIComputerView* addComp = [[UIComputerView alloc] initForAddWithCallback:self];
    UIComputerView* compView;
    float prevEdge = -1;
    @synchronized (hostList) {
        for (Host* comp in hostList) {
            compView = [[UIComputerView alloc] initWithComputer:comp andCallback:self];
            compView.center = CGPointMake([self getCompViewX:compView addComp:addComp prevEdge:prevEdge], hostScrollView.frame.size.height / 2);
            prevEdge = compView.frame.origin.x + compView.frame.size.width;
            [hostScrollView addSubview:compView];
        }
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
