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
}
static int LABEL_DY = 20;

- (id) initWithApp:(App*)app {
    self = [super init];
    _app = app;
    
    _appButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_appButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_appButton setBackgroundImage:[[UIImage imageNamed:@"Left4Dead2"] resizableImageWithCapInsets:UIEdgeInsetsMake(10, 10, 10, 10)] forState:UIControlStateNormal];
    [_appButton sizeToFit];
    
    _appLabel = [[UILabel alloc] init];
    [_appLabel setText:_app.displayName];
    [_appLabel sizeToFit];
    _appLabel.center = CGPointMake(_appButton.bounds.origin.x + (_appButton.bounds.size.width / 2), _appButton.bounds.origin.y + _appButton.bounds.size.height + LABEL_DY);
    
    [self addSubview:_appButton];
    [self addSubview:_appLabel];
    
    self.frame = CGRectMake(0, 0, _appButton.frame.size.width > _appLabel.frame.size.width ? _appButton.frame.size.width : _appLabel.frame.size.width, _appButton.frame.size.height + _appLabel.frame.size.height);
    
    return self;
}



/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect {
    // Drawing code
}
*/

@end
