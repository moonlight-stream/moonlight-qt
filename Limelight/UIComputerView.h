//
//  UIComputerView.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "Computer.h"

@protocol HostCallback <NSObject>

- (void) hostClicked:(Computer*)computer;
- (void) addHostClicked;

@end

@interface UIComputerView : UIView

- (id) initWithComputer:(Computer*)computer andCallback:(id<HostCallback>)callback;
- (id) initForAddWithCallback:(id<HostCallback>)callback;

@end
