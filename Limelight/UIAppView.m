//
//  UIAppView.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "UIAppView.h"

@implementation UIAppView {
    App* _app;
    UIButton* _appButton;
    UILabel* _appLabel;
    UIImageView* _appOverlay;
    id<AppCallback> _callback;
}

- (id) initWithApp:(App*)app andCallback:(id<AppCallback>)callback {
    self = [super init];
    _app = app;
    _callback = callback;
    
    _appButton = [UIButton buttonWithType:UIButtonTypeCustom];
    UIImage* noImage = [UIImage imageNamed:@"NoAppImage"];
    [_appButton setBackgroundImage:noImage forState:UIControlStateNormal];
	[_appButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_appButton sizeToFit];
    [_appButton addTarget:self action:@selector(appClicked) forControlEvents:UIControlEventTouchUpInside];
    
    _appOverlay = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"Play"]];
    _appOverlay.layer.shadowColor = [UIColor blackColor].CGColor;
    _appOverlay.layer.shadowOffset = CGSizeMake(0, 0);
    _appOverlay.layer.shadowOpacity = 1;
    _appOverlay.layer.shadowRadius = 2.0;
    [_appOverlay setHidden: YES];
    
    [self addSubview:_appButton];
    [self addSubview:_appOverlay];
    [self sizeToFit];
    
    _appButton.frame = CGRectMake(0, 0, noImage.size.width, noImage.size.height);
    _appOverlay.frame = CGRectMake(0, 0, noImage.size.width / 2.f, noImage.size.height / 4.f);
    self.frame = CGRectMake(0, 0, noImage.size.width, noImage.size.height);
    [_appOverlay setCenter:CGPointMake(self.frame.size.width/2, self.frame.size.height/6)];
    
    return self;
}

- (void) appClicked {
    [_callback appClicked:_app];
}

- (void) updateAppImage {
    [_appOverlay setHidden:!_app.isRunning];
    
    if (_app.appImage != nil && !(_app.appImage.size.width == 130.f && _app.appImage.size.height == 180.f)) {
        _appButton.frame = CGRectMake(0, 0, _app.appImage.size.width / 2, _app.appImage.size.height / 2);
        self.frame = CGRectMake(0, 0, _app.appImage.size.width / 2, _app.appImage.size.height / 2);
        _appOverlay.frame = CGRectMake(0, 0, self.frame.size.width / 2.f, self.frame.size.height / 4.f);
        _appOverlay.layer.shadowRadius = 4.0;
        [_appOverlay setCenter:CGPointMake(self.frame.size.width/2, self.frame.size.height/6)];
        [_appButton setBackgroundImage:_app.appImage forState:UIControlStateNormal];
        [self setNeedsDisplay];
    }

    // TODO: Improve no-app image detection
    if (_app.appImage == nil || (_app.appImage.size.width == 130.f && _app.appImage.size.height == 180.f)) { // This size of image might be blank image received from GameStream.
        _appLabel = [[UILabel alloc] init];
        CGFloat padding = 4.f;
        [_appLabel setFrame: CGRectMake(padding, padding, _appButton.frame.size.width - 2 * padding, _appButton.frame.size.height - 2 * padding)];
        [_appLabel setTextColor:[UIColor whiteColor]];
        [_appLabel setFont:[UIFont fontWithName:@"Roboto-Regular" size:10.f]];
        [_appLabel setBaselineAdjustment:UIBaselineAdjustmentAlignCenters];
        [_appLabel setTextAlignment:NSTextAlignmentCenter];
        [_appLabel setLineBreakMode:NSLineBreakByWordWrapping];
        [_appLabel setNumberOfLines:0];
        [_appLabel setText:_app.appName];
        [_appButton addSubview:_appLabel];
    }

}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect {
    // Drawing code
}
*/

@end
