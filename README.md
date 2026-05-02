# SDR MSi Driver

Standalone SDR receiver for MSi2500/MSi001-based devices (SDRplay RSP1A, RSP2, Mirics dongles, and clones).

Real-time spectrum display, waterfall, and FM demodulation with audio output.

## Features

- Spectrum analyzer with FFT (2048-point Hann window)
- Scrolling waterfall display
- Wideband FM demodulation with de-emphasis
- IIR DC blocker (removes zero-IF center spike)
- Band presets (LF, MW, HF, FM)
- macOS and Linux support

## Dependencies

| Library | Purpose |
|---------|---------|
| libusb 1.0 | USB device communication |
| SDL2 | Window, OpenGL context, audio |
| FFTW3f | Single-precision FFT |
| OpenGL | Spectrum/waterfall rendering |

### macOS (Homebrew)

```bash
brew install libusb sdl2 fftw
```

### Linux (apt)

```bash
sudo apt install libusb-1.0-0-dev libsdl2-dev libfftw3-dev libgl-dev
```

## Build

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## Usage

```bash
./sdr-msi-driver [freq_mhz] [gain_db]
```

Examples:
```bash
./sdr-msi-driver              # 100 MHz, 40 dB
./sdr-msi-driver 98.5         # 98.5 MHz FM
./sdr-msi-driver 98.5 49      # 98.5 MHz, 49 dB (LNA on)
./sdr-msi-driver -L log.txt 100  # with logging
```

## Controls

| Key | Action |
|-----|--------|
| Up/Down | Tune +/- fine step |
| Left/Right | Tune +/- coarse step |
| Mouse wheel | Tune +/- fine step |
| Click spectrum | Tune to frequency |
| PgUp/PgDn | Gain +/- 5 dB |
| +/- | Volume |
| M | Mute/unmute |
| D | Toggle FM demod |
| 1-5 | Band presets (LF1/LF2/MW/HF/FM) |
| Q/Esc | Quit |

## Project Structure

```
sdr-msi-driver/
+-- driver/     libmsisdr USB driver (GPL-2.0)
+-- app/        Application (SDL2 + OpenGL + FFTW3)
+-- assets/     Icons
+-- doc/        Architecture documentation
+-- cmake/      CMake modules
```

## License

- Application code (`app/`): MIT
- Driver code (`driver/`): GPL-2.0 (derived from libmirisdr)
