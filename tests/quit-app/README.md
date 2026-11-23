# Quit request regression test

This test compiles the production `NvHTTP` implementation and exercises it against
a loopback HTTPS server with client certificates. No paired hosts or user settings
are used. Python 3 with `cryptography`, Qt, a C++ compiler, and the normal Moonlight
OpenSSL dependencies are required.

From a configured build environment, create a separate build directory, run
`qmake /path/to/tests/quit-app/quit-app.pro`, then build with `jom release` on
Windows or `make` on Linux. Run the resulting `quit-app-test` executable with the
absolute path to `server.py` as its only argument. Qt/OpenSSL libraries must be on
the runtime library path. The test takes approximately 32 seconds.

Cases cover success, a refused cancel, a missing cancel result, a host error,
GFE's success response while an application remains running, and a successful
cancel delayed beyond the former 30-second timeout. A nonzero exit code indicates
failure. This validates the protocol path, not a real streaming session or UI.
