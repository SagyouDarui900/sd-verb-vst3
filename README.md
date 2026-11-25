# SD-Verb

[English](README.md) | [日本語](README.ja.md)

SD-Verb is a 64-bit VST3 and Standalone audio reverb plugin written in C++20 and JUCE 8. It implements short-decay Feedback Delay Network (FDN) and Schroeder reverb algorithms combined with a synchronized gate and built-in reverb ducker.

## Features

- **Dual Reverb Algorithms**
  - **DENSE**: 8-channel Hadamard Feedback Delay Network using Fast Walsh-Hadamard Transform (FWHT).
  - **METAL**: 4 parallel comb filters followed by 2 series Schroeder allpass filters per channel, with a +23 sample offset on the right channel at 44.1 kHz base rate.
- **Input Conditioning**
  - Linearly interpolated pre-delay (`0.0 - 100.0 ms`).
  - 2nd-order Butterworth high-pass (`20 - 1000 Hz`, Q = 0.7071) and low-pass (`1000 - 20000 Hz`, Q = 0.7071) input filters.
- **Shared Sidechain Gate & Ducking**
  - Peak envelope follower (Attack: `1.0 ms`, Release: `50.0 ms`) shared by both the gate and the ducker.
  - 3-state gate (`OPEN`, `HOLD`, `SHUT`) with `-3.0 dB` hysteresis and `1.5 ms` re-open smoothing.
  - Tempo-synced hold duration (`1/32`, `1/16`, `1/8`, `1/4`, `1/16T`, `1/8D`) via DAW `AudioPlayHead` (falls back to `120.0 BPM` when unavailable) or free time in milliseconds.
  - Fixed-threshold reverb ducker (`-36.0 dB` threshold, Attack: `2.0 ms`, Release: `80.0 ms`).
- **Real-Time Audio Safety**
  - Delay buffers are pre-allocated up to `192 kHz` during `prepareToPlay()`. No dynamic memory allocation occurs on the audio thread.
  - Continuous parameters use `5.0 ms` smoothing and fractional delay lines use linear interpolation.
- **User Interface**
  - Aspect-ratio-locked resizable window (`720 x 410` default, scalable from `540 x 308` to `1440 x 820`).
  - 4 color themes (`LIGHT` / `DARK` × `ORG` / `CYN`), saved with plugin state.
  - 160-point envelope scope (`8 ms` interval, `1.28 s` window), parameter-linked shape guide, and stereo width indicator.

---

## Terminology

- **FDN (Feedback Delay Network)**: A reverb topology that feeds delay-line outputs back into the inputs through a mixing matrix to generate high-density reverberation.
- **Schroeder Reverb**: A topology combining parallel comb filters and series allpass filters. In `METAL` mode, this produces audible metallic resonances on short decays.
- **Reverb Ducking**: Automatic attenuation of the wet reverb signal while the dry input signal is present, reducing masking of the dry transient.
- **Gated Reverb**: A reverb configuration that holds the wet tail for a set duration and then rapidly attenuates it using a threshold-triggered gate.

---

## Parameters

| Parameter ID | Name | Range | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| `algo` | Mode | `DENSE`, `METAL` | `DENSE` | Reverb algorithm selection (flushes delay buffers on switch) |
| `size` | Size | `10.0 - 100.0 %` | `35.0 %` | Room size scaling factor applied to internal delay lengths |
| `decay` | Decay | `0.05 - 5.00 s` | `0.80 s` | RT60 decay time |
| `pdly_sync` | Pre-Delay Sync | `OFF`, `ON` | `OFF` | Switches pre-delay between milliseconds and DAW tempo |
| `predelay` | Pre-Delay | `0.0 - 100.0 ms` | `10.0 ms` | Delay before input reaches the reverb core (`Pre-Delay Sync` OFF) |
| `pdly_rate` | Pre-Delay Rate | `1/64` .. `1/8` | `1/32` | Note value for pre-delay when `Pre-Delay Sync` is `ON` (clamped to `500 ms`) |
| `damping` | Damping | `0.0 - 100.0 %` | `40.0 %` | Feedback loop high-frequency damping (`20 kHz` to `1 kHz`) |
| `in_hpf` | Low Cut | `20 - 1000 Hz` | `120 Hz` | Input 2nd-order Butterworth high-pass filter cutoff |
| `in_lpf` | High Cut | `1000 - 20000 Hz` | `12000 Hz` | Input 2nd-order Butterworth low-pass filter cutoff |
| `gate_enabled` | Gate Active | `OFF`, `ON` | `OFF` | Enables or bypasses the reverb gate |
| `gate_thresh` | Gate Thresh | `-60.0 - 0.0 dB` | `-24.0 dB` | Gate open threshold (close threshold is `-3.0 dB` below) |
| `gate_sync` | Hold Sync | `OFF`, `ON` | `OFF` | Switches gate hold time between milliseconds and DAW tempo |
| `gate_hold` | Hold Time | `0.0 - 500.0 ms` | `80.0 ms` | Gate hold duration when `Hold Sync` is `OFF` |
| `gate_rate` | Hold Rate | `1/32` .. `1/8D` | `1/16` | Note value for gate hold when `Hold Sync` is `ON` |
| `gate_rel` | Gate Rel | `5.0 - 300.0 ms` | `30.0 ms` | Gate release time after hold expires |
| `duck_amount` | Ducking | `0.0 - 100.0 %` | `0.0 %` | Attenuation depth applied to wet signal during dry input |
| `lofi_enabled` | Lo-Fi Feedback | `OFF`, `ON` | `OFF` | Applies sample-rate reduction and bit quantization inside the feedback loop |
| `width` | Width | `0.0 - 200.0 %` | `100.0 %` | Mid/Side stereo width of the wet signal |
| `mix` | Mix | `0.0 - 100.0 %` | `30.0 %` | Dry/Wet balance |

