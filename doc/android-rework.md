# Plan: Rework sdr-msi-driver into Android SDR Driver App

## Context

The `sdr-msi-driver` project at `/Users/oleksiiorel/workspace/olexii4/sdr-msi-driver` is currently a **desktop** SDR app (SDL2/OpenGL/FFTW3). The user wants to rework it into a standalone **Android** SDR driver app modeled after `rtl_tcp_andro-` — an Android app that exposes SDR devices over TCP (rtl_tcp protocol). The desktop app layer (SDL, OpenGL, FFTW) will be removed and replaced with Android modules (Gradle, JNI, Java, TCP server).

## What Changes

### Remove (desktop-specific)
- `app/` — SDL2/OpenGL UI, desktop DSP, audio (main.c, device.c, dsp.c, audio.c, ui.c, headers)
- `CMakeLists.txt` — root CMake (replaced by Gradle)
- `cmake/FindLibUSB.cmake` — not needed on Android (bundled libusb)
- `build/` — desktop build output

### Keep
- `driver/` — libmsisdr source (reorganized into Android module)
- `assets/antenna.png` — becomes app icon
- `doc/` — documentation
- `LICENSE`, `README.md` (updated), `.gitignore` (updated)

### Create (Android structure)

```
sdr-msi-driver/
├── build.gradle                    # Root Gradle config (AGP 8.5.2)
├── settings.gradle                 # Includes :app, :msisdr, :sdrdrivertools
├── gradle.properties               # JVM + AndroidX settings
├── gradlew, gradlew.bat            # Gradle wrapper
├── gradle/wrapper/                  # Gradle wrapper JAR + properties
├── app/                            # Android application module
│   ├── build.gradle
│   ├── src/main/
│   │   ├── AndroidManifest.xml
│   │   ├── java/com/sdrtouch/msisdr/
│   │   │   ├── StreamActivity.java
│   │   │   └── DeviceProviderRegistry.java
│   │   └── res/
│   │       ├── xml/device_filter.xml
│   │       └── ...
├── msisdr/                         # Driver + JNI library module
│   ├── build.gradle
│   ├── src/main/
│   │   ├── AndroidManifest.xml
│   │   ├── cpp/
│   │   │   ├── CMakeLists.txt      # Top-level: msiSdrAndroid shared lib
│   │   │   ├── libmsisdr/          # ← moved from driver/
│   │   │   │   ├── CMakeLists.txt  # Static lib
│   │   │   │   ├── include/msisdr.h, msisdr_export.h
│   │   │   │   ├── src/            # All driver .c/.h files
│   │   │   │   └── libusb/         # ← copied from rtl_tcp_andro-
│   │   │   └── src/                # JNI bridge code
│   │   │       ├── msisdrdevice.c  # ← copied from rtl_tcp_andro-
│   │   │       ├── sdrtcp.c/h
│   │   │       ├── tcp_commands.h
│   │   │       ├── common.c/h
│   │   │       ├── SdrException.h
│   │   │       ├── threading.c/h
│   │   │       ├── extbuffer.c/h
│   │   │       ├── queue.c/h
│   │   │       └── workpool.c/h
│   │   ├── java/com/sdrtouch/msisdr/driver/
│   │   │   ├── MsiSdrDevice.java
│   │   │   └── MsiSdrDeviceProvider.java
│   │   └── res/xml/msi_sdr_device_filter.xml
├── sdrdrivertools/                 # ← copied from rtl_tcp_andro-
│   ├── build.gradle
│   └── src/main/java/...           # SdrDevice, SdrDeviceProvider, etc.
├── doc/
│   ├── architecture.md             # Updated
│   └── android-rework.md           # This plan
├── assets/antenna.png
├── LICENSE
├── README.md                       # Updated for Android
└── .gitignore                      # Updated for Gradle
```

## Steps

### Step 1: Write plan document
Create `doc/android-rework.md` with this plan.

### Step 2: Clean desktop files
Remove `app/`, `CMakeLists.txt`, `cmake/`, `build/`.

### Step 3: Copy Gradle wrapper from rtl_tcp_andro-
Copy `gradlew`, `gradlew.bat`, `gradle/` directory.

### Step 4: Create root Gradle files
- `build.gradle` — AGP 8.5.2 plugin declaration
- `settings.gradle` — include ':app', ':msisdr', ':sdrdrivertools'
- `gradle.properties` — JVM args, AndroidX, nonTransitiveRClass

### Step 5: Copy sdrdrivertools module
Copy entire `sdrdrivertools/` from rtl_tcp_andro- (SdrDevice, SdrDeviceProvider, SdrException, utilities).

### Step 6: Create msisdr module structure
- Move `driver/` contents → `msisdr/src/main/cpp/libmsisdr/`
- Copy `libusb/` from rtl_tcp_andro- → `msisdr/src/main/cpp/libmsisdr/libusb/`
- Copy JNI bridge code from rtl_tcp_andro- → `msisdr/src/main/cpp/src/`
  (msisdrdevice.c, sdrtcp.c/h, tcp_commands.h, common.c/h, SdrException.h, threading.c/h, extbuffer.c/h, queue.c/h, workpool.c/h)
- Create CMakeLists.txt files (top-level + libmsisdr)
- Copy Java files (MsiSdrDevice.java, MsiSdrDeviceProvider.java)
- Create `msisdr/build.gradle`, AndroidManifest.xml, device filter XML
- Remove old `driver/` directory

### Step 7: Create app module
- Copy app structure from rtl_tcp_andro- (StreamActivity, DeviceProviderRegistry)
- Simplify: only MsiSdr provider (remove RTL-SDR, HackRF references)
- Create `app/build.gradle` with msisdr + sdrdrivertools dependencies
- Create AndroidManifest.xml, device filter, resources

### Step 8: Update project files
- Update `.gitignore` for Gradle/Android
- Update `README.md` for Android build instructions
- Update `doc/architecture.md`

### Step 9: Build APK
```bash
ANDROID_HOME=~/Library/Android/sdk \
JAVA_HOME=/opt/homebrew/Cellar/openjdk@17/17.0.19/libexec/openjdk.jdk/Contents/Home \
./gradlew assembleDebug
```

### Step 10: Install to phone
```bash
~/Library/Android/sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Source Files (from rtl_tcp_andro-)

All files copied from `/Users/oleksiiorel/workspace/olexii4/rtl_tcp_andro-`:

| Source | Destination |
|--------|------------|
| `gradlew`, `gradlew.bat`, `gradle/` | Root |
| `sdrdrivertools/` (entire module) | `sdrdrivertools/` |
| `msisdr/src/main/cpp/libmsisdr/libusb/` | `msisdr/src/main/cpp/libmsisdr/libusb/` |
| `msisdr/src/main/cpp/src/*.c` + `*.h` | `msisdr/src/main/cpp/src/` |
| `msisdr/src/main/java/` | `msisdr/src/main/java/` |
| `msisdr/src/main/res/` | `msisdr/src/main/res/` |
| `app/src/main/java/.../StreamActivity.java` | Adapted for app/ |
| `app/src/main/res/` | Adapted for app/ |

## Verification
1. `./gradlew assembleDebug` builds clean
2. APK installs on phone
3. App lists MSi.SDR devices when connected via USB OTG
