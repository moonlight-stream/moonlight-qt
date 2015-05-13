//
//  UIAppView.h
//  Moonlight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "App.h"

@protocol AppCallback <NSObject>

- (void) appClicked:(App*) app;

@end

@interface UIAppView : UIView

- (id) initWithApp:(App*)app andCallback:(id<AppCallback>)callback;
- (void) updateAppImage;

@end