---

## Installation (Pre-built Binary for Windows x64)

1. Download `SD-Verb-Windows-x64.zip` from the GitHub Releases page and extract it.
2. Copy the `SD-Verb.vst3` folder to `C:\Program Files\Common Files\VST3\`.
3. In REAPER, open **Options -> Preferences -> Plug-ins -> VST** and click **Re-scan** (or restart REAPER).

---

## Factory Presets

Select presets from the top-right dropdown menu in the plugin window or from the DAW's VST3 preset list.

| # | Preset Name | Mode | Target / Behavior |
| :---: | :--- | :---: | :--- |
| 0 | `Default (Init)` | `DENSE` | Initial state (`Decay: 0.80 s`, `Mix: 30 %`) |
| 1 | `Main Lead - Tight Ducking` | `DENSE` | Main melodies; applies `55 %` ducking and `220 Hz` low-cut |
| 2 | `Main Lead - 1/16 Gate` | `DENSE` | Staccato / rhythmic melodies; `1/16` synced gate cuts tail per step |
| 3 | `Harmony / Backing - Wide` | `DENSE` | Harmonies and backing layers; `165 %` width and `300 Hz - 8.5 kHz` band-limited tail |
| 4 | `Vocal - Short Room + Duck` | `DENSE` | Vocals; `20 ms` pre-delay preserves consonant onset with `45 %` ducking |
| 5 | `Drum Snare - Gated Metal` | `METAL` | Snare and claps; metallic tail cut by a `120 ms` hold / `35 ms` release gate |
| 6 | `Drum Kit - Tight Room` | `DENSE` | Drum bus and percussion; `22 %` size and `0.38 s` decay |
| 7 | `SE / Synth Hit - Metallic Tail` | `METAL` | One-shot hits and sound effects; `78 %` size and `2.40 s` decay |
| 8 | `Main Lead - 1/16 Sync Pre-Delay` | `DENSE` | Rhythmic lead; `1/16` tempo-synced pre-delay with `40 %` ducking |
| 9 | `SE / Retro - Lo-Fi Metal` | `METAL` | Retro game / low-bitrate texture; `LO-FI` feedback with `1/16T` gated tail |

---

## Usage Examples

- **Tight Ambience with Ducking (Vocals / Rhythmic Cuts)**
  - `Mode`: `DENSE`, `Size`: `35 %`, `Decay`: `0.60 s`, `Ducking`: `60 %`, `Mix`: `30 %`
  - Preserves dry transient clarity during rapid phrases while filling gaps between notes.
- **Tempo-Synced Gated Reverb (Percussion / Snare / Short Hits)**
  - `Mode`: `METAL`, `Size`: `50 %`, `Decay`: `1.50 s`, `Gate Active`: `ON`, `Gate Thresh`: `-24 dB`, `Hold Sync`: `ON`, `Hold Rate`: `1/16`, `Gate Rel`: `30 ms`
  - Cuts off the metallic tail precisely on the next sixteenth-note grid division.

## UI Controls

- **Double-click knob**: Resets the parameter to its default value.
- **`Shift + Double-click` / `Alt + Click` / Click value text**: Opens inline numeric input (accepts units such as `12k` or note values such as `1/16T`).
- **Right-click knob**: Opens the DAW host parameter context menu.
- **Top-right preset selector**: Switches between the 8 built-in presets and synchronizes with the host program list.

---

## Building from Source (Windows x64)

### Requirements

- Windows 10/11 (x64)
- CMake 3.22 or later
- Visual Studio 2022 (or Build Tools for Visual Studio 2022) with C++20 toolset
- JUCE 8 placed in the `JUCE/` directory at the repository root

### Build Steps

```powershell
# Clone JUCE 8 if not already present
git clone --depth 1 --branch 8.0.9 https://github.com/juce-framework/JUCE.git JUCE

# Configure and build Release targets (VST3 and Standalone)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -- /nr:false /m:1
```

> Note: Passing `/nr:false /m:1` to MSBuild disables multi-node reuse, which avoids build hangs observed in some Visual Studio 2022 Build Tools environments.

### Output Artifacts

- **VST3**: `build/SDVerb_artefacts/Release/VST3/SD-Verb.vst3`
- **Standalone**: `build/SDVerb_artefacts/Release/Standalone/SD-Verb.exe`

Copy `SD-Verb.vst3` to `C:\Program Files\Common Files\VST3\` to load it in a VST3-compatible DAW (such as REAPER).

---

## Project Structure

```text
.
├── .github/
│   └── workflows/
│       └── build.yml
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── README.md
├── README.ja.md
├── Resources/
│   └── Fonts/
│       ├── ChakraPetch-OFL.txt
│       ├── ChakraPetch-SemiBold.ttf
│       ├── EncodeSansExpanded-Black.ttf
│       └── OFL.txt
└── Source/
    ├── PluginEditor.cpp
    ├── PluginEditor.h
    ├── PluginProcessor.cpp
    ├── PluginProcessor.h
    ├── dsp/
    │   ├── FdnReverb.h
    │   └── GateDucker.h
    └── ui/
        ├── RetroLookAndFeel.h
        └── VerbVisualizer.h
```

## License

- Plugin source code: [MIT License](LICENSE)
- Embedded fonts (`Encode Sans Expanded`, `Chakra Petch`): [SIL Open Font License 1.1](Resources/Fonts/OFL.txt) / [ChakraPetch-OFL.txt](Resources/Fonts/ChakraPetch-OFL.txt)
- Built with [JUCE](https://juce.com/) (refer to `JUCE/LICENSE.md` for JUCE licensing terms when redistributing binaries).
