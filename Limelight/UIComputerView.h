//
//  UIComputerView.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "Host.h"

@protocol HostCallback <NSObject>

- (void) hostClicked:(Host*)host;
- (void) hostLongClicked:(Host*)host view:(UIView*)view;
- (void) addHostClicked;

@end

@interface UIComputerView : UIView

- (id) initWithComputer:(Host*)host andCallback:(id<HostCallback>)callback;
- (id) initForAddWithCallback:(id<HostCallback>)callback;

@end
