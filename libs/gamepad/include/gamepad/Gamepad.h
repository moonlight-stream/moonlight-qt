/*
  Copyright (c) 2014 Alex Diener
  
  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.
  
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:
  
  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
  
  Alex Diener alex@ludobloom.com
*/

#ifndef __GAMEPAD_H__
#define __GAMEPAD_H__
#ifdef __cplusplus
extern "C" {
#endif

#if _MSC_VER <= 1600
#define bool int
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif

struct Gamepad_device {
	// Unique device identifier for application session, starting at 0 for the first device attached and
	// incrementing by 1 for each additional device. If a device is removed and subsequently reattached
	// during the same application session, it will have a new deviceID.
	unsigned int deviceID;
	
	// Human-readable device name
	const char * description;
	
	// USB vendor/product IDs as returned by the driver. Can be used to determine the particular model of device represented.
	int vendorID;
	int productID;
	
	// Number of axis elements belonging to the device
	unsigned int numAxes;
	
	// Number of button elements belonging to the device
	unsigned int numButtons;
	
	// Array[numAxes] of values representing the current state of each axis, in the range [-1..1]
	float * axisStates;
	
	// Array[numButtons] of values representing the current state of each button
	bool * buttonStates;
	
	// Platform-specific device data storage. Don't touch unless you know what you're doing and don't
	// mind your code breaking in future versions of this library.
	void * privateData;
};

/* Initializes gamepad library and detects initial devices. Call this before any other Gamepad_*()
   function, other than callback registration functions. If you want to receive deviceAttachFunc
   callbacks from devices detected in Gamepad_init(), you must call Gamepad_deviceAttachFunc()
   before calling Gamepad_init().
   
   This function must be called from the same thread that will be calling Gamepad_processEvents()
   and Gamepad_detectDevices(). */
void Gamepad_init();

/* Tears down all data structures created by the gamepad library and releases any memory that was
   allocated. It is not necessary to call this function at application termination, but it's
   provided in case you want to free memory associated with gamepads at some earlier time. */
void Gamepad_shutdown();

/* Returns the number of currently attached gamepad devices. */
unsigned int Gamepad_numDevices();

/* Returns the specified Gamepad_device struct, or NULL if deviceIndex is out of bounds. */
struct Gamepad_device * Gamepad_deviceAtIndex(unsigned int deviceIndex);

/* Polls for any devices that have been attached since the last call to Gamepad_detectDevices() or
   Gamepad_init(). If any new devices are found, the callback registered with
   Gamepad_deviceAttachFunc() (if any) will be called once per newly detected device.
   
   Note that depending on implementation, you may receive button and axis event callbacks for
   devices that have not yet been detected with Gamepad_detectDevices(). You can safely ignore
   these events, but be aware that your callbacks might receive a device ID that hasn't been seen
   by your deviceAttachFunc. */
void Gamepad_detectDevices();

/* Reads pending input from all attached devices and calls the appropriate input callbacks, if any
   have been registered. */
void Gamepad_processEvents();

/* Registers a function to be called whenever a device is attached. The specified function will be
   called only during calls to Gamepad_init() and Gamepad_detectDevices(), in the thread from
   which those functions were called. Calling this function with a NULL argument will stop any
   previously registered callback from being called subsequently. */
void Gamepad_deviceAttachFunc(void (* callback)(struct Gamepad_device * device, void * context), void * context);

/* Registers a function to be called whenever a device is detached. The specified function can be
   called at any time, and will not necessarily be called from the main thread. Calling this
   function with a NULL argument will stop any previously registered callback from being called
   subsequently. */
void Gamepad_deviceRemoveFunc(void (* callback)(struct Gamepad_device * device, void * context), void * context);

/* Registers a function to be called whenever a button on any attached device is pressed. The
   specified function will be called only during calls to Gamepad_processEvents(), in the
   thread from which Gamepad_processEvents() was called. Calling this function with a NULL
   argument will stop any previously registered callback from being called subsequently.  */
void Gamepad_buttonDownFunc(void (* callback)(struct Gamepad_device * device, unsigned int buttonID, double timestamp, void * context), void * context);

/* Registers a function to be called whenever a button on any attached device is released. The
   specified function will be called only during calls to Gamepad_processEvents(), in the
   thread from which Gamepad_processEvents() was called. Calling this function with a NULL
   argument will stop any previously registered callback from being called subsequently.  */
void Gamepad_buttonUpFunc(void (* callback)(struct Gamepad_device * device, unsigned int buttonID, double timestamp, void * context), void * context);

/* Registers a function to be called whenever an axis on any attached device is moved. The
   specified function will be called only during calls to Gamepad_processEvents(), in the
   thread from which Gamepad_processEvents() was called. Calling this function with a NULL
   argument will stop any previously registered callback from being called subsequently.  */
void Gamepad_axisMoveFunc(void (* callback)(struct Gamepad_device * device, unsigned int axisID, float value, float lastValue, double timestamp, void * context), void * context);

#ifdef __cplusplus
}
#endif
#endif
