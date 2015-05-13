//
//  SettingsViewController.m
//  Moonlight
//
//  Created by Diego Waxemberg on 10/27/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "SettingsViewController.h"
#import "Settings.h"
#import "DataManager.h"

#define BITRATE_INTERVAL 500 // in kbps

@implementation SettingsViewController {
    NSInteger _bitrate;
}
static NSString* bitrateFormat = @"Bitrate: %.1f Mbps";


- (void)viewDidLoad {
    [super viewDidLoad];
    
    DataManager* dataMan = [[DataManager alloc] init];
    Settings* currentSettings = [dataMan retrieveSettings];
    
    // Bitrate is persisted in kbps
    _bitrate = [currentSettings.bitrate integerValue];
    NSInteger framerate = [currentSettings.framerate integerValue] == 30 ? 0 : 1;
    NSInteger resolution;
    if ([currentSettings.height integerValue] == 720) {
        resolution = 0;
    } else if ([currentSettings.height integerValue] == 1080) {
        resolution = 1;
    } else {
        resolution = 2;
    }
    NSInteger onscreenControls = [currentSettings.onscreenControls integerValue];
    
    [self.resolutionSelector setSelectedSegmentIndex:resolution];
    [self.framerateSelector setSelectedSegmentIndex:framerate];
    [self.onscreenControlSelector setSelectedSegmentIndex:onscreenControls];
    [self.bitrateSlider setValue:(_bitrate / BITRATE_INTERVAL) animated:YES];
    [self.bitrateSlider addTarget:self action:@selector(bitrateSliderMoved) forControlEvents:UIControlEventValueChanged];
    [self updateBitrateText];
}

- (void) bitrateSliderMoved {
    _bitrate = BITRATE_INTERVAL * (int)self.bitrateSlider.value;
    [self updateBitrateText];
}

- (void) updateBitrateText {
    // Display bitrate in Mbps
    [self.bitrateLabel setText:[NSString stringWithFormat:bitrateFormat, _bitrate / 1000.]];
}

- (void) saveSettings {
    DataManager* dataMan = [[DataManager alloc] init];
    NSInteger framerate = [self.framerateSelector selectedSegmentIndex] == 0 ? 30 : 60;
    NSInteger height;
    NSInteger width;
    if ([self.resolutionSelector selectedSegmentIndex] == 2) {
        // Get screen native resolution
        height = [UIScreen mainScreen].bounds.size.height * [UIScreen mainScreen].scale;
        width = [UIScreen mainScreen].bounds.size.width * [UIScreen mainScreen].scale;
        
        UIAlertController *alertResolution = [UIAlertController alertControllerWithTitle:@"Native resolution" message:[NSString stringWithFormat:@"You must select the following resolution in the game settings: %lix%li", (long)width, (long)height] preferredStyle:UIAlertControllerStyleAlert];
        [alertResolution addAction:[UIAlertAction actionWithTitle:@"Ok" style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:alertResolution animated:YES completion:nil];
    }
    else
    {
        height = [self.resolutionSelector selectedSegmentIndex] == 0 ? 720 : 1080;
        width = height == 720 ? 1280 : 1920;
    }
    NSInteger onscreenControls = [self.onscreenControlSelector selectedSegmentIndex];
    [dataMan saveSettingsWithBitrate:_bitrate framerate:framerate height:height width:width onscreenControls:onscreenControls];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


#pragma mark - Navigation

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
}


@end
