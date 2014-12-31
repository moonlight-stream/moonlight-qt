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
    id<HostCallback> _callback;
    CGSize _labelSize;
}
static int LABEL_DY = 20;

- (id) init {
    self = [super init];
    _hostButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_hostButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_hostButton setBackgroundImage:[UIImage imageNamed:@"Computer"] forState:UIControlStateNormal];
    [_hostButton sizeToFit];
    
     _hostButton.layer.shadowColor = [[UIColor blackColor] CGColor];
    _hostButton.layer.shadowOffset = CGSizeMake(5,8);
    _hostButton.layer.shadowOpacity = 0.7;
    
    _hostLabel = [[UILabel alloc] init];
    
    return self;
}

- (id) initWithComputer:(Computer*)computer andCallback:(id<HostCallback>)callback {
    self = [self init];
    _computer = computer;
    _callback = callback;

    [_hostLabel setText:[_computer displayName]];
    [_hostLabel sizeToFit];
    _hostLabel.textColor = [UIColor whiteColor];
    [_hostButton addTarget:self action:@selector(hostClicked) forControlEvents:UIControlEventTouchUpInside];
    _hostLabel.center = CGPointMake(_hostButton.frame.origin.x + (_hostButton.frame.size.width / 2), _hostButton.frame.origin.y + _hostButton.frame.size.height + LABEL_DY);
    [self updateBounds];
    [self addSubview:_hostButton];
    [self addSubview:_hostLabel];
    
    return self;
}

- (void) updateBounds {
    float x = _hostButton.frame.origin.x < _hostLabel.frame.origin.x ? _hostButton.frame.origin.x : _hostLabel.frame.origin.x;
    float y = _hostButton.frame.origin.y < _hostLabel.frame.origin.y ? _hostButton.frame.origin.y : _hostLabel.frame.origin.y;
    self.bounds = CGRectMake(x , y, _hostButton.frame.size.width > _hostLabel.frame.size.width ? _hostButton.frame.size.width : _hostLabel.frame.size.width, _hostButton.frame.size.height + _hostLabel.frame.size.height + LABEL_DY / 2);
    self.frame = CGRectMake(x , y, _hostButton.frame.size.width > _hostLabel.frame.size.width ? _hostButton.frame.size.width : _hostLabel.frame.size.width, _hostButton.frame.size.height + _hostLabel.frame.size.height + LABEL_DY / 2);
}

- (id) initForAddWithCallback:(id<HostCallback>)callback {
    self = [self init];
    _callback = callback;
    
    [_hostButton setBackgroundImage:[UIImage imageNamed:@"Computer"] forState:UIControlStateNormal];
    [_hostButton sizeToFit];
    [_hostButton addTarget:self action:@selector(addClicked) forControlEvents:UIControlEventTouchUpInside];
    
    [_hostLabel setText:@"Add Host"];
    [_hostLabel sizeToFit];
    _hostLabel.textColor = [UIColor whiteColor];
    _hostLabel.center = CGPointMake(_hostButton.frame.origin.x + (_hostButton.frame.size.width / 2), _hostButton.frame.origin.y + _hostButton.frame.size.height + LABEL_DY);
    
    UIImageView* addIcon = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"AddComputerIcon"]];
    [addIcon sizeToFit];
    addIcon.center = CGPointMake(_hostButton.frame.origin.x + _hostButton.frame.size.width, _hostButton.frame.origin.y);
    
    [self updateBounds];
    [self addSubview:_hostButton];
    [self addSubview:_hostLabel];
    [self addSubview:addIcon];
    
    return self;
}

- (void) hostClicked {
    [_callback hostClicked:_computer];
}

- (void) addClicked {
    [_callback addHostClicked];
}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect {
    // Drawing code
}
*/

@end
