//
//  UIComputerView.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "Host.h"

@protocol HostCallback <NSObject>

- (void) hostClicked:(Host*)host;
- (void) hostLongClicked:(Host*)host;
- (void) addHostClicked;

@end

@interface UIComputerView : UIView

- (id) initWithComputer:(Host*)host andCallback:(id<HostCallback>)callback;
- (id) initForAddWithCallback:(id<HostCallback>)callback;
- (NSString*) online;
@end
