# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Install

```bash
# Debug APK (requires JDK 17)
ANDROID_HOME=~/Library/Android/sdk \
JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
./gradlew assembleDebug

# Install to connected device
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Architecture

Three Gradle modules:

**`sdrdrivertools`** — shared Java interfaces and utilities used by both the app and the driver module. Key types:
- `SdrDevice` — abstract device (open/close/status callbacks)
- `SdrDeviceProvider` — factory that detects compatible USB devices
- `SdrTcpArguments` — parameters passed from the UI to the driver (gain, sample rate, frequency, TCP port)
- `UsbPermissionObtainer` — async USB permission helper

**`msisdr`** — Android library containing the driver JNI bridge and all native C code.
- Java: `MsiSdrDevice` (opens USB, calls JNI), `MsiSdrDeviceProvider` (USB VID/PID detection)
- JNI (`msisdr/src/main/cpp/src/`): `msisdrdevice.c` bridges Java ↔ C; `sdrtcp.c` runs an rtl_tcp-compatible TCP server; `workpool.c`/`queue.c`/`threading.c` manage the streaming worker pool
- Native library (`libmsisdr/`): self-contained fork of libmirisdr. Compiled as a single translation unit via `libmsisdr.c` which `#include`s all other `.c` files. Has Android-specific paths for USBDEVFS_BULK and USBDEVFS_CONTROL ioctls (Samsung SELinux workaround)

**`app`** — Android application. `DeviceOpenActivity` handles USB device selection and permission. `BinaryRunnerService` runs `MsiSdrDevice.openAsync()` as a foreground service. `StreamActivity` shows streaming status.

## Data Flow

```
USB device → MsiSdrDevice.openAsync() → JNI openAsync()
  → msisdr_open_fd() [libmsisdr]
  → sdrtcp_start()   [TCP server on requested port]
  → msisdr_read_sync_android() [USBDEVFS_BULK loop]
  → sdrtcp callback  [sends raw IQ to TCP clients]
```

SDR clients (SDR++, SDR#) connect to `localhost:<port>` using the rtl_tcp protocol.

## Known Hardware Quirks (see `doc/architecture.md` for details)

- **Samsung/Android USB**: `USBDEVFS_SUBMITURB` (async URBs) fails — interface claim belongs to the Android USB service. The driver uses synchronous `USBDEVFS_BULK` and `USBDEVFS_CONTROL` ioctls instead.
- **DC spike**: Zero-IF architecture causes a center-frequency spike. IIR DC blocker (`alpha=0.998`) applied in the JNI layer.
- **USB reset skipped on Android**: `msisdr_reset()` is a no-op on Android to avoid invalidating the device handle.
- **Gain range**: 0–102 dB total (LNA + Mixer + Baseband). Low gain (<20 dB) causes hardware self-oscillation.
- **HW flavour**: RSP1A/RSP2 auto-detected by PID (0x3000/0x3010) → `MSISDR_HW_SDRPLAY`. MSi2500 → `MSISDR_HW_DEFAULT`.

## Supported Devices

VID:PID `1df7:2500` (MSi2500), `1df7:3000` (RSP1A), `1df7:3010` (RSP2), `2040:d300`, `07ca:8591`, `04bb:0537`, `0511:0037`.
