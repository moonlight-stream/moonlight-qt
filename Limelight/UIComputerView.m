//
//  UIComputerView.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
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
static const float REFRESH_CYCLE = 2.0f;
static const int LABEL_DY = 20;

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
    [_hostLabel setFont:[UIFont fontWithName:@"Roboto-Regular" size:[UIFont systemFontSize]]];
    [_hostStatus setFont:[UIFont fontWithName:@"Roboto-Regular" size:[UIFont systemFontSize]]];
	[_hostPairState setFont:[UIFont fontWithName:@"Roboto-Regular" size:[UIFont systemFontSize]]];
    return self;
}

- (id) initForAddWithCallback:(id<HostCallback>)callback {
    self = [self init];
    _callback = callback;
    
    [_hostButton setBackgroundImage:[UIImage imageNamed:@"Computer"] forState:UIControlStateNormal];
    [_hostButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_hostButton addTarget:self action:@selector(addClicked) forControlEvents:UIControlEventTouchUpInside];
    [_hostButton sizeToFit];
    
    [_hostLabel setText:@"Add Host"];
    [_hostLabel sizeToFit];
    _hostLabel.textColor = [UIColor whiteColor];
    _hostLabel.center = CGPointMake(_hostButton.frame.origin.x + (_hostButton.frame.size.width / 2), _hostButton.frame.origin.y + _hostButton.frame.size.height + LABEL_DY);
    
    UIImageView* addIcon = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"AddComputerIcon"]];
    [addIcon sizeToFit];
    addIcon.center = CGPointMake(_hostButton.frame.origin.x + _hostButton.frame.size.width, _hostButton.frame.origin.y);
    
    // This is required to ensure this button is the same size as the others
    _hostPairState.text = @"None";
    _hostStatus.text = @"None";
    [_hostPairState sizeToFit];
    [_hostStatus sizeToFit];
    
    [self updateBounds];
    [self addSubview:_hostButton];
    [self addSubview:_hostLabel];
    [self addSubview:addIcon];
    
    return self;
}

- (id) initWithComputer:(Host*)host andCallback:(id<HostCallback>)callback {
    self = [self init];
    _host = host;
    _callback = callback;
    
    UILongPressGestureRecognizer* longPressRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(hostLongClicked)];
    [_hostButton addGestureRecognizer:longPressRecognizer];
    [_hostButton addTarget:self action:@selector(hostClicked) forControlEvents:UIControlEventTouchUpInside];
    
    [self updateContentsForHost:host];
    [self updateBounds];
    [self addSubview:_hostButton];
    [self addSubview:_hostLabel];
    [self addSubview:_hostStatus];
    [self addSubview:_hostPairState];
    [self startUpdateLoop];

    return self;
}

- (void) updateBounds {
    float x = FLT_MAX;
    float y = FLT_MAX;
    float width = 0;
    float height;
    
    x = MIN(x, _hostButton.frame.origin.x);
    x = MIN(x, _hostLabel.frame.origin.x);
    x = MIN(x, _hostStatus.frame.origin.x);
    x = MIN(x, _hostPairState.frame.origin.x);
    
    y = MIN(y, _hostButton.frame.origin.y);
    y = MIN(y, _hostLabel.frame.origin.y);
    y = MIN(y, _hostStatus.frame.origin.y);
    y = MIN(y, _hostPairState.frame.origin.y);

    width = MAX(width, _hostButton.frame.size.width);
    width = MAX(width, _hostLabel.frame.size.width);
    width = MAX(width, _hostStatus.frame.size.width);
    width = MAX(width, _hostPairState.frame.size.width);
    
    height = _hostButton.frame.size.height +
        _hostLabel.frame.size.height +
        _hostStatus.frame.size.height +
        _hostPairState.frame.size.height +
        LABEL_DY / 2;
    
    self.bounds = CGRectMake(x, y, width, height);
    self.frame = CGRectMake(x, y, width, height);
}

- (void) updateContentsForHost:(Host*)host {
    _hostLabel.text = _host.name;
    _hostLabel.textColor = [UIColor whiteColor];
    [_hostLabel sizeToFit];
    
    switch (host.pairState) {
        case PairStateUnknown:
            _hostPairState.text = @"Pair State Unknown";
            break;
        case PairStateUnpaired:
            _hostPairState.text = @"Not Paired";
            break;
        case PairStatePaired:
            _hostPairState.text = @"Paired";
            break;
    }
    _hostPairState.textColor = [UIColor whiteColor];
    [_hostPairState sizeToFit];
    
    if (host.online) {
        _hostStatus.text = @"Online";
        _hostStatus.textColor = [UIColor greenColor];
    } else {
        _hostStatus.text = @"Offline";
        _hostStatus.textColor = [UIColor grayColor];
    }
    [_hostStatus sizeToFit];
    
    float x = _hostButton.frame.origin.x + _hostButton.frame.size.width / 2;
    _hostLabel.center = CGPointMake(x, _hostButton.frame.origin.y + _hostButton.frame.size.height + LABEL_DY);
    _hostPairState.center = CGPointMake(x, _hostLabel.center.y + LABEL_DY);
    _hostStatus.center = CGPointMake(x, _hostPairState.center.y + LABEL_DY);
}

- (void) startUpdateLoop {
    [self performSelector:@selector(updateLoop) withObject:self afterDelay:REFRESH_CYCLE];
}

- (void) updateLoop {
    [self updateContentsForHost:_host];
    [self performSelector:@selector(updateLoop) withObject:self afterDelay:REFRESH_CYCLE];
}

- (void) hostLongClicked {
    [_callback hostLongClicked:_host view:self];
}

- (void) hostClicked {
    [_callback hostClicked:_host view:self];
}

- (void) addClicked {
    [_callback addHostClicked];
}

@end
