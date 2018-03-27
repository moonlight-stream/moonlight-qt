//
//  keyboardTranslation.h
//  Moonlight macOS
//
//  Created by Felix Kratz on 10.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//
#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

#ifndef keyboardTranslation_h
#define keyboardTranslation_h

CGKeyCode keyCharFromKeyCode(CGKeyCode keyCode);
CGKeyCode keyCodeFromModifierKey(NSEventModifierFlags keyModifier);
char modifierFlagForKeyModifier(NSEventModifierFlags keyModifier);

#endif /* keyboardTranslation_h */
