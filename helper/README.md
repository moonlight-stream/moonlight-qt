# MoonlightAWDLHelper — privileged helper tool (macOS only)

On macOS, Apple Wireless Direct Link (AWDL, interface `awdl0`) periodically
takes over the Wi-Fi radio for peer discovery (AirDrop, AirPlay, Sidecar,
Universal Control). During a game stream that shows up as latency spikes and
stuttering. The manual workaround is `sudo ifconfig awdl0 down`, but macOS
re-enables AWDL on its own, so it has to be re-run.

This helper lets Moonlight do that automatically and safely: when the
"Disable AWDL while streaming" setting is on, Moonlight asks this helper to
bring `awdl0` down at stream start and back up at stream end.

## How it works

- The helper is installed once via **SMJobBless** (ServiceManagement
  framework). That triggers a single admin authorization prompt the first
  time the setting is enabled. After that, no password prompts: the helper
  runs as root under launchd.
- The app talks to the helper over **XPC** (Mach service
  `com.moonlight-stream.Moonlight.AWDLHelper`, see
  `AWDLHelperProtocol.h`).
- The helper is bundled inside `Moonlight.app/Contents/Library/LaunchServices/`
  by `app.pro` (`QMAKE_BUNDLE_DATA`) and built by `moonlightawdlhelper.pro`
  (added as a macOS-only subdir in `moonlight-qt.pro`).

## Security model (non-negotiable)

- **Exactly one operation.** The XPC protocol takes a single boolean
  (AWDL on/off). It never accepts commands, paths, shell strings, or any
  other caller-controlled data.
- **No shell, fixed command.** The helper runs `/sbin/ifconfig` with the
  fixed arguments `awdl0 up` / `awdl0 down` via `NSTask`. There is no
  command-execution path in the binary.
- **Client verification.** The Mach service name is public, so any local
  process can *attempt* a connection. The helper verifies every client in
  `listener:shouldAcceptNewConnection:` against the Moonlight code-signing
  requirement (same team identifier as the helper itself when one is
  present, otherwise bundle identifier + Apple anchor). Anything else is
  rejected. This is the fix for the class of flaw described in
  CVE-2019-20057 against Apple's EvenBetterAuthorizationSample, whose
  helper accepted any client.
- **No GUI as root.** The helper is a command-line tool; it runs no GUI
  code and loads no plugins.
- **Plist gates.** The app's `Info.plist` declares the helper in
  `SMPrivilegedExecutables` (requirement the *helper* must satisfy);
  the helper's embedded Info.plist declares `SMAuthorizedClients`
  (requirement the *app* must satisfy). The two plists are embedded into
  the helper binary with `-sectcreate` (see `moonlightawdlhelper.pro`).
  SMJobBless refuses to install if either side fails its requirement.

## Signing requirement — read this before testing

SMJobBless requires the app and the helper to be **signed with the same
identity**. Consequences:

- **An unsigned build can never install the helper.** GitHub Actions
  builds (`build-win-mac.yml`) are unsigned, so a CI-produced DMG will
  show the setting but helper installation will fail with a clear warning
  and streaming will continue without AWDL management. This is by design,
  not a bug.
- **Testing requires a local signed build.** Either a Developer ID
  certificate ("Developer ID Application: …") or a self-signed certificate
  created in Keychain Access (Certificate Assistant → Create a
  Certificate, "Code Signing" type). Both app and helper must use the
  same certificate. The default requirement strings use
  `anchor apple generic`, which matches any Apple-issued certificate
  (including free "Mac Developer" certificates). If you sign with a
  purely self-signed local certificate, replace `anchor apple generic`
  with the requirement matching that certificate in both
  `app/Info.plist` (`SMPrivilegedExecutables`) and
  `MoonlightAWDLHelper-Info.plist` (`SMAuthorizedClients`).
