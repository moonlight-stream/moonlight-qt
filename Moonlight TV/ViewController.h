//
//  ViewController.h
//  Moonlight TV
//
//  Created by Diego Waxemberg on 8/25/18.
//  Copyright © 2018 Moonlight Game Streaming Project. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "DiscoveryManager.h"
#import "PairManager.h"
#import "StreamConfiguration.h"
#import "UIComputerView.h"
#import "UIAppView.h"
#import "AppAssetManager.h"

@interface ViewController : UICollectionViewController <DiscoveryCallback, PairCallback, HostCallback, AppCallback, AppAssetCallback, NSURLConnectionDelegate>


@end

