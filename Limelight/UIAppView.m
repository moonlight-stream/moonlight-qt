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
    _appButton.layer.shadowColor = [[UIColor blackColor] CGColor];
    _appButton.layer.shadowOffset = CGSizeMake(5,8);
    _appButton.layer.shadowOpacity = 0.7;
    
    _appLabel = [[UILabel alloc] init];
    [_appLabel setText:_app.appName];
    [_appLabel sizeToFit];
    _appLabel.center = CGPointMake(_appButton.bounds.origin.x + (_appButton.bounds.size.width / 2), _appButton.bounds.origin.y + _appButton.bounds.size.height + LABEL_DY);
    
    [self updateBounds];
    [self addSubview:_appButton];
    [self addSubview:_appLabel];
    
    
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
        [_appButton setBackgroundImage:_app.appImage forState:UIControlStateNormal];
        [self setNeedsDisplay];
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
