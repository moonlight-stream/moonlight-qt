//
//  Controller.swift
//  Moonlight
//
//  Created by David Aghassi on 4/11/16.
//  Copyright © 2016 Moonlight Stream. All rights reserved.
//

import Foundation

@objc
/**
 Defines a controller layout
 */
class Controller: NSObject {
  // Swift requires initial properties
  @objc var playerIndex: CInt = 0           // Controller number (e.g. 1, 2 ,3 etc)
  @objc var lastButtonFlags: CInt = 0
  @objc var emulatingButtonFlags: CInt = 0
  @objc var lastLeftTrigger: CChar = 0      // Last left trigger pressed
  @objc var lastRightTrigger: CChar = 0     // Last right trigger pressed
  @objc var lastLeftStickX: CShort = 0      // Last X direction the left joystick went
  @objc var lastLeftStickY: CShort = 0      // Last Y direction the left joystick went
  @objc var lastRightStickX: CShort = 0     // Last X direction the right joystick went
  @objc var lastRightStickY: CShort = 0     // Last Y direction the right joystick went
}
