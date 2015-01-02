//
//  UIComputerView.m
//  Limelight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "UIComputerView.h"

@implementation UIComputerView {
    Host* _host;
    UIButton* _hostButton;
    UILabel* _hostLabel;
    UILabel* _hostStatus;
    UILabel* _hostPairState;
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
    _hostStatus = [[UILabel alloc] init];
    _hostPairState = [[UILabel alloc] init];
    return self;
}

- (id) initWithComputer:(Host*)host andCallback:(id<HostCallback>)callback {
    self = [self init];
    _host = host;
    _callback = callback;

    [_hostLabel setText:_host.name];
    [_hostLabel sizeToFit];
    _hostLabel.textColor = [UIColor whiteColor];
    
    
    switch (host.pairState) {
        case PairStateUnknown:
            [_hostPairState setText:@"Pair State Unknown"];
            break;
        case PairStateUnpaired:
            [_hostPairState setText:@"Not Paired"];
            break;
        case PairStatePaired:
            [_hostPairState setText:@"Paired"];
            break;
    }
    [_hostPairState sizeToFit];
    _hostPairState.textColor = [UIColor whiteColor];
    
    if (host.online) {
        _hostStatus.text = @"Online";
        _hostStatus.textColor = [UIColor greenColor];
    } else {
        _hostStatus.text = @"Offline";
        _hostStatus.textColor = [UIColor grayColor];
    }
    [_hostStatus sizeToFit];
    
    [_hostButton addTarget:self action:@selector(hostClicked) forControlEvents:UIControlEventTouchUpInside];
    _hostLabel.center = CGPointMake(_hostButton.frame.origin.x + (_hostButton.frame.size.width / 2), _hostButton.frame.origin.y + _hostButton.frame.size.height + LABEL_DY);
    _hostPairState.center = CGPointMake(_hostLabel.center.x, _hostLabel.center.y + LABEL_DY);
    _hostStatus.center = CGPointMake(_hostPairState.center.x, _hostPairState.center.y + LABEL_DY);
    
    UILongPressGestureRecognizer* longPressRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(hostLongClicked)];
    [_hostButton addGestureRecognizer:longPressRecognizer];
    
    [self updateBounds];
    [self addSubview:_hostButton];
    [self addSubview:_hostLabel];
    [self addSubview:_hostStatus];
    [self addSubview:_hostPairState];
    return self;
}

- (NSString *)online {
    return _hostStatus.text;
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

- (void) hostLongClicked {
    [_callback hostLongClicked:_host];
}

- (void) hostClicked {
    [_callback hostClicked:_host];
}

- (void) addClicked {
    [_callback addHostClicked];
}

@end
