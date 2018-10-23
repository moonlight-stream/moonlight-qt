//
//  SettingsViewController.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/27/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "SettingsViewController.h"
#import "TemporarySettings.h"
#import "DataManager.h"

@implementation SettingsViewController {
    NSInteger _bitrate;
    Boolean _adjustedForSafeArea;
}
static NSString* bitrateFormat = @"Bitrate: %.1f Mbps";
static const int bitrateTable[] = {
    500,
    1000,
    1500,
    2000,
    2500,
    3000,
    4000,
    5000,
    6000,
    7000,
    8000,
    9000,
    10000,
    12000,
    15000,
    18000,
    20000,
    30000,
    40000,
    50000,
    60000,
    70000,
    80000,
    100000
};

-(int)getSliderValueForBitrate:(NSInteger)bitrate {
    int i;
    
    for (i = 0; i < (sizeof(bitrateTable) / sizeof(*bitrateTable)); i++) {
        if (bitrate <= bitrateTable[i]) {
            return i;
        }
    }
    
    // Return the last entry in the table
    return i - 1;
}

-(void)viewDidLayoutSubviews {
    // On iPhone layouts, this view is rooted at a ScrollView. To make it
    // scrollable, we'll update content size here.
    if (self.scrollView != nil) {
        CGFloat highestViewY = 0;
        
        // Enumerate the scroll view's subviews looking for the
        // highest view Y value to set our scroll view's content
        // size.
        for (UIView* view in self.scrollView.subviews) {
            // UIScrollViews have 2 default UIImageView children
            // which represent the horizontal and vertical scrolling
            // indicators. Ignore these when computing content size.
            if ([view isKindOfClass:[UIImageView class]]) {
                continue;
            }
            
            CGFloat currentViewY = view.frame.origin.y + view.frame.size.height;
            if (currentViewY > highestViewY) {
                highestViewY = currentViewY;
            }
        }
        
        // Add a bit of padding so the view doesn't end right at the button of the display
        self.scrollView.contentSize = CGSizeMake(self.scrollView.contentSize.width,
                                                 highestViewY + 20);
    }
    
    // Adjust the subviews for the safe area on the iPhone X.
    if (!_adjustedForSafeArea) {
        if (@available(iOS 11.0, *)) {
            for (UIView* view in self.view.subviews) {
                // HACK: The official safe area is much too large for our purposes
                // so we'll just use the presence of any safe area to indicate we should
                // pad by 20.
                if (self.view.safeAreaInsets.left >= 20 || self.view.safeAreaInsets.right >= 20) {
                    view.frame = CGRectMake(view.frame.origin.x + 20, view.frame.origin.y, view.frame.size.width, view.frame.size.height);
                }
            }
        }

        _adjustedForSafeArea = true;
    }
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    DataManager* dataMan = [[DataManager alloc] init];
    TemporarySettings* currentSettings = [dataMan getSettings];
    
    // Ensure we pick a bitrate that falls exactly onto a slider notch
    _bitrate = bitrateTable[[self getSliderValueForBitrate:[currentSettings.bitrate intValue]]];
    
    NSInteger framerate;
    switch ([currentSettings.framerate integerValue]) {
        case 30:
            framerate = 0;
            break;
        default:
        case 60:
            framerate = 1;
            break;
        case 120:
            framerate = 2;
            break;
    }
    NSInteger resolution;
    switch ([currentSettings.height integerValue]) {
        case 360:
            resolution = 0;
            break;
        default:
        case 720:
            resolution = 1;
            break;
        case 1080:
            resolution = 2;
            break;
    }
    
    // Only show the 120 FPS option if we have a > 60-ish Hz display
    bool enable120Fps = false;
    if (@available(iOS 10.3, *)) {
        if ([UIScreen mainScreen].maximumFramesPerSecond > 62) {
            enable120Fps = true;
        }
    }
    if (!enable120Fps) {
        [self.framerateSelector removeSegmentAtIndex:2 animated:NO];
    }
    
    [self.optimizeSettingsSelector setSelectedSegmentIndex:currentSettings.optimizeGames ? 1 : 0];
    [self.multiControllerSelector setSelectedSegmentIndex:currentSettings.multiController ? 1 : 0];
    [self.audioOnPCSelector setSelectedSegmentIndex:currentSettings.playAudioOnPC ? 1 : 0];
    [self.hevcSelector setSelectedSegmentIndex:currentSettings.useHevc ? 1 : 0];
    NSInteger onscreenControls = [currentSettings.onscreenControls integerValue];
    [self.resolutionSelector setSelectedSegmentIndex:resolution];
    [self.resolutionSelector addTarget:self action:@selector(newResolutionFpsChosen) forControlEvents:UIControlEventValueChanged];
    [self.framerateSelector setSelectedSegmentIndex:framerate];
    [self.framerateSelector addTarget:self action:@selector(newResolutionFpsChosen) forControlEvents:UIControlEventValueChanged];
    [self.onscreenControlSelector setSelectedSegmentIndex:onscreenControls];
    [self.bitrateSlider setMinimumValue:0];
    [self.bitrateSlider setMaximumValue:(sizeof(bitrateTable) / sizeof(*bitrateTable)) - 1];
    [self.bitrateSlider setValue:[self getSliderValueForBitrate:_bitrate] animated:YES];
    [self.bitrateSlider addTarget:self action:@selector(bitrateSliderMoved) forControlEvents:UIControlEventValueChanged];
    [self updateBitrateText];
}

