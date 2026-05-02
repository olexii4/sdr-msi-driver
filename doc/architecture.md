# SDR MSi Driver - Architecture & Implementation Plan

## Overview

Standalone SDR receiver application for MSi2500/MSi001-based devices (SDRplay RSP1A, RSP2,
Mirics dongles, and clones). Provides real-time spectrum display, waterfall, and FM demodulation
with audio output.

## Lessons Learned (from msi-sdr-radio)

### Critical Fixes Incorporated

1. **macOS USB Reset Bug**: `libusb_reset_device()` returns `LIBUSB_ERROR_NOT_FOUND` on macOS,
   which invalidates the device handle. All subsequent USB operations fail silently or crash.
   **Fix**: Skip `libusb_reset_device` entirely on macOS via `#ifdef __APPLE__`.

2. **Disconnect Crash (SIGSEGV)**: When `LIBUSB_TRANSFER_NO_DEVICE` fires during async
   streaming, `p->dh` remains non-NULL but points to freed libusb internals.
   `msisdr_streaming_stop` then tries a control transfer on the dead handle -> crash.
   **Fix**: Set `p->dh = NULL` in the libusb callback on NO_DEVICE status.

3. **DC Spike at Center Frequency**: Zero-IF architecture (MSi001) creates LO self-mixing DC
   offset visible as a spike at center. Hardware DC calibration (PERIODIC2 mode) is insufficient.
   **Fix**: Software IIR DC blocker (`avg = 0.998 * avg + 0.002 * sample; corrected = sample - avg`)
   applied before FFT and FM demod. Creates a ~1 Hz notch at DC - negligible impact on wanted signals.

4. **HW Flavour Auto-Detection**: The driver auto-detects RSP1A/RSP2 by USB PID (0x3000/0x3010)
   and selects `MSISDR_HW_SDRPLAY` frequency plan. For generic MSi2500 dongles, `MSISDR_HW_DEFAULT`
   is used. The application should NOT override this auto-detection.

5. **USB Transfer Resilience**: ISOC transfers can fail on macOS - automatic fallback to BULK mode.
   Drain loop limited to 2 seconds (was 100s). Error recovery resubmits on stall/overflow/timeout.

### Gain Model

Total gain = LNA (0/24 dB) + Mixer (0/19 dB) + Baseband (0-59 dB) = 0-102 dB.
- Gain >= 43 dB: LNA on, Mixer on, Baseband adjusts remainder
- Gain 19-42 dB: LNA off, Mixer on, Baseband adjusts
- Gain 0-18 dB: LNA off, Mixer off, Baseband only

Application uses tenths-of-dB internally (0-1020), divides by 10 when calling driver.

## Project Structure

```
sdr-msi-driver/
|-- doc/                          Documentation
|   +-- architecture.md           This file
|-- assets/                       Icons and resources
|   +-- antenna.png               Application icon (512x512)
|-- driver/                       libmsisdr USB driver (GPL-2.0)
|   |-- include/
|   |   |-- msisdr.h             Public API
|   |   +-- msisdr_export.h      Symbol export macros
|   +-- src/
|       |-- libmsisdr.c          Single compilation unit (includes all .c below)
|       |-- structs.h             Core data structures
|       |-- constants.h           Timeouts, defaults
|       |-- async.h               Transfer buffer sizes
|       |-- async.c               Async USB transfer callbacks
|       |-- streaming.c           Start/stop data streaming
|       |-- gain.h                Gain register definitions
|       |-- gain.c                Gain decomposition & DC calibration
|       |-- hard.h                Sample rate limits
|       |-- hard.c                Sample rate & format registers
|       |-- soft.h                Frequency plan tables
|       |-- soft.c                PLL synthesis & frequency tuning
|       |-- devices.c             USB VID/PID device table
|       |-- reg.c                 Register write via USB control
|       |-- adc.c                 ADC initialization
|       |-- sync.c                Synchronous bulk read
|       +-- convert/              Sample format converters
|           |-- base.c            Include aggregator
|           |-- 252_s16.c         14-bit -> S16
|           |-- 336_s16.c         12-bit -> S16
|           |-- 384_s16.c         10+2-bit -> S16
|           |-- 504_s16.c         8-bit -> S16
|           +-- 504_s8.c          8-bit passthrough
|-- app/                          Application layer
|   |-- main.c                    Entry point, main loop
|   |-- device.h / device.c       Device management, IQ ring buffer, async streaming
|   |-- dsp.h / dsp.c             FFT spectrum, DC blocker, FM demodulation
|   |-- audio.h / audio.c         SDL2 audio output with ring buffer
|   +-- ui.h / ui.c               OpenGL rendering, controls, device selector
|-- cmake/
|   +-- FindLibUSB.cmake          CMake module for libusb-1.0
|-- CMakeLists.txt                Build configuration
|-- LICENSE                       MIT (app) + GPL-2.0 (driver)
+-- README.md                     Build instructions
```

