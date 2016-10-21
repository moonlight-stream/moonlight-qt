//
//  UIAppView.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "UIAppView.h"

@implementation UIAppView {
    TemporaryApp* _app;
    UIButton* _appButton;
    UILabel* _appLabel;
    UIImageView* _appOverlay;
    NSCache* _artCache;
    id<AppCallback> _callback;
}

static UIImage* noImage;

- (id) initWithApp:(TemporaryApp*)app cache:(NSCache*)cache andCallback:(id<AppCallback>)callback {
    self = [super init];
    _app = app;
    _callback = callback;
    _artCache = cache;
    
    // Cache the NoAppImage ourselves to avoid
    // having to load it each time
    if (noImage == nil) {
        noImage = [UIImage imageNamed:@"NoAppImage"];
    }
    
    _appButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_appButton setBackgroundImage:noImage forState:UIControlStateNormal];
    [_appButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_appButton sizeToFit];
    [_appButton addTarget:self action:@selector(appClicked) forControlEvents:UIControlEventTouchUpInside];
    
    [self addSubview:_appButton];
    [self sizeToFit];
    
    _appButton.frame = CGRectMake(0, 0, noImage.size.width, noImage.size.height);
    self.frame = CGRectMake(0, 0, noImage.size.width, noImage.size.height);
    
    // Rasterizing the cell layer increases rendering performance by quite a bit
    self.layer.shouldRasterize = YES;
    self.layer.rasterizationScale = [UIScreen mainScreen].scale;
    
    return self;
}

- (void) appClicked {
    [_callback appClicked:_app];
}

- (void) updateAppImage {
    if (_appOverlay != nil) {
        [_appOverlay removeFromSuperview];
        _appOverlay = nil;
    }
    
    if ([_app.id isEqualToString:_app.host.currentGame]) {
        // Only create the app overlay if needed
        _appOverlay = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"Play"]];
        _appOverlay.layer.shadowColor = [UIColor blackColor].CGColor;
        _appOverlay.layer.shadowOffset = CGSizeMake(0, 0);
        _appOverlay.layer.shadowOpacity = 1;
        _appOverlay.layer.shadowRadius = 2.0;
        
        [self addSubview:_appOverlay];
        
        _appOverlay.frame = CGRectMake(0, 0, noImage.size.width / 2.f, noImage.size.height / 4.f);
        [_appOverlay setCenter:CGPointMake(self.frame.size.width/2, self.frame.size.height/6)];
    }
    
    // TODO: Improve no-app image detection
    BOOL noAppImage = false;
    
    if (_app.image != nil) {
        // Load the decoded image from the cache
        UIImage* appImage = [_artCache objectForKey:_app];
        if (appImage == nil) {
            // Not cached; we have to decode this now
            appImage = [UIImage imageWithData:_app.image];
            [_artCache setObject:appImage forKey:_app];
        }
        
        // This size of image might be blank image received from GameStream.
        if (!(appImage.size.width == 130.f && appImage.size.height == 180.f) && // GFE 2.0
            !(appImage.size.width == 628.f && appImage.size.height == 888.f)) { // GFE 3.0
            _appButton.frame = CGRectMake(0, 0, appImage.size.width / 2, appImage.size.height / 2);
            self.frame = CGRectMake(0, 0, appImage.size.width / 2, appImage.size.height / 2);
            _appOverlay.frame = CGRectMake(0, 0, self.frame.size.width / 2.f, self.frame.size.height / 4.f);
            _appOverlay.layer.shadowRadius = 4.0;
            [_appOverlay setCenter:CGPointMake(self.frame.size.width/2, self.frame.size.height/6)];
            [_appButton setBackgroundImage:appImage forState:UIControlStateNormal];
            [self setNeedsDisplay];
        } else {
            noAppImage = true;
        }
    } else {
        noAppImage = true;
    }
    
    if (noAppImage) {
        _appLabel = [[UILabel alloc] init];
        CGFloat padding = 4.f;
        [_appLabel setFrame: CGRectMake(padding, padding, _appButton.frame.size.width - 2 * padding, _appButton.frame.size.height - 2 * padding)];
        [_appLabel setTextColor:[UIColor whiteColor]];
        [_appLabel setFont:[UIFont fontWithName:@"Roboto-Regular" size:10.f]];
        [_appLabel setBaselineAdjustment:UIBaselineAdjustmentAlignCenters];
        [_appLabel setTextAlignment:NSTextAlignmentCenter];
        [_appLabel setLineBreakMode:NSLineBreakByWordWrapping];
        [_appLabel setNumberOfLines:0];
        [_appLabel setText:_app.name];
        [_appButton addSubview:_appLabel];
    }
    
}

@end
