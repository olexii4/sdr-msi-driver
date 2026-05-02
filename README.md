# SDR MSi Driver

Android SDR driver for MSi2500/MSi001-based devices (SDRplay RSP1A, RSP2, Mirics dongles, and clones).

Provides rtl_tcp compatible streaming of raw I/Q samples over TCP from MSi.SDR hardware on Android.

## Supported Devices

| VID:PID | Device |
|---------|--------|
| 1df7:2500 | Mirics MSi2500 (RSP1C) |
| 1df7:3000 | SDRplay RSP1A |
| 1df7:3010 | SDRplay RSP2 |
| 2040:d300 | Hauppauge WinTV 133559 LF |
| 07ca:8591 | AverMedia A859 Pure DVBT |
| 04bb:0537 | IO-DATA GV-TV100 |
| 0511:0037 | Logitec LDT-1S310U/J |

## Features

- rtl_tcp compatible TCP server for SDR client apps
- IIR DC blocker (removes zero-IF center spike)
- 16-bit signed sample mode
- HW flavour auto-detection (RSP1A/RSP2 vs generic)
- USB disconnect crash fix
- USB transfer resilience (ISOC/BULK fallback)

## Build

Requires Android SDK and JDK 17.

```bash
ANDROID_HOME=~/Library/Android/sdk \
JAVA_HOME=/opt/homebrew/Cellar/openjdk@17/17.0.19/libexec/openjdk.jdk/Contents/Home \
./gradlew assembleDebug
```

## Install

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Project Structure

```
sdr-msi-driver/
+-- app/             Android application
+-- msisdr/          Driver + JNI library module
+-- sdrdrivertools/  Shared SDR interfaces
+-- assets/          Icons
+-- doc/             Documentation
```

## License

- Application code: MIT
- Driver code (libmsisdr): GPL-2.0
