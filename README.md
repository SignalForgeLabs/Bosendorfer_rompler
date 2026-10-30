# Bosendorfer Imperial 290 (SignalForgeLabs)

A 97-key concert grand piano virtual instrument and acoustic microphone/hall processor built with **JUCE** and **TinySoundFont (TSF)**.

## Key Features
- **97-Key Sub-Contra Range (`C0` to `C8`)**: Extended sub-contra bass keys (`C0`–`G#0`) rendered with dark ebony/red-felted key styling and full acoustic bass extension.
- **Dual Embedded SoundFont Banks (`Standard` & `Brite`)**: Instant switching between `BOSENDRF.SF2` and `BOSENBRI.SF2`.
- **Per-Voice Acoustic Keyboard Split (`F3`–`A4` Equal-Power Crossover)**:
  - Routes Bass (Left) and Treble (Right) registers into independent 1024-tap zero-latency FIR Microphone Convolution channels with adjustable stereo crossfeed bleed.
  - **4 Selectable Studio Microphones** assignable to either Left (Bass) or Right (Treble) with interactive floating microphone graphics over the soundboard/harp:
    1. **Neumann KM 184** (`Mic_Neumann_KM184.wav`)
    2. **Neuman TLM 103** (`Mic_Neumann_TLM103.wav`)
    3. **Neumann U87** (`Mic_Neumann_U87.wav`)
    4. **RCA 44BX NEVE 1073** (`RCA 44BX NEVE.wav`)
- **Physical Damper & Keybed Mechanics (`AuthenticDamperClunk`)**:
  - **Pedal Down (`CC64 >= 64`)**: 97-damper felt lift airy whoosh + sympathetic open-string harmonic resonance wash.
  - **Pedal Release (`CC64 < 64`)**: Coupled dual-mode spruce soundboard thump (`72–88 Hz`), maple guide-rail knock (`400 Hz`), and felt impact transient (`2000 Hz`) injected prior to the microphone and concert hall convolution stages, completely independent of the key velocity curve.
- **3 Concert Hall Convolution Spaces**:
  - *Vienna Konzerthaus*, *Maple Stage 40ft*, and *Scoring Studio Hall*.

## Building Locally
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 8
```
