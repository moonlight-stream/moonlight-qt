//
//  Controller.h
//  Moonlight
//
//  Created by Cameron Gutman on 2/11/19.
//  Copyright © 2019 Moonlight Game Streaming Project. All rights reserved.
//

@import GameController;

@interface Controller : NSObject

@property (nullable, nonatomic, retain) GCController* gamepad;
@property (nonatomic)                   int playerIndex;
@property (nonatomic)                   int lastButtonFlags;
@property (nonatomic)                   int emulatingButtonFlags;
@property (nonatomic)                   unsigned char lastLeftTrigger;
@property (nonatomic)                   unsigned char lastRightTrigger;
@property (nonatomic)                   short lastLeftStickX;
@property (nonatomic)                   short lastLeftStickY;
@property (nonatomic)                   short lastRightStickX;
@property (nonatomic)                   short lastRightStickY;
@property (nonatomic)                   unsigned short lowFreqMotor;
@property (nonatomic)                   unsigned short highFreqMotor;

@end
