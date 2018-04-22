//
//  OverlayView.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 01.04.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import "StreamView.h"
#import "OverlayView.h"
#import "NetworkTraffic.h"

@implementation OverlayView {
    StreamView* _streamView;
    bool statsDisplayed;
    unsigned long lastNetworkDown;
    unsigned long lastNetworkUp;
    NSTextField* _textFieldIncomingBitrate;
    NSTextField* _textFieldOutgoingBitrate;
    NSTextField* _textFieldCodec;
    NSTextField* _textFieldFramerate;
    NSTextField* _stageLabel;

    NSTimer* _statTimer;
}

- (id)initWithFrame:(NSRect)frame sender:(StreamView*)sender
{
    self = [super initWithFrame:frame];
    if (self) {
        _streamView = sender;
    }
    return self;
}

- (void)initStats {
    _textFieldCodec = [[NSTextField alloc] initWithFrame:NSMakeRect(5, NSScreen.mainScreen.frame.size.height - 22, 200, 17)];
    _textFieldIncomingBitrate = [[NSTextField alloc] initWithFrame:NSMakeRect(5, 5, 250, 17)];
    _textFieldOutgoingBitrate = [[NSTextField alloc] initWithFrame:NSMakeRect(5, 5 + 20, 250, 17)];
    _textFieldFramerate = [[NSTextField alloc] initWithFrame:NSMakeRect(NSScreen.mainScreen.frame.size.width - 50, NSScreen.mainScreen.frame.size.height - 22, 50, 17)];
    
    [self setupTextField:_textFieldOutgoingBitrate];
    [self setupTextField:_textFieldIncomingBitrate];
    [self setupTextField:_textFieldCodec];
    [self setupTextField:_textFieldFramerate];
}

- (void)setupTextField:(NSTextField*)textField {
    textField.drawsBackground = false;
    textField.bordered = false;
    textField.editable = false;
    textField.alignment = NSTextAlignmentLeft;
    textField.textColor = [NSColor whiteColor];
    [self addSubview:textField];
}

- (void)toggleOverlay:(int)codec {
    statsDisplayed = !statsDisplayed;
    if (statsDisplayed) {
        _streamView.frameCount = 0;
        if (_textFieldIncomingBitrate == nil || _textFieldCodec == nil || _textFieldOutgoingBitrate == nil || _textFieldFramerate == nil) {
            [self initStats];
        }
        _statTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector:@selector(statTimerTick) userInfo:nil repeats:true];
        NSLog(@"display stats");
        if (codec == 1) {
            _textFieldCodec.stringValue = @"Codec: H.264";
        }
        else if (codec == 256) {
            _textFieldCodec.stringValue = @"Codec: HEVC/H.265";
        }
        else {
            _textFieldCodec.stringValue = @"Codec: Unknown";
        }
        [self statTimerTick];
    }
    else    {
        [_statTimer invalidate];
        _textFieldCodec.stringValue = @"";
        _textFieldIncomingBitrate.stringValue = @"";
        _textFieldOutgoingBitrate.stringValue = @"";
        _textFieldFramerate.stringValue = @"";
    }
}

- (void)statTimerTick {
    _textFieldFramerate.stringValue = [NSString stringWithFormat:@"%i fps", _streamView.frameCount];
    _streamView.frameCount = 0;
    
    unsigned long currentNetworkDown = getBytesDown();
    _textFieldIncomingBitrate.stringValue = [NSString stringWithFormat:@"Incoming Bitrate (System): %lu kbps", (currentNetworkDown - lastNetworkDown)*8 / 1000];
    lastNetworkDown = currentNetworkDown;
    
    unsigned long currentNetworkUp = getBytesUp();
    _textFieldOutgoingBitrate.stringValue = [NSString stringWithFormat:@"Outgoing Bitrate (System): %lu kbps", (currentNetworkUp - lastNetworkUp)*8 / 1000];
    lastNetworkUp = currentNetworkUp;
}
@end
