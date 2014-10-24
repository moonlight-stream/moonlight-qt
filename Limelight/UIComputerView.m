//
//  UIComputerView.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "UIComputerView.h"

@implementation UIComputerView {
    Computer* _computer;
    UIButton* _hostButton;
    UILabel* _hostLabel;
}
static int LABEL_DY = 20;

- (id) initWithComputer:(Computer*)computer {
    self = [super init];
    _computer = computer;
    
    _hostButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_hostButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_hostButton setBackgroundImage:[[UIImage imageNamed:@"Computer"] resizableImageWithCapInsets:UIEdgeInsetsMake(10, 10, 10, 10)] forState:UIControlStateNormal];
    [_hostButton sizeToFit];
    
    _hostLabel = [[UILabel alloc] init];
    [_hostLabel setText:[_computer displayName]];
    [_hostLabel sizeToFit];
    _hostLabel.center = CGPointMake(_hostButton.bounds.origin.x + (_hostButton.bounds.size.width / 2), _hostButton.bounds.origin.y + _hostButton.bounds.size.height + LABEL_DY);
    
    [self addSubview:_hostButton];
    [self addSubview:_hostLabel];
    
    self.frame = CGRectMake(0, 0, _hostButton.frame.size.width > _hostLabel.frame.size.width ? _hostButton.frame.size.width : _hostLabel.frame.size.width, _hostButton.frame.size.height + _hostLabel.frame.size.height);
    
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
