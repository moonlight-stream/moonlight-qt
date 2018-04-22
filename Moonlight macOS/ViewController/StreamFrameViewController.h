//
//  StreamFrameViewController.h
//  Moonlight macOS
//
//  Created by Felix Kratz on 09.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Connection.h"
#import "StreamConfiguration.h"
#import "StreamView.h"
#import "ViewController.h"

@interface StreamFrameViewController : NSViewController <ConnectionCallbacks>

- (ViewController*) _origin;

- (void)setOrigin: (ViewController*) viewController;

@property (nonatomic) StreamConfiguration* streamConfig;
@property (strong) IBOutlet StreamView *streamView;
@property (weak) IBOutlet NSProgressIndicator *progressIndicator;

@end
