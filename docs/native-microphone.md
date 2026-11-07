# Native NVIDIA GameStream microphone forwarding

This desktop client contribution forwards the default microphone on macOS, Windows and Linux through the native NVIDIA GameStream path. It is opt-in for the current application run and requires the separately documented Windows host integration. It is not Sunshine microphone support.

## Use and consent

Enable **Forward microphone to NVIDIA GameStream for this run** in audio settings, then connect to the paired host. On macOS the first attempt requests microphone permission; grant it and reconnect. A denied request leaves ordinary streaming available and displays a privacy-settings explanation. Permission callbacks never start capture after the session that requested them has ended.

While forwarding, the streaming status includes **Microphone forwarding ON** and the stop shortcut. Press **Control + Option + Shift + N** on macOS or **Ctrl + Alt + Shift + N** on Windows/Linux to stop immediately. Reconnect to enable forwarding again. Stop/error notices expire after eight seconds; connection and gamepad messages retain their own portion of the status display. Capture stops synchronously before the streaming window is torn down and before deferred network cleanup or finished signals return to the Qt UI.

The preference is not saved to QSettings. The OS microphone indicator provides an additional capture indication. No microphone audio is recorded to disk by this sender. On macOS, synthetic diagnostic mode (MOONLIGHT_NATIVE_TEST_TONE=1) sends a generated 1 kHz tone instead and never opens an input device; its visible status identifies it as synthetic.

## Implemented scope

The tested native host set is NVIDIA GameStream 7.1.437.0, GeForce Experience 3.25.1.27 and native audio 4.37.0.0. Other host versions are unverified. The current client accepts the session's resolved private IPv4 destination (including loopback and the CGNAT range used by Tailscale); DNS names work through the existing session resolver. IPv6 microphone forwarding is not implemented; ordinary IPv6 streaming remains available without this microphone path. Source UDP port 50002 must be available.

Native negotiation can advertise microphone port zero before SETUP. Explicit opt-in allows the optional native SETUP exchange; a valid server port returned by SETUP is then required. Sunshine peers are excluded. A declined or unavailable microphone leaves normal streaming running, with a visible explanation. The source does not invent a default destination port.

Capture uses mono 48 kHz PCM, 240-sample (5 ms) Opus frames at fixed 32 kbps, native millisecond RTP timestamps, and periodic PINGs. Locally reported capture/queue/UDP errors stop sending and are surfaced by the session status poll. UDP send success alone is not proof that the host consumed audio. Queue shutdown/disposal is synchronous before sender resources and key bytes are released.

## Security tradeoffs

The sender requires the tested native encrypted-audio mode and does not silently send plaintext. That mode is legacy AES-128-CBC with PKCS7, not authenticated encryption. It follows NVIDIA's existing input-key/identifier and 16-bit RTP sequence construction. Its IV repeats after 65,536 packets (327.68 seconds at 5 ms), and the playback and microphone IV domains overlap. Do not describe this as modern authenticated transport or a newly negotiated Sunshine protocol. Use only a trusted private network or a separately secured tunnel; the private-address check by itself does not authenticate packets or protect an untrusted LAN.

The Windows integration uses privileged, version-specific modifications and must be evaluated separately from client consent. Its optional executable is a convenience route; the reviewed source and complete manual effects/rollback are part of the contribution. No proprietary NVIDIA executable or DLL is to be redistributed.

Prior AES-GCM advice on the separate Sunshine microphone proposal is useful design context, not a stated requirement for this native NVIDIA contribution. Maintainer preference on a future protocol and on executable/manual distribution should guide subsequent changes.

## Verification and limits

The isolated client built on macOS 15.6.1 arm64 with Qt 6.12.0 and Apple Clang 17. Sender tests compile the actual implementation with warnings as errors and send synthetic loopback audio only. They verify 300 encrypted packets, mono Opus decoding, a 1 kHz tone, pacing/PINGs, stop behavior, repeated starts/stops, malformed buffers, destination/key rejection, occupied source port and cleanup. See tests/native-mic/README.md. Production overlay composition has automated contract checks; no human UI walkthrough is claimed.

Historical end-to-end Windows evidence is separate from these new local tests. Reboot, full permanent uninstall/reinstall, actual macOS permission UI/device failure, and delivery of this newly integrated revision to Windows require explicit evidence before those cases can be called passed. The review package reports each boundary rather than treating a successful build as full acceptance.

## Windows and Linux desktop clients

Windows and Linux desktop builds use SDL capture of the default input device.
The client encodes 48 kHz mono input and sends through the shared common-c
transport. The same per-run opt-in, indicator, stop shortcut and teardown apply.
Allow desktop microphone access in the client operating system if needed.
The Steam Link build leaves forwarding disabled.

The portable adapter and shared transport passed 300 encrypted loopback frames
with SDL dummy capture on macOS, independent CBC/Opus decoding, and ten restart
cycles. The macOS release build also passed. Windows and Linux hardware and
live native host delivery remain separate acceptance checks.
