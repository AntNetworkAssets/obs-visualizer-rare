# Desktop Audio Visualizer (OBS Native Plugin)

**What it is:** A native OBS source that visualizes **any OBS audio source** (e.g., *Desktop Audio*, *Mic/Aux*).  
No Browser Source hacks; it taps the selected source's PCM and renders GPU bars (Monstercat-style).

> This repository contains **source code** + CMake project files. Build it against your OBS Studio SDK.
> Shipping prebuilt binaries isn't possible here, but the project is ready-to-build.

## Features
- Select any OBS audio source via a dropdown (defaults to "Desktop Audio")
- 12–96 bars, mirror mode, gap spacing
- Primary/secondary color gradient (defaults: RareSRV blue + RareSix green)
- Lightweight filterbank DSP (no external FFT dependency)
- Gravity-like peak fall, smooth decay envelopes
- 1920×1080 default canvas (auto scales inside your scene)

## Build (Windows, Visual Studio)
1. Install OBS Studio (from obsproject.com). During install, make sure **"OBS Studio (with SDK headers)"** is included (or download the SDK).
2. Note your OBS path, e.g. `C:\Program Files\obs-studio`.
3. Open **x64 Native Tools Command Prompt for VS**.
4. Configure:
   ```bat
   set OBS_PATH=C:\Program Files\obs-studio
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   ```
5. Build:
   ```bat
   cmake --build build --config Release
   ```
6. Install (copies the module to your OBS folders):
   ```bat
   cmake --install build --config Release
   ```

## Build (macOS / Linux)
- Ensure you have libobs dev packages and CMake ≥ 3.16.
- Set `OBS_PATH` to your OBS prefix or adjust CMake prefix path.
- Usual flow:
  ```bash
  OBS_PATH=/Applications/OBS.app/Contents cmake -S . -B build
  cmake --build build --config Release
  cmake --install build --config Release
  ```

## Usage
1. Launch OBS → **Sources → + → Desktop Audio Visualizer (Source)**.
2. In properties:
   - **Audio Source**: choose *Desktop Audio* (or any audio-producing source)
   - **Bars**: 64 is a good start
   - **Mirror**: on for symmetrical look
   - **Gap/Glow/Gravity**: style to taste
   - **Colors**: RareSRV blue + RareSix green by default
3. Resize the source in your scene like any other source.

## Notes
- The DSP uses a logarithmic filterbank (60 Hz–16 kHz). It’s fast and avoids an external FFT dependency.
- For true Monstercat exactness you can swap in a KissFFT/FFTW implementation—wire the magnitudes into `meters[i].level`.
- If you don't see "Desktop Audio" in the dropdown, ensure your OBS scene actually has it as a source (or pick another).

## Roadmap
- Peak caps (floating toppers)
- Beat event output (trigger stingers/filters)
- Presets import/export
- GPU shader glow and particle layers

## License
MIT. Have fun.