- (void) newResolutionFpsChosen {
    NSInteger fps = [self getChosenFrameRate];
    NSInteger width = [self getChosenStreamWidth];
    NSInteger height = [self getChosenStreamHeight];
    NSInteger defaultBitrate;
    
    // This table prefers 16:10 resolutions because they are
    // only slightly more pixels than the 16:9 equivalents, so
    // we don't want to bump those 16:10 resolutions up to the
    // next 16:9 slot.
    //
    // This logic is shamelessly stolen from Moonlight Qt:
    // https://github.com/moonlight-stream/moonlight-qt/blob/master/app/settings/streamingpreferences.cpp
    
    if (width * height <= 640 * 360) {
        defaultBitrate = 1000 * (fps / 30.0);
    }
    // This covers 1280x720 and 1280x800 too
    else if (width * height <= 1366 * 768) {
        defaultBitrate = 5000 * (fps / 30.0);
    }
    else if (width * height <= 1920 * 1200) {
        defaultBitrate = 10000 * (fps / 30.0);
    }
    else if (width * height <= 2560 * 1600) {
        defaultBitrate = 20000 * (fps / 30.0);
    }
    else /* if (width * height <= 3840 * 2160) */ {
        defaultBitrate = 40000 * (fps / 30.0);
    }
    
    // We should always be exactly on a slider position with default bitrates
    _bitrate = defaultBitrate;
    assert(bitrateTable[[self getSliderValueForBitrate:_bitrate]] == _bitrate);
    [self.bitrateSlider setValue:[self getSliderValueForBitrate:_bitrate] animated:YES];
    
    [self updateBitrateText];
}

- (void) bitrateSliderMoved {
    assert(self.bitrateSlider.value < (sizeof(bitrateTable) / sizeof(*bitrateTable)));
    _bitrate = bitrateTable[(int)self.bitrateSlider.value];
    [self updateBitrateText];
}

- (void) updateBitrateText {
    // Display bitrate in Mbps
    [self.bitrateLabel setText:[NSString stringWithFormat:bitrateFormat, _bitrate / 1000.]];
}

- (NSInteger) getChosenFrameRate {
    switch ([self.framerateSelector selectedSegmentIndex]) {
        case 0:
            return 30;
        case 1:
            return 60;
        case 2:
            return 120;
        default:
            abort();
    }
}

- (NSInteger) getChosenStreamHeight {
    const int resolutionTable[] = { 360, 720, 1080 };
    return resolutionTable[[self.resolutionSelector selectedSegmentIndex]];
}

- (NSInteger) getChosenStreamWidth {
    // Assumes fixed 16:9 aspect ratio
    return ([self getChosenStreamHeight] * 16) / 9;
}

- (void) saveSettings {
    DataManager* dataMan = [[DataManager alloc] init];
    NSInteger framerate = [self getChosenFrameRate];
    NSInteger height = [self getChosenStreamHeight];
    NSInteger width = [self getChosenStreamWidth];
    NSInteger onscreenControls = [self.onscreenControlSelector selectedSegmentIndex];
    BOOL optimizeGames = [self.optimizeSettingsSelector selectedSegmentIndex] == 1;
    BOOL multiController = [self.multiControllerSelector selectedSegmentIndex] == 1;
    BOOL audioOnPC = [self.audioOnPCSelector selectedSegmentIndex] == 1;
    BOOL useHevc = [self.hevcSelector selectedSegmentIndex] == 1;
    [dataMan saveSettingsWithBitrate:_bitrate
                           framerate:framerate
                              height:height
                               width:width
                    onscreenControls:onscreenControls
                              remote:NO
                       optimizeGames:optimizeGames
                     multiController:multiController
                           audioOnPC:audioOnPC
                             useHevc:useHevc
                           enableHdr:NO];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


#pragma mark - Navigation

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
}


@end