## Dependencies

| Library   | Purpose                    | License |
|-----------|----------------------------|---------|
| libusb    | USB device communication   | LGPL    |
| SDL2      | Window, OpenGL, audio      | zlib    |
| FFTW3f    | FFT for spectrum analysis  | GPL     |
| OpenGL    | Spectrum/waterfall render  | Khronos |
| pthreads  | Async streaming thread     | POSIX   |

## Architecture

### Threading Model

```
Main Thread                    USB Thread (detached)
+-----------+                  +-------------------+
| SDL Event |                  | msisdr_read_async|
| Loop      |<--- IQ Ring ----|   async_callback   |
| DSP/FFT   |    Buffer       |   libusb events    |
| Audio     |                  +-------------------+
| UI Render |
+-----------+
```

- **Main thread**: Event handling, DSP processing, audio output, UI rendering at 60 FPS
- **USB thread**: Detached pthread running `msisdr_read_async`, feeds IQ data into ring buffer
- **Communication**: Lock-free ring buffer (4M samples), `device_error` flag for disconnect

### Data Flow

```
USB -> async_callback -> ring_buffer -> device_read_iq -> dsp_process_iq -> spectrum/waterfall
                                                      |-> dsp_fm_demod -> audio_push -> SDL audio callback
```

### DC Blocker (IIR High-Pass)

Applied at two points in the DSP chain:
1. Before FFT input (removes center spike from spectrum display)
2. Before FM discriminator (prevents DC offset from corrupting demodulation)

```c
dc_avg_i = 0.998 * dc_avg_i + 0.002 * sample_i;
dc_avg_q = 0.998 * dc_avg_q + 0.002 * sample_q;
corrected_i = sample_i - dc_avg_i;
corrected_q = sample_q - dc_avg_q;
```

### Band Presets

| Key | Band | Center    | Bandwidth | Range           |
|-----|------|-----------|-----------|-----------------|
| 1   | LF1  | 65 kHz    | 200 kHz   | 30-100 kHz      |
| 2   | LF2  | 200 kHz   | 300 kHz   | 100-300 kHz     |
| 3   | MW   | 1.65 MHz  | 1.536 MHz | 300 kHz - 3 MHz |
| 4   | HF   | 16.5 MHz  | 5 MHz     | 3-30 MHz        |
| 5   | FM   | 100 MHz   | 8 MHz     | 88-108 MHz      |

### Keyboard Controls

| Key         | Action                |
|-------------|-----------------------|
| Up/Down     | Tune +/- fine step    |
| Left/Right  | Tune +/- coarse step  |
| Mouse wheel | Tune +/- fine step    |
| Click       | Tune to frequency     |
| PgUp/PgDn   | Gain +/- 5 dB         |
| +/-         | Volume +/- 10%        |
| M           | Mute/unmute           |
| D           | Toggle FM demod       |
| 1-5         | Band presets          |
| Q/Esc       | Quit                  |

## Build Instructions

```bash
cd sdr-msi-driver/build
cmake ..
make -j$(nproc)
./sdr-msi-driver [freq_mhz] [gain_db]
```

Optional: `-L logfile` for logging to file.
