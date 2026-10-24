// awdlhelper.h -- App-side client for the MoonlightAWDLHelper privileged
// helper tool. macOS only; the corresponding .mm file is compiled only
// when Q_OS_MACOS is defined (see app.pro).
//
// These are plain C++ functions so they can be called from Qt code
// (e.g. Session) without pulling Objective-C headers into the rest of
// the app. All functions are synchronous and must NOT be called on the
// main thread: the XPC reply is waited on with a semaphore.

#pragma once

#include <QString>

// Mach service name / launchd job label of the helper. Must match the
// helper's own label and the SMPrivilegedExecutables key in Info.plist.
extern const char * const kMoonlightAWDLHelperLabel;

// Is the helper currently installed (blessed) in /Library/PrivilegedHelperTools?
bool awdlHelperIsInstalled();

// Install (bless) the helper. Triggers the one-time admin authorization
// prompt. Returns false and fills *errorMessage on failure (user cancelled,
// helper missing from the bundle, signing mismatch, ...). The caller should
// surface the failure as a non-blocking warning, never as a streaming error.
bool awdlHelperInstall(QString *errorMessage);

// Ask the helper to enable (true) or disable (false) AWDL. Blocks the
// calling thread for up to timeoutMs waiting for the helper's reply.
// Returns false and fills *errorMessage if the helper is not installed,
// the connection fails, or the helper reports an error.
bool awdlHelperSetEnabled(bool enabled, int timeoutMs, QString *errorMessage);

// Best-effort restore of AWDL on app launch. Covers the case where a
// previous Moonlight process was killed mid-stream and never got to
// re-enable AWDL. No-op unless the helper is installed. Runs on a
// background queue and never blocks the caller.
void awdlHelperRestoreAtStartupAsync();
