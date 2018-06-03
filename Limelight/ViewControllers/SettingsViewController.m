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
    50000
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
    
    NSInteger framerate = [currentSettings.framerate integerValue] == 30 ? 0 : 1;
    NSInteger resolution;
    if ([currentSettings.height integerValue] == 360) {
        resolution = 0;
    } else if ([currentSettings.height integerValue] == 720) {
        resolution = 1;
    } else if ([currentSettings.height integerValue] == 1080) {
        resolution = 2;
    } else {
        resolution = 1;
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
    NSInteger frameRate = [self getChosenFrameRate];
    NSInteger resHeight = [self getChosenStreamHeight];
    NSInteger defaultBitrate;
    
    // 1080p60 is 20 Mbps
    if (frameRate == 60 && resHeight == 1080) {
        defaultBitrate = 20000;
    }
    // 720p60 and 1080p30 are 10 Mbps
    else if ((frameRate == 60 && resHeight == 720) ||
             (frameRate == 30 && resHeight == 1080)) {
        defaultBitrate = 10000;
    }
    // 720p30 is 5 Mbps
    else if (resHeight == 720) {
        defaultBitrate = 5000;
    }
    // 360p60 is 2 Mbps
    else if (frameRate == 60 && resHeight == 360) {
        defaultBitrate = 2000;
    }
    // 360p30 is 1 Mbps
    else {
        defaultBitrate = 1000;
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
    return [self.framerateSelector selectedSegmentIndex] == 0 ? 30 : 60;
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