- The helper binary should be signed with the identifier matching its
  label: `codesign --sign "IDENTITY" --identifier
  com.moonlight-stream.Moonlight.AWDLHelper <helper>`. (`codesign --deep`
  in `scripts/generate-dmg.sh` uses the file name as the identifier,
  which already matches, but pass `--identifier` explicitly for manual
  builds.)
- The app must **not** be sandboxed; SMJobBless does not work from a
  sandboxed app.

Note: SMJobBless is deprecated in macOS 13 in favour of `SMAppService`,
but it still works. A future change could switch installation to
`SMAppService.daemon` without changing the helper or the XPC protocol.

## Building and testing locally

Requires Xcode command-line tools and Qt (the macOS CI job uses Qt 6.11;
any recent Qt 6 works). From a clean checkout:

```sh
# Configure (universal build, as generate-dmg.sh does)
mkdir build && cd build
qmake ../moonlight-qt.pro QMAKE_APPLE_DEVICE_ARCHS="x86_64 arm64"
make -j$(sysctl -n hw.logicalcpu) release

# The helper must be inside the bundle before signing:
#   app/Moonlight.app/Contents/Library/LaunchServices/com.moonlight-stream.Moonlight.AWDLHelper
# (app.pro copies it there automatically; verify it exists.)

# Sign app and helper with the SAME identity (hardened runtime for
# Developer ID builds, as generate-dmg.sh does):
codesign --force --deep --options runtime --timestamp \
  --sign "Developer ID Application: Your Name (TEAMID)" \
  app/Moonlight.app
# If signing manually instead of --deep, sign the helper explicitly:
# codesign --force --options runtime --timestamp \
#   --sign "Developer ID Application: Your Name (TEAMID)" \
#   --identifier com.moonlight-stream.Moonlight.AWDLHelper \
#   app/Moonlight.app/Contents/Library/LaunchServices/com.moonlight-stream.Moonlight.AWDLHelper

# Sanity-check the signatures and requirements:
codesign -dv app/Moonlight.app
codesign -d -r - app/Moonlight.app/Contents/Library/LaunchServices/com.moonlight-stream.Moonlight.AWDLHelper
```

Then, in the signed app:

1. Open Settings, enable **Disable AWDL while streaming** (macOS only).
   The first enable triggers the admin prompt that installs the helper.
   If installation fails, the setting shows the error text and streaming
   is unaffected.
2. Verify the helper is installed:
   ```sh
   ls -l /Library/PrivilegedHelperTools/com.moonlight-stream.Moonlight.AWDLHelper
   sudo launchctl print system/com.moonlight-stream.Moonlight.AWDLHelper
   ```
3. Start a stream and confirm AWDL goes down, then ends up again:
   ```sh
   ifconfig awdl0   # "status: inactive" while streaming, "active" after
   ```
4. Confirm AirDrop/AirPlay/Sidecar/Universal Control work again after the
   stream ends, and that a failed launch (e.g. host unreachable) also
   leaves AWDL up.

## Behavior notes

- The setting applies at **stream start**. Toggling it mid-stream does
  nothing until the next stream.
- If helper installation failed or the helper is missing, Moonlight logs
  a warning and streams normally without touching AWDL — helper problems
  never break streaming.
- If Moonlight is killed mid-stream (crash, force-quit), the stream-end
  restore never runs. On next launch, Moonlight re-enables AWDL once as
  a safety net (only when the setting is on and the helper is installed).
- The safety net assumes a single Moonlight instance; a second instance
  launched while another is streaming would re-enable AWDL early. This
  is an accepted edge case for a personal-use setting.
- To remove the helper entirely: delete
  `/Library/PrivilegedHelperTools/com.moonlight-stream.Moonlight.AWDLHelper`
  and `/Library/LaunchDaemons/com.moonlight-stream.Moonlight.AWDLHelper.plist`
  (requires admin), or use `SMJobRemove` from a future uninstall path.
