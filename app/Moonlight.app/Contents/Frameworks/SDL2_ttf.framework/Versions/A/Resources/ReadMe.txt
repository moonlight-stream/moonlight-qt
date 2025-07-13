SDL_ttf is an example portable font rendering library for use with SDL.

The source code is available from: http://www.libsdl.org/projects/SDL_ttf

This library is distributed under the terms of the zlib license: http://www.zlib.net/zlib_license.html

Portions of this software are copyright © 2013 The FreeType Project (www.freetype.org).  All rights reserved.

This packages contains the SDL2_ttf.framework for OS X. Conforming with Apple guidelines, this framework contains both the SDL runtime component and development header files.

Requirements:
You must have the SDL2.framework installed.

To Install:
Copy the SDL2_ttf.framework to /Library/Frameworks

You may alternatively install it in <your home directory>/Library/Frameworks if your access privileges are not high enough. (Be aware that the Xcode templates we provide in the SDL Developer Extras package may require some adjustment for your system if you do this.)


(Partial) History of PB/Xcode projects:
2009-09-21 - Updated for 64-bit (Snow Leopard) Universal Binaries.
	Switched to 10.4 minimum requirement.
	Now dynamic linking Mac OS X bundled freetype directly in /usr/X11R6. (We used to just copy this one into our package since older versions of Mac OS X didn't ship with the library.)
	Reason: We no longer statically link libfreetype.a into the binary because Apple stopped supplying a libfreetype.a in the 10.5 and 10.6 SDKs.
	For static library target, you will need to link against libfreetype.dylib for your final app.

2006-01-31 - First entry in history. Updated for Universal Binaries. Static library of libfreetype has been updated by copying the version Apple provides from the 10.4u SDK.

