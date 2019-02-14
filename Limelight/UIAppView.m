//
//  UIAppView.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/22/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "UIAppView.h"
#import "AppAssetManager.h"

static const float REFRESH_CYCLE = 2.0f;

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
    
#if TARGET_OS_TV
    _appButton = [UIButton buttonWithType:UIButtonTypeSystem];
#else
    _appButton = [UIButton buttonWithType:UIButtonTypeCustom];
#endif
    [_appButton setBackgroundImage:noImage forState:UIControlStateNormal];
    [_appButton setContentEdgeInsets:UIEdgeInsetsMake(0, 4, 0, 4)];
    [_appButton sizeToFit];
    if (@available(iOS 9.0, tvOS 9.0, *)) {
        [_appButton addTarget:self action:@selector(appClicked) forControlEvents:UIControlEventPrimaryActionTriggered];
    }
    else {
        [_appButton addTarget:self action:@selector(appClicked) forControlEvents:UIControlEventTouchUpInside];
    }
    
    [self addSubview:_appButton];
    [self sizeToFit];
    
    // Rasterizing the cell layer increases rendering performance by quite a bit
    self.layer.shouldRasterize = YES;
    self.layer.rasterizationScale = [UIScreen mainScreen].scale;
    
    [self updateAppImage];
    [self startUpdateLoop];
    
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
    
    _appButton.frame = CGRectMake(0, 0, noImage.size.width, noImage.size.height);
    self.frame = CGRectMake(0, 0, noImage.size.width, noImage.size.height);
    
    if ([_app.id isEqualToString:_app.host.currentGame]) {
#if TARGET_OS_TV
        _appButton.layer.masksToBounds = NO;

        _appButton.layer.shadowColor = [UIColor greenColor].CGColor;
        _appButton.layer.shadowOffset = CGSizeMake(5,8);
        _appButton.layer.shadowOpacity = 0.7;
#else
        // Only create the app overlay if needed
        _appOverlay = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"Play"]];
        _appOverlay.layer.shadowColor = [UIColor blackColor].CGColor;
        _appOverlay.layer.shadowOffset = CGSizeMake(0, 0);
        _appOverlay.layer.shadowOpacity = 1;
        _appOverlay.layer.shadowRadius = 2.0;
        [self addSubview:_appOverlay];
        
        _appOverlay.frame = CGRectMake(0, 0, noImage.size.width / 2.f, noImage.size.height / 4.f);
        [_appOverlay setCenter:CGPointMake(self.frame.size.width/2, self.frame.size.height/6)];
#endif
    }
    
    BOOL noAppImage = false;
    
    // First check the memory cache
    UIImage* appImage = [_artCache objectForKey:_app];
    if (appImage == nil) {
        // Next try to load from the on disk cache
        appImage = [UIImage imageWithContentsOfFile:[AppAssetManager boxArtPathForApp:_app]];
        if (appImage != nil) {
            [_artCache setObject:appImage forKey:_app];
        }
    }
    if (appImage != nil) {
        // This size of image might be blank image received from GameStream.
        // TODO: Improve no-app image detection
        if (!(appImage.size.width == 130.f && appImage.size.height == 180.f) && // GFE 2.0
            !(appImage.size.width == 628.f && appImage.size.height == 888.f)) { // GFE 3.0
#if TARGET_OS_TV
            //custom image to do TvOS hover popup effect
            UIImageView *imageView = [[UIImageView alloc] initWithImage:appImage];
            imageView.userInteractionEnabled = YES;
            imageView.adjustsImageWhenAncestorFocused = YES;
            imageView.frame = CGRectMake(0, 0, 200, 265);
            [_appButton addSubview:imageView];
            
            _appButton.frame = CGRectMake(0, 0, 200, 265);
            self.frame = CGRectMake(0, 0, 200, 265);
#else
            _appButton.frame = CGRectMake(0, 0, appImage.size.width / 2, appImage.size.height / 2);
            self.frame = CGRectMake(0, 0, appImage.size.width / 2, appImage.size.height / 2);

            _appOverlay.frame = CGRectMake(0, 0, self.frame.size.width / 2.f, self.frame.size.height / 4.f);
            _appOverlay.layer.shadowRadius = 4.0;
            [_appOverlay setCenter:CGPointMake(self.frame.size.width/2, self.frame.size.height/6)];
#endif
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
        [_appLabel setTextColor:[UIColor whiteColor]];
        [_appLabel setBaselineAdjustment:UIBaselineAdjustmentAlignCenters];
        [_appLabel setTextAlignment:NSTextAlignmentCenter];
        [_appLabel setLineBreakMode:NSLineBreakByWordWrapping];
        [_appLabel setNumberOfLines:0];
        [_appLabel setText:_app.name];
#if TARGET_OS_TV
        [_appLabel setFont:[UIFont systemFontOfSize:16]];
        [_appLabel setAdjustsFontSizeToFitWidth:YES];
        [_appLabel setFrame: CGRectMake(0, 0, 200, 265)];
        //custom image to do TvOS hover popup effect
        UIImageView *imageView = [[UIImageView alloc] initWithImage:appImage];
        imageView.userInteractionEnabled = YES;
        imageView.adjustsImageWhenAncestorFocused = YES;
        imageView.frame = CGRectMake(0, 0, 200, 265);
        UIGraphicsBeginImageContextWithOptions(_appLabel.frame.size, false, 0);
        [imageView.layer renderInContext:(UIGraphicsGetCurrentContext())];
        [_appLabel.layer renderInContext:(UIGraphicsGetCurrentContext())];
        UIImage *imageWithText = UIGraphicsGetImageFromCurrentImageContext();
        UIGraphicsEndImageContext();
        [imageView setImage:imageWithText];
        [_appButton addSubview:imageView];
        _appButton.frame = CGRectMake(0, 0, 200, 265);
        self.frame = CGRectMake(0, 0, 200, 265);
#else
        CGFloat padding = 4.f;
        [_appLabel setFrame: CGRectMake(padding, padding, _appButton.frame.size.width - 2 * padding, _appButton.frame.size.height - 2 * padding)];
        [_appButton addSubview:_appLabel];
#endif
    }
    
}

- (void) startUpdateLoop {
    [self performSelector:@selector(updateLoop) withObject:self afterDelay:REFRESH_CYCLE];
}

- (void) updateLoop {
    // Update the app image if neccessary
    if ((_appOverlay != nil && ![_app.id isEqualToString:_app.host.currentGame]) ||
        (_appOverlay == nil && [_app.id isEqualToString:_app.host.currentGame])) {
#if !TARGET_OS_TV
        [self updateAppImage];
#endif
    }
    
    // Stop updating when we detach from our parent view
    if (self.superview != nil) {
        [self performSelector:@selector(updateLoop) withObject:self afterDelay:REFRESH_CYCLE];
    }
}

@end
