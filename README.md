# Pitchly — VST3 auto-tune for FL Studio (Windows x64 / ARM64)

Retune speed · key & scale snapping · humanize · formant · mix, with a live pitch graph.
UI follows the Figma design (900x580, dark, teal accent).

## Which build do I need?  (important on Windows ARM)

FL Studio for Windows currently runs as an **x64 app under Windows' emulation**, even on ARM PCs.
An x64 host can only load **x64** plugins, so for FL Studio use the **x64** build.
The **ARM64** build is for hosts that are natively ARM64 (Reaper, Bitwig, Cubase...) and for
FL Studio if/when Image-Line ships a native ARM64 version. Building both is cheap, so do both.

## Get the plugin (pick one)

### A) GitHub Actions — no tools to install
1. Create a GitHub repo, upload this folder, push. 
2. Open the **Actions** tab → "Build Pitchly VST3" → wait ~10 min.
3. Download artifacts `Pitchly-VST3-x64` and `Pitchly-VST3-ARM64` (each contains `Pitchly.vst3`).

### B) Build locally (Visual Studio 2022 or newer, "Desktop development with C++")
    build_windows.bat x64
    build_windows.bat ARM64
First build downloads JUCE (needs git + internet). Output: `build-<arch>\Pitchly_artefacts\Release\VST3\Pitchly.vst3`

## Install
Copy the **whole `Pitchly.vst3` folder** to `C:\Program Files\Common Files\VST3\`
(or run `install_windows.bat x64` as Administrator).

## FL Studio
1. Options → Manage plugins → **Find installed plugins** (Pitchly is under *Effects*; star it in the list).
2. Put Pitchly on a mixer insert (it's an effect, put it on the vocal track).
3. Latency is reported to FL (~38 ms), so Plugin Delay Compensation lines things up automatically.

## Notes
* Mono voice works best. Choose key/scale (or click individual notes) and set Retune Speed: 0 ms = hard "T-Pain" snap, 30–90 ms = natural.
* The engine is tested headless: `g++ -O2 -std=c++17 tests/engine_test.cpp -o t && ./t`
* Built with JUCE (AGPLv3 / commercial). Fine for personal use; read JUCE's licence before selling it.
