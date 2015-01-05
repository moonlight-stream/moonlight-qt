//
//  UIAppView.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "UIAppView.h"

@implementation UIAppView {
    App* _app;
    UIButton* _appButton;
    UILabel* _appLabel;
    id<AppCallback> _callback;
}
static int LABEL_DY = 20;

- (id) initWithApp:(App*)app andCallback:(id<AppCallback>)callback {
    self = [super init];
    _app = app;
    _callback = callback;
    
    _appButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_appButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_appButton setBackgroundImage:[UIImage imageNamed:@"NoAppImage"] forState:UIControlStateNormal];
    [_appButton sizeToFit];
    [_appButton addTarget:self action:@selector(appClicked) forControlEvents:UIControlEventTouchUpInside];
    
    [self addSubview:_appButton];
    [self sizeToFit];
    
    return self;
}

- (void) updateBounds {
    float x = _appButton.frame.origin.x < _appLabel.frame.origin.x ? _appButton.frame.origin.x : _appLabel.frame.origin.x;
    float y = _appButton.frame.origin.y < _appLabel.frame.origin.y ? _appButton.frame.origin.y : _appLabel.frame.origin.y;
    self.bounds = CGRectMake(x , y, _appButton.frame.size.width > _appLabel.frame.size.width ? _appButton.frame.size.width : _appLabel.frame.size.width, _appButton.frame.size.height + _appLabel.frame.size.height + LABEL_DY / 2);
    self.frame = CGRectMake(x , y, _appButton.frame.size.width > _appLabel.frame.size.width ? _appButton.frame.size.width : _appLabel.frame.size.width, _appButton.frame.size.height + _appLabel.frame.size.height + LABEL_DY / 2);
}

- (void) appClicked {
    [_callback appClicked:_app];
}

- (void) updateAppImage {
    if (_app.appImage != nil) {
        _appButton.frame = CGRectMake(0, 0, _app.appImage.size.width / 2, _app.appImage.size.height / 2);
        self.frame = CGRectMake(0, 0, _app.appImage.size.width / 2, _app.appImage.size.height / 2);
        [_appButton setBackgroundImage:_app.appImage forState:UIControlStateNormal];
        [self setNeedsDisplay];
    }

    // TODO: Improve no-app image detection
    if (_app.appImage == nil || (_app.appImage.size.width == 130.f && _app.appImage.size.height == 180.f)) { // This size of image might be blank image received from GameStream.
        _appLabel = [[UILabel alloc] init];
        CGFloat padding = 4.f;
        [_appLabel setFrame: CGRectMake(padding, padding, _appButton.frame.size.width - 2 * padding, _appButton.frame.size.height - 2 * padding)];
        [_appLabel setTextColor:[UIColor whiteColor]];
        [_appLabel setFont:[UIFont systemFontOfSize:10.f]];
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
