# Build instructions: TETRAX, a function generator and a modal percussion voice for VCV Rack

**Audience:** an Opus model implementing this plugin from scratch.
**Owner:** Hardy. Fluent in VCV Rack, does not read C++. Build the whole plugin in one go, test it yourself, hand over the finished result. No milestone check-ins.

**What it is:** a VCV Rack 2 plugin, brand and slug `Tetrax`, with three modules whose behaviour follows two Intellijel modules as described in their public manuals:

| Our module | Inspired by | One line |
|---|---|---|
| **TETRA** Four-channel function generator | Quadrax (firmware 1.4.5) | Four linkable channels, each an AD, AHR, cycling, burst or LFO function, with a ring link and visible per-channel CV |
| **TETRA X** End-of-stage expander | Qx | End-of-rise and end-of-fall gates for each TETRA channel |
| **PLINK** Modal percussion voice | Plonk (firmware 1.16) | A mallet and noise exciter striking one of six modal objects, two voices, X/Y/MOD assignment, preset step, randomize and morph |

Research and sources: `docs/RESEARCH.md` (copy of `/mnt/project-files/quadrax-plonk-vcv/01-research-and-proposal.md`).

**Decisions used here (Fable's defaults, 2026-11-15; Hardy may override any before the build):**

| Question | Answer used here |
|---|---|
| Faithfulness | Behavioural, from the official manuals. No schematics or DSP are published. PLINK's sound engine is our own modal synthesis, not AAS's models. Where a manual leaves out a number, this spec gives an estimate marked **[est]**, kept in one constants file and logged in the accuracy ledger (§10) |
| Names | Plugin `Tetrax`; modules `TETRA`, `TETRAX`, `PLINK`. "Intellijel", "Quadrax", "Qx", "Plonk", "Applied Acoustics", "AAS" never appear in plugin, module, panel, tag or preset names, and no Intellijel art is used. The manual may say "inspired by the Intellijel Quadrax / Plonk" once per module |
| TETRA CV | Per-channel CV jacks for RISE, FALL, SHAPE and LEVEL, each with an attenuverter trimpot. The hardware's hidden four-input matrix is dropped (Rack users can't discover it, and patching one CV to several jacks does the same job) |
| Qx | A separate expander module, TETRA X, that must sit directly to the right of TETRA |
| PLINK controls | Every exciter and object parameter is a knob on the panel. A small display shows preset, X/Y/MOD destinations and values. Hardware button-hold combos become labelled buttons and right-click menu items |
| PLINK voices | 2 voices (hardware). Right-click option: 1, 2, 4 or 8 voices, with polyphonic TRIG/PITCH/VEL routing (§5.12) |
| PLINK presets | 128 internal slots stored in the patch, 32 factory sounds of our own (§5.14) |
| Look | Clean flat panels (§3). Text labels on every control and jack, no symbols, no silver |
| Repo | `hardyfinn/Tetrax` (Hardy renamed the plugin to Tetrax and TOCK to PLINK on 2026-11-15) (created by Hardy 2026-11-15), work on `main` |
| Builds | Apple silicon Mac only (Hardy's standing preference) |

---

## 0. Ground rules

1. **Behaviour first.** Implement the behaviour described here exactly. Where a number is marked [est], use it as given and log it.
2. **All tunable constants in one place:** `src/dsp/Constants.hpp`, one named `constexpr` per [est] value, each with a comment naming the section and saying "estimate".
3. **DSP separate from Rack.** Engines live in `src/dsp/` as plain C++17 with no Rack includes, unit-tested on the host. `src/modules/*.cpp` are thin wrappers.
4. **No allocation, locks, file I/O or logging in `process()`.** PLINK's mode tables and preset bank are allocated once.
5. **Every param has a tooltip with a unit, every port a name.** Tooltip names spell out the printed label. PLINK knobs show the hardware's 0 to 127 (or −64 to +63) value in the tooltip, plus the physical value in brackets (e.g. `Noise decay: 64 (180 ms)`).
6. **Stability.** Clamp every state variable; end each `process()` with an `isfinite` guard that resets bad state to 0. Outputs stay within ±10 V (PLINK's limiter, §5.10, handles audio).
7. **Ambiguity:** pick the reading closest to the manual, write it in `docs/DECISIONS.md`, list it in the hand-over.
8. **CPU at 48 kHz:** TETRA under 0.5% of one core, TETRA X under 0.05%, PLINK under 2% with 2 voices and 64 partials (SIMD `float_4` across partials).
9. **Persistence:** every button state, menu choice and PLINK's whole preset bank saves to the patch via `dataToJson`/`dataFromJson`.
10. Licence GPL-3.0-or-later.

---

## 1. Setup

- **VCV Rack 2.x**, newest Rack SDK. Read https://vcvrack.com/manual/PluginDevelopmentTutorial, https://vcvrack.com/manual/Panel, https://vcvrack.com/manual/DSP, https://vcvrack.com/manual/Polyphony and https://vcvrack.com/manual/Expanders (TETRA X) first.
- `make`, `make dist`, `make install` with `RACK_DIR` set.
- Unit tests with **doctest** in `tests/`; `make test` builds `tests/*.cpp` with `src/dsp/*.cpp` as a host executable and runs it.
- `.github/workflows/build.yml`: reuse the working workflow from `hardyfinn/silverbox` or `hardyfinn/tudor` (both build Mac arm64 green). Build macOS arm64, run `make test` on the runner, upload the `.vcvplugin` artifact on every push, attach it to a release on tags `v*`. The Rack SDK download is blocked from the cloud container, so Rack builds happen only on Actions; `make test` runs locally.

```
plugin.json  Makefile  README.md  LICENSE
docs/  BUILD-INSTRUCTIONS.md (this file)  RESEARCH.md  DECISIONS.md  MANUAL.md  Tetrax-Manual.pdf
res/   TETRA.svg  TETRAX.svg  PLINK.svg  components/
scripts/ make_panels.py  make_mode_tables.py  make_manual_pdf.sh  render_plink_demo.cpp (optional)
src/
  plugin.hpp  plugin.cpp
  dsp/  Constants.hpp  Curves.hpp  TetraChannel.hpp  TetraEngine.hpp
        ModeTables.hpp (generated)  ModalBank.hpp  Exciter.hpp  PlinkVoice.hpp  PlinkParams.hpp
        PlinkEngine.hpp  Presets.hpp  FactoryPresets.hpp  (+ .cpp as needed)
  modules/  TETRA.cpp  TETRAX.cpp  PLINK.cpp
  widgets/  TetraxComponents.hpp  PlinkDisplay.hpp
tests/
```

`plugin.json`: slug `Tetrax`, name `Tetrax`, brand `Tetrax`, version `2.0.0`.

| Slug | Name shown in browser | HP | Tags |
|---|---|---|---|
| `TETRA` | `TETRA Four-channel function generator` | 24 | Envelope generator, Function generator, LFO, Clock modulator, Polyphonic |
| `TETRAX` | `TETRA X End-of-stage expander` | 6 | Expander, Logic, Utility |
| `PLINK` | `PLINK Modal percussion voice` | 32 | Drum, Physical modeling, Synth voice, Polyphonic |

Descriptions contain "4 channel", "envelope", "burst", "LFO", "random", "function" (TETRA), "end of rise", "end of fall", "gates" (TETRAX), and "physical model", "modal", "percussion", "mallet", "marimba", "drum", "plate" (PLINK) so search finds them. Rack accepts only its own tag list; use the closest valid tags.

---

## 2. Signal conventions

| Signal | Level |
|---|---|
| Gates, triggers, clocks in | Schmitt: high above 1.5 V, low below 0.5 V [est; Plonk's MOD gate threshold is 1.5 V, Quadrax gives none]. Minimum detected width: one sample (hardware: 1 ms) |
| TETRA outputs, AD/AHR/CYCLE/BURST | Unipolar 0 V to MAX (5 V or 10 V per channel), times LEVEL |
| TETRA outputs, LFO and LFV | Bipolar ±5 V, times LEVEL (the hardware fixes LFO at ±5 V) |
| TETRA X gates | 0 V / 10 V [est; manual gives only "high" and 0 V] |
| CV inputs | ±10 V accepted, clipped to ±5 V before use (Quadrax manual). 5 V = full knob travel unless stated |
| PLINK PITCH | 1 V/oct, ±5 V, summed with the knob |
| PLINK X, Y, MOD, DECAY CV | ±5 V |
| PLINK VEL | 0 to 5 V, normalled to 5 V |
| PLINK OUT | Audio, nominal ±5 V peak, hard ceiling ±10 V after the limiter |

---

## 3. Panels

Hardy finds silver panels look dirty. Make these clean:
- **Flat matte off-white** background `#F2F1ED`, **near-black** text and lines `#1B1B1B`, one accent colour per module used only for section headings and the thin rule under them: TETRA deep teal `#1F6F78`, TETRA X the same teal, PLINK warm orange `#D9622B`.
- No gradients, textures, brushed metal, drop shadows or screws drawn into the art. Use Rack's standard black screws.
- One sans-serif typeface converted to paths (e.g. Inter or DejaVu Sans), label size 7 pt, section headings 8 pt bold capitals, module name 12 pt bold at the top, plugin name `TETRAX` small at the bottom.
- Knobs: Rack's plain dark knobs (`RoundBlackKnob`, `RoundSmallBlackKnob`, `Trimpot`). Jacks: `PJ301MPort`. Output jacks sit on a small dark rounded rectangle labelled OUT so they read as outputs.
- Every control and jack has a printed text label. No icons or symbols.
- Generate SVGs with `scripts/make_panels.py` (text converted to paths), as in the Silverbox repo. Keep layout coordinates in the script and export a header of positions the module code reads, so panel and widgets never drift apart.

---

## 4. TETRA, Four-channel function generator (24 HP)

### 4.1 Overview

Four identical channels. Each is one function: AD, AHR, CYCLE, BURST or LFO, each with an ALTERNATE variant. Channels can be fired by the channel before them (ring: 4 → 1). Shared engine code: `TetraChannel` (one channel) and `TetraEngine` (links, ring, clock sync).

### 4.2 Layout

Four vertical channel columns, 6 HP each, numbered 1 to 4 at the top. Top to bottom in each column:

| Control / jack | Type | Notes |
|---|---|---|
| Channel LED | RGB | Brightness = |output| / MAX; green positive, red negative (hardware) |
| RISE | Large knob | Label changes meaning per mode (§4.4 table); tooltip shows the mode's name for it, e.g. "Rate" in LFO |
| FALL | Large knob | As above |
| SHAPE | Medium knob | As above; centre detent at noon |
| LEVEL | Small knob | 0 to 100%, default 100%. Replaces the hardware's 20% attenuation steps (continuous is better in Rack; right-click "Stepped level" restores 20% steps) |
| MODE | Button with RGB LED | Cycles AD (white, hardware: unlit) → AHR (red) → CYCLE (green) → BURST (yellow) → LFO (magenta) → AD |
| ALT | Latching button with LED | ALTERNATE variant of the current mode (hardware: long-press MODE). LED lit = ALT. Remembered per mode |
| LINK | Button with RGB LED | Cycles OFF (unlit) → TRIG (red) → EOR (green) → EOF (yellow) → OFF. Source is the channel to the left; channel 1's source is channel 4 |
| 10V | Latching button with LED | Lit = 10 V MAX, unlit = 5 V MAX. Default lit (10 V, firmware 1.1 default). Ignored in LFO (always ±5 V) |
| FIRE | Momentary button | Manual trigger/gate (hardware Utility mode). Link rules apply |
| RISE CV, FALL CV, SHAPE CV, LEVEL CV | 4 jacks, each with a Trimpot attenuverter beside it | Trimpot −1 to +1, default +1 (see note) |
| TRIG | Input jack | Gate/trigger, or clock in LFO |
| OUT | Output jack | |

Note on CV trimpots: default each trimpot to **+1** (fully on), so a patched cable works at once, matching the hardware's default full-strength assignment. 5 V × 1.0 = full knob travel.

Bottom row across the panel: **POLY OUT** (4 channels: OUT 1 to 4) and **POLY TRIG** input (channel c of the cable is OR'd into TRIG c; a mono cable goes to channel 1 only).

Right-click menu: Burst retrigger (OFF default / ON, §4.7), Stepped level, Reset channel (each), Reset all, LED colour palette (Standard / Colour-blind, hardware CVD palette: positive blue, negative orange).

### 4.3 Shared laws (`Curves.hpp`)

**Time law (RISE, FALL in envelope modes).** Knob position p in [0, 1] plus CV (5 V = 1.0), total clamped to [−0.15, 1.15] so CV extends the range as the manual says. Piecewise exponential through the manual's three points:
- p ≤ 0.5: T = 0.05 ms × 1000^(2p) (0.05 ms at 0, 50 ms at noon)
- p > 0.5: T = 50 ms × 400^(2(p − 0.5)) (20 s at full CW)
- Minimum effective time 1 sample. [est law, manual endpoints]

Apply a 2% dead zone at each knob end (firmware 1.2.1 reduced the 5% one) [est].

**Rate law (BURST rate, LFO rate).** f = 0.05 Hz × (3333 / 0.05)^p, same clamped p, so 0.05 Hz at CCW, ~13 Hz at noon, 3.33 kHz at CW [est law, manual endpoints]. Cap at 0.45 × sample rate.

**Curve law (SHAPE in envelope modes).** s = 2 × SHAPE − 1 in [−1, 1] after CV. For a normalised stage progress x in [0, 1]:
- s = 0: y = x (linear)
- s > 0 (exponential, slow start): y = (e^(k x) − 1) / (e^k − 1), k = 10 s [est]
- s < 0 (logarithmic, fast start): y = 1 − curve(1 − x, −s)
Use the same curve for the rise (output = y × peak) and the fall (output = peak × (1 − y)). Firmware 1.1 made the curves "more extreme"; k up to 10 gives that.

**ALTERNATE envelope curves.** s > 0: exponential rise and logarithmic fall ("spiky"). s < 0: logarithmic rise and exponential fall ("bell-like"). Implement by using curve(x, +|s|) on one stage and curve(x, −|s|) on the other as listed.

**Output.** out = shape value (0..1 unipolar or −1..1 bipolar) × MAX × LEVEL, where LEVEL = knob + LEVEL CV × trimpot / 5, clamped 0..1 (CV can't invert).

### 4.4 Knob meaning per mode

| Mode | RISE | FALL | SHAPE |
|---|---|---|---|
| AD / AHR / CYCLE | Rise time | Fall time | Curve |
| BURST | Pulse rate | Burst length | Pulse waveform + envelope tilt |
| LFO | Rate (or clock ratio) | Morph | Wave shape |
| LFV (LFO + ALT) | Rate (or clock ratio) | Variance | Slew |

### 4.5 AD, AHR, CYCLE

- **AD:** a rising edge starts the rise from the **current level** (no jump to 0 [est]); after the rise the fall runs to 0. Gate length is ignored. A new trigger during rise or fall restarts the rise from the current level.
- **AHR:** gate rising edge starts the rise; at the peak it holds while the gate is high; gate falling edge starts the fall from wherever it is (so a gate shorter than the rise falls early). A new rising edge restarts the rise from the current level.
- **CYCLE:** free-running rise then fall, repeating; period = rise time + fall time. A rising edge on TRIG (or link) restarts at the start of the rise from the current level [est]. Gate length ignored.
- Times are read continuously, so turning RISE during a long rise changes its speed at once (progress x is kept, the increment changes).

### 4.6 Stage events (feed links and TETRA X)

Each channel exposes per-sample booleans **EOR** and **EOF** and edge events, defined exactly as the manual (§7 lists them). Links use the **rising edge** of the EOR or EOF gate as the trigger. A TRIG link passes the previous channel's TRIG **gate** (level, not just the edge), so a linked AHR follows the original gate length (firmware 1.2.1).

### 4.7 BURST

- **Rate** f from the rate law. **Length** L from the time law, with a minimum of 0.3 ms (manual).
- Number of pulses N = max(1, floor(L × f)). Pulses are evenly spaced: pulse period P = L / N, so the burst always fills the length exactly [est reading of "evenly spaced within the burst length"].
- Each pulse: within its period, phase φ in [0, 1). Waveform w(φ) in [0, 1]:
  - STANDARD: SHAPE within 10% of either end → square (1 for φ < 0.5, else 0); otherwise raised sine (0.5 − 0.5 cos 2πφ). Firmware 1.2.0 removed the crossfade, so it's a hard switch [est threshold].
  - ALT: morph by SHAPE: full CCW sawtooth (1 − φ), noon triangle, full CW ramp (φ); interpolate the triangle's peak position from 0 to 1 with SHAPE.
- **Envelope tilt** across the burst, position u = t / L: a triangle envelope with its peak at u_peak = SHAPE (0 at CCW: instant attack then decreasing; 1 at CW: rising then instant cut; 0.5 at noon). e(u) = u / u_peak before the peak, (1 − u) / (1 − u_peak) after (guard the 0 and 1 cases). In ALT mode the envelope is flat (e = 1) [est; manual gives the ALT waveform only].
- Output = w × e × MAX × LEVEL.
- **Retrigger:** menu Burst retrigger OFF (default): triggers are ignored until the burst completes. ON: a trigger restarts the burst envelope (u = 0) while the pulse phase keeps running; the pulse phase only resets when a burst completes (manual).
- LED green, brightness = output level.

### 4.8 LFO (STANDARD)

- Free-running when TRIG is unpatched and no link is active: rate from the rate law.
- **Clocked** when TRIG is patched (or the channel is linked): measure the clock period from the last two rising edges; RISE picks a ratio from this table, noon = 1:1 [est table; manual gives only the ends]:
  `1/64, 1/32, 1/16, 1/8, 1/6, 1/4, 1/3, 1/2, 1, 2, 3, 4, 6, 8, 16, 32, 64` (17 steps across the knob, CV moves through it too).
  Ratios ≥ 1 reset phase on every clock edge; ratios < 1 reset phase every n-th edge (n = 1/ratio). If no clock for 4 periods, keep running at the last rate.
- **SHAPE** sweeps five base shapes evenly across the knob, crossfading between neighbours [est, the manual's chart is an image]: **sine → triangle → ramp up → square → ramp down**.
- **FALL (MORPH)** gives a variant of the current shape [est]: phase skew with skew amount m = FALL in [0, 1], noon (0.5) = undistorted. Implement as a piecewise-linear phase warp: φ' = φ × 0.5 / m for φ < m, else 0.5 + (φ − m) × 0.5 / (1 − m), with m clamped to [0.02, 0.98]. On the square this is pulse width; on the triangle it moves the peak; on the ramps it bends them.
- Output ±5 V × LEVEL. Phase 0 = start of the sine's rise from 0 V (so a reset gives a predictable start).
- **Linked LFOs:** if a channel is linked (any link type) and in LFO mode, the link event resets its phase. LFOs linked in a chain are phase-locked to the first LFO's TRIG: a reset entering any channel's TRIG also resets every earlier channel in the chain, up to the channel whose LINK is OFF (firmware 1.2.0: a reset into TRIG 2 resets channels 1 and 2). In a full ring, a reset resets all four.

### 4.9 LFV (LFO + ALT, "vacillator")

Random, non-repeating: each cycle (period from RISE, free or clocked as §4.8) moves from the current value to a new target.
- **FALL (VARIANCE):** v = FALL in [0, 1].
  - Above noon (v > 0.5): random walk centred on 0 V. New target = uniform random in ±5 V × w, where w = (v − 0.5) × 2 scales the spread [est].
  - Below noon: drunken walk. New target = previous target + uniform random step in ±5 V × (0.5 − v) × 2 × 0.5, reflected at ±5 V [est].
  - At noon: target = previous (a slowly settling line).
- **SHAPE (SLEW):** CCW: linear segments between targets (sharp peaks). CW: cosine-interpolated segments then a one-pole low-pass at 0.5 × the cycle rate, which rounds the peaks and lowers their amplitude as the manual describes [est]. Crossfade the two by SHAPE.
- Output ±5 V × LEVEL. Each channel has its own random generator seeded differently.

### 4.10 Links (`TetraEngine`)

- Per channel: OFF, TRIG, EOR, EOF. Source = channel to the left (ring: 1 takes from 4).
- Effective trigger/gate of channel c = its own TRIG (or POLY TRIG channel or FIRE) **OR** the link source. For TRIG links the source is the left channel's effective gate (so chains of TRIG links pass one gate down the line). For EOR/EOF, the source is a short internal gate (1 ms [est]) on the rising edge of that event.
- Processing order: compute channels in order 1, 2, 3, 4 each sample, then evaluate channel 1's link from channel 4 using channel 4's state from the **previous** sample (one-sample delay breaks the feedback loop in a ring). Document this.
- A full ring of EOF links with a CYCLE-free setup (e.g. all AD, each linked EOF) creates a self-running loop once fired: FIRE on any channel starts it. This is a feature (manual: "loop around").

### 4.11 LEDs

Channel LED per §4.2. MODE, LINK colours per the tables. FIRE LED flashes on each effective trigger of that channel (firmware 1.4 "sync LEDs flash on trigger").

### 4.12 Behaviour tests (doctest, on `TetraEngine`)

1. AD at RISE = FALL = noon: output peaks at MAX after 50 ms ±1%, reaches 0 after 100 ms ±1%.
2. Time law endpoints: 0.05 ms, 50 ms, 20 s at p = 0, 0.5, 1 (±1%).
3. SHAPE noon gives a straight line (max deviation < 0.1%); CW is below the line mid-rise, CCW above.
4. AHR: 1 s gate, rise 10 ms: holds at MAX until 1 s, then falls. A 5 ms gate with 50 ms rise peaks below MAX and falls.
5. CYCLE: period = rise + fall; a trigger mid-fall restarts the rise.
6. BURST: rate 100 Hz, length 100 ms → 10 pulses; noon tilt peaks at 50 ms; retrigger OFF ignores a trigger at 30 ms, ON restarts the envelope.
7. LFO free at p = 0 → 0.05 Hz; clocked at noon follows a 2 Hz clock with phase reset on each edge; ratio 1/4 resets every 4th edge.
8. LFV: output stays within ±5 V for 10 minutes of random settings; with VARIANCE full CW the mean over 10 minutes is within ±0.3 V of 0.
9. Links: ch1 AD, ch2 EOF link: ch2 starts at ch1's end of fall. Ring EOF on four AD channels self-runs after one FIRE. TRIG link passes the gate length to an AHR.
10. Linked LFO reset: a trigger into ch2 resets ch1 and ch2 when ch2 links from ch1.
11. CV: RISE CV +5 V at trimpot +1 on p = 0.5 gives T = 20 s ×400^0.3 (range extension) ±1%.
12. Sample-rate change 44.1 → 96 kHz: times within 1%.

### 4.13 Expander protocol

TETRA writes to its **right expander** message every sample: for each channel, EOR gate, EOF gate, mode, and the current output. Use Rack's double-buffered `rightExpander.producerMessage` / `requestMessageFlip()` pattern from the Expanders manual page. TETRA X reads `leftExpander`; if the module on its left is not TETRA, all its outputs are 0 V and its CONNECTED LED is off.

---

## 5. PLINK, Modal percussion voice (32 HP)

### 5.1 Overview

Each voice: **exciter** (mallet impulse + filtered noise burst with its own envelope) → **object** (a modal resonator bank: up to 64 decaying sinusoidal partials whose frequencies, strike-position weights and decays depend on the object type and parameters) → sum of voices → **output stage** (gain, saturation, bitcrusher, limiter). Pitch, X, Y, MOD, DECAY and VEL as on the hardware. Two voices with voice stealing; preset bank with step, randomize and morph.

PLINK is our own physical-modelling design. It does not try to reproduce AAS code; it reproduces the controls, their ranges and the behaviour the Plonk manual describes.

### 5.2 Parameter list (`PlinkParams.hpp`)

All "sound" parameters use the hardware's integer-looking 0 to 127 scale (bipolar ones −64 to +63) as float values, so X/Y/MOD depths (§5.9) work in the same units. Each has a physical mapping. These are the parameters a preset stores.

| # | Name (tooltip) | Short (display) | Range | Default | Physical mapping [est unless stated] |
|---|---|---|---|---|---|
| 1 | Mallet:Noise mix | Mallet:Noise | 0..127 | 0 | 0 = mallet only, 127 = noise only; equal-power crossfade |
| 2 | Mallet stiffness | M Stiffness | 0..127 | 64 | Mallet pulse width 8 ms (0, soft) to 0.15 ms (127, stiff), exponential; also mallet low-pass 1 kHz → 18 kHz |
| 3 | Noise density | N Density | 0..127 | 127 | Random impulse rate 5 Hz (0) to 24 kHz (127), exponential; ≥ 100 sounds continuous (manual: 50 to 100 for bowed and snares) |
| 4 | Noise attack | N Attack | 0..127 | 0 | 0.1 ms to 2 s, exponential |
| 5 | Noise decay | N Decay | 0..127 | 40 | 1 ms to 5 s, exponential |
| 6 | Noise LP cutoff | N LP Cutoff | 0..127 | 127 | 40 Hz to 20 kHz, exponential |
| 7 | Noise LP envelope | N LP Env | −64..+63 | 0 | Envelope moves cutoff by ±6 octaves at full |
| 8 | Noise LP resonance | N LP Q | 0..127 | 0 | SVF Q 0.5 to 12 |
| 9 | Noise HP cutoff | N HP Cutoff | 0..127 | 0 | 20 Hz to 16 kHz, exponential |
| 10 | Noise HP envelope | N HP Env | −64..+63 | 0 | ±6 octaves |
| 11 | Noise HP resonance | N HP Q | 0..127 | 0 | SVF Q 0.5 to 12 |
| 12 | Object decay | R Decay | 0..127 | 80 | Base T60 of the fundamental 5 ms to 30 s, exponential |
| 13 | Strike position | R Position | 0..127 | 20 | Manual: 0 = edge, 127 = middle. Mapped per object (§5.5) |
| 14 | Tone (material) | R Tone | 0..127 | 64 | Manual: low = wood/nylon/plastic, high = metal/glass. Sets high-partial decay (§5.5) |
| 15 | Inharmonicity | R Inharmonic | −64..+63 | 0 | Manual: negative compresses partials, positive spreads them (§5.5) |
| 16 | Low cut | R Low Cut | 0..127 | 0 | Manual: 0 = full spectrum, higher removes low partials (§5.5) |
| 17 | Pitch envelope amount | R PEnv Amt | −64..+63 | 0 | Manual: −64 = −1 octave, +63 = +1 octave (start offset, linear glide to 0) |
| 18 | Pitch envelope decay | R PEnv Decay | 0..127 | 30 | Glide time 1 ms to 2 s, exponential |
| 19 | Object type | R Type | 6 values | Drumhead | String, Beam, Marimba, Drumhead, Membrane, Plate (manual). Not morphable |
| 20 | Noise envelope type | N Env Type | AR / AHR | AR | Manual: AHR holds while TRIG is high |
| 21 | Octave | Octave | −3..+3 | −1 | Root at PITCH knob noon and 0 V = C4 × 2^octave (−1 → C3, 130.8 Hz) [est default] |
| 22 | Quantize | Quantize | off/on | off | Manual: off = knob sweeps ±2 octaves; on = chromatic steps |
| 23 | Polyphony | Polyphony | 1 / 2 | 2 | Manual (§5.11). Not changed by randomize |
| 24 | Output gain | Output Gain | −20..+40 dB | 0 dB | Manual range (firmware 1.15) |
| 25 | Saturation | Saturation | 0..60 dB | 0 | Manual range; asymmetric (§5.10) |
| 26 | Bitcrusher | Bitcrusher | off, 10..1 bit | off | Manual: off = full resolution, then 10 bit down to 1 bit |

The panel knobs (§5.3) and the preset values are the same thing: loading a preset moves the knobs. Saturation and Bitcrusher are exempt from randomize (manual). Output Gain is applied from the current knob, not from stepped presets (manual: the current preset's gain applies).

### 5.3 Panel layout (32 HP)

Top: module name, then the **display** (a 2-line OLED-style text widget, dark background `#1B1B1B`, warm white text, 26 characters per line, under the title across the middle of the panel).

Sections, left to right (each a labelled box):

- **EXCITER** (12 small knobs in a 4 × 3 grid plus a switch): MALLET:NOISE, STIFFNESS, DENSITY, ATTACK, DECAY (noise decay), LP CUTOFF, LP ENV, LP Q, HP CUTOFF, HP ENV, HP Q, and an **AR/AHR** two-position switch.
- **OBJECT** (7 small knobs plus a type selector): OBJECT type is a 6-position **button with six small labelled LEDs** (STRING, BEAM, MARIMBA, DRUMHEAD, MEMBRANE, PLATE; press cycles, right-click selects directly). Knobs: DECAY (object decay, labelled "OBJ DECAY"), POSITION, TONE, INHARM, LOW CUT, PENV AMT, PENV DECAY.
- **PERFORM** (the hardware's four panel knobs, large): PITCH, DECAY (master decay, labelled "DECAY ALL"), X, Y. Under PITCH: **OCTAVE −** / **OCTAVE +** buttons and a **QUANT** latching button. TRIGGER: a large momentary button (red LED).
- **MOD** row: three buttons **X DEST**, **Y DEST**, **MOD DEST** and one **DEPTH** button. Pressing a DEST button shows that input's destination on the display and enters "pick" mode: turning a **SELECT** knob (an endless encoder, Rack `RoundBlackKnob` with snap off, read as relative steps) scrolls destinations; pressing the same DEST button again confirms. DEPTH cycles LOW → MED → HIGH → FULL for the selected input. Right-click on each DEST button also gives a plain submenu of destinations and depths.
- **PRESET** row: **LOAD**, **SAVE**, **PREV**, **NEXT**, **RANDOM** buttons, and the SELECT encoder doubles as preset scroll in LOAD/SAVE mode (§5.14).
- **OUTPUT** (3 trimpots): GAIN, SATURATION, CRUSH.
- Two voice LEDs (amplitude of voice 1 and 2, as on the hardware).

Bottom jack row, inputs then output, labelled: **X**, **Y**, **MOD**, **DECAY**, **PITCH**, **TRIG**, **VEL**, **OUT**. Above X, MOD and DECAY: small attenuverter trimpots labelled ATTEN (the hardware lists attenuverters for X, MOD and DECAY, and the manual treats Y the same as X, so give Y one too). Default +1.

Right-click menu: Voices (1, 2, 4, 8; §5.12), Velocity mode (Accent / Dynamics / Volume), Gate delay (0, 1, 2, 5 ms; default 0 because Rack cables are sample-accurate; hardware minimum ~2 ms), Partials (32 / 64; default 64), Init preset, Reset bank to factory, Export bank (JSON file), Import bank.

### 5.4 Exciter (`Exciter.hpp`, per voice)

**Mallet.** On trigger, a single raised-cosine pulse of width W (from Stiffness), peak amplitude A_m = velocity gain (§5.8), followed by a one-pole low-pass at the stiffness frequency. Softer = longer pulse, fewer highs, fuller body (manual). Optional realism [est]: add 3% random variation of W per hit so repeated hits are not identical.

**Noise.** A "dust" generator: at density rate R, emit random-sign impulses of random amplitude (uniform 0.5 to 1) at Poisson-random times; at R ≥ 0.5 × sample rate, switch to plain white noise (crossfade over the top 5% of the knob). Low density gives audible clicks, high density continuous noise (manual).
- Then **HP filter** then **LP filter** in series (12 dB/oct state-variable, Cytomic/TPT form), each with its Q. Their cutoffs are modulated by the noise envelope × the ENV amount in octaves.
- Then the **noise envelope**: AR = linear-in-dB attack then exponential decay [est curve]; AHR = attack, hold at full while TRIG is high, then decay on release. Retrigger restarts the attack from the current level.

**Mix.** exciter = cos(θ) × mallet + sin(θ) × noise, θ = (mix / 127) × π/2. The exciter drives the modal bank. Also add a small direct exciter leak to the output (−30 dB [est]) so very short decays still click.

### 5.5 Object: modal bank (`ModalBank.hpp`, `ModeTables.hpp`)

Each partial k is a two-pole resonator (implement as a decaying complex phasor / "modal oscillator": y_k[n] = r_k × e^(iω_k) × y_k[n−1] + g_k × x[n], output = Im(y_k)) with frequency f_k, decay radius r_k from T60_k, and input gain g_k. Process 4 partials per `float_4`. Drop partials with f_k > 0.45 × sample rate (fade gain over the last 10% to avoid clicks when pitch moves).

**Mode tables.** `scripts/make_mode_tables.py` (numpy + scipy, run once, output committed) generates `ModeTables.hpp`: for each object, 64 entries of (frequency ratio to the first partial, a position-weight function index, mode numbers). Do not use `std::cyl_bessel_j` at runtime (libc++ on Mac lacks it); everything heavy is tabulated. For 2-D objects, also tabulate weights on a 128-point position grid. Table definitions:

| Object | Partial ratios (sorted, normalised so the first = 1) | Strike-position weight w_k(p), p in [0, 1] from Position 0..127 |
|---|---|---|
| **String** | k (harmonic) | Ideal string, struck at x = 0.02 + 0.48 p from one end: w = |sin(k π x)|. At p = 1 (middle) even harmonics vanish (manual) |
| **Beam** (uniform free-free bar) | (β_k / β_1)², β_k = roots of cos β cosh β = 1 (4.7300, 7.8532, 10.9956, then (2k+1)π/2): 1, 2.756, 5.404, 8.933, 13.344, 18.638, … | x = 0.5 − 0.5 p (edge to centre). Free-bar mode shapes, approximated as w = |cos((k+1) π x)| normalised so the edge (x = 0) gives 1 for all k. At the centre the anti-symmetric partials vanish |
| **Marimba** (undercut bar, variable cross-section) | Tuned first three partials 1 : 4 : 10 (marimba practice), then the beam series rescaled to continue smoothly from 10 (multiply beam ratios k ≥ 4 by 10 / 8.933 × 1.05) [est] | As Beam. Also lower partials 2 and 3 by 3 dB so the fundamental dominates [est] |
| **Drumhead** (circular membrane, clamped edge) | j_mn / j_01 for Bessel zeros j_mn, m = 0..15, n = 1..8, sorted: 1, 1.594, 2.136, 2.296, 2.653, 2.918, … (first 64) | Radius r = 0.92 (1 − p) from centre (edge → near rim, middle → centre): w = |J_m(j_mn r)| normalised per table. At the centre only m = 0 modes ring, which sounds "bunched" and drum-like (manual) |
| **Membrane** (rectangular, clamped) | sqrt((m/a)² + (n/b)²) with a : b = 1 : 1.37 [est], m, n = 1..12, sorted, first 64 | Strike point moves along the diagonal from (0.05, 0.05) at p = 0 to (0.5, 0.5) at p = 1: w = |sin(m π x) sin(n π y)| |
| **Plate** (rectangular, simply supported) | (m/a)² + (n/b)² normalised, a : b = 1 : 1.27 [est], m, n = 1..12, sorted, first 64 | As Membrane |

Normalise each object's weights so the summed energy at Position 0 equals the summed energy at Position 64 (avoid big level jumps when sweeping position) [est].

**Inharmonicity** (I = −64..+63, i = I / 64 in [−1, ~1]). Warp each ratio: ρ'_k = ρ_k^(1 + 0.35 i) [est]. Negative i compresses partials toward the fundamental, positive spreads them (manual). Keep ρ'_1 = 1 and the order sorted.

**Tone (material).** Per-partial decay: T60_k = T60_base × (f_1 / f_k)^α, with α = 2.2 − 2.0 × (Tone / 127) [est]: Tone 0 → α 2.2 (high partials die very fast, woody); Tone 127 → α 0.2 (highs ring almost as long as the fundamental, metallic/glassy). Floor T60_k at 1 ms.

**Low Cut** (0..127). Gain multiplier per partial: g_k ×= h(ρ'_k / c), with c = 1 + 15 × (LowCut / 127)² (cut-off "ratio") and h(z) = z² / (1 + z²) when LowCut > 0, else 1 [est]. At high settings the fundamental and low partials are attenuated, leaving bright, thin tones (manual: "attenuates low partials").

**Decay.** T60_base = R Decay time (§5.2) × master DECAY factor. Master DECAY knob plus DECAY CV (× attenuverter, 5 V = full travel) gives d in [0, 1]; factor = d^2 × 1.0 (fully CW = 100% per manual, CCW = 0, i.e. very short) [est curve]. The manual says the DECAY knob scales both noise-envelope decay and object decay: apply the same factor to the noise decay time too (floor 1 ms).

**Pitch.** f_1 = root(Octave) × 2^(knob + PITCH CV) where the knob gives ±2 octaves continuous (Quantize off) or chromatic semitone steps ±24 (Quantize on). **PITCH input is sampled only while TRIG is high** (manual): latch the CV at the trigger edge (after Gate delay) and keep following it while the gate stays high, hold it after. f_k = f_1 × ρ'_k × pitch envelope.

**Pitch envelope.** On every trigger, a linear glide in octaves from PEnv Amt (−1..+1 oct) to 0 over PEnv Decay time (manual: linear, retriggered on every exciter trigger).

**Parameter changes while ringing.** Frequencies and decays update every 32 samples (control rate) with interpolation of ω and r per sample block, so X/Y/MOD sweeps and pitch envelopes are smooth. Weights (Position, Low Cut) apply to the input gains g_k, so changing them affects the next strike only for already-ringing partials; that is physically right.

**Level.** Normalise the bank output so a default Drumhead hit at velocity 5 V peaks around ±5 V [est]. Calibrate per object type with a gain table generated by the test harness (render a default hit per object, set gains so peaks match within 2 dB).

### 5.6 Voices (`PlinkVoice.hpp`, `PlinkEngine.hpp`)

- **Polyphony 2 (default):** a new trigger goes to a free voice; if both are busy, it takes the **oldest** voice (manual). The old sound keeps ringing on the other voice with its modulation **frozen at its last value** (manual): when a voice stops being the newest, snapshot its X/Y/MOD-modulated parameter values and stop applying live modulation and knob changes to it. Only the shared output stage (§5.10) still affects it.
- **Polyphony 1:** a new trigger chokes the decaying sound: fade the old voice over 3 ms [est] and start the new one (on the same voice slot; use a short crossfade buffer so there is no click).
- A voice is "free" when its summed partial energy falls below −90 dBFS for 50 ms.

### 5.7 TRIG

Rising edge (Schmitt §2) after Gate delay: latch pitch, pick voice, start mallet, noise envelope and pitch envelope, evaluate MOD special functions (§5.9) that act on trigger. The TRIGGER button does the same (velocity per §5.8 as if VEL were unpatched). For AHR, the gate high time holds the noise envelope.

### 5.8 VEL

Normalled to +5 V. Read at the trigger (dynamics) or continuously (volume). Modes (right-click, saved per patch; the hardware keeps it in CONFIG):
- **Accent** (default): if VEL > 1.5 V at the trigger, the hit is accented: exciter gain × 2 (+6 dB) and mallet stiffness +12 units for that hit [est]; otherwise gain × 1. With nothing patched, every hit is accented (manual).
- **Dynamics:** exciter strength ∝ VEL voltage: gain = VEL / 5, clamped 0..1.2, with stiffness + (gain − 0.5) × 16 units so harder hits are brighter [est, physically sensible].
- **Volume:** VEL directly sets the final output level of that voice: level = VEL / 5, clamped 0..1, read continuously (smoothed 2 ms).

### 5.9 X, Y and MOD

**Panel and CV.** X knob is a bipolar manual offset for the X-assigned parameter; same for Y. The X input (± 5 V, × its attenuverter) adds to it. MOD has no knob (as on the hardware); its jack goes through its attenuverter. DECAY has its own knob and jack (§5.5).

**Destinations (parametric)**, for X, Y and MOD: Mallet:Noise, M Stiffness, N Attack, N Decay, N Density, N LP Cutoff, N LP Env, N LP Q, N HP Cutoff, N HP Env, N HP Q, R Decay, R Position, R Tone, R Inharmonic, R PEnv Amt, R PEnv Decay, R Pitch, Saturation, Bitcrusher. Default X = R Tone, Y = R Position, MOD = R Decay [est defaults].

**R Pitch** destination is the manual's way to FM the object: depth units are semitones (FULL = ±128 units → treat as ±24 semitones at ±5 V [est]) and it bypasses the trigger latch (audio-rate FM works).

**Depth:** LOW ±16, MED ±32, HIGH ±64, FULL ±128 units for ±5 V (manual). The knob's full travel (X, Y) = the same span. Parameter value = base (panel knob) + X contribution + Y contribution + MOD contribution, clamped to the parameter's range. Several inputs may target the same parameter; contributions add.

**Special MOD functions** (MOD only):
- **Choke Both / Choke Res / Choke Noise:** a gate above 1.5 V at MOD applies a fast decay (T60 = 15 ms [est]) to the object, the noise envelope, or both, on all voices, while high.
- **Preset Step:** on each trigger, the MOD voltage picks a preset in the range A..B: 0 V = A, ±5 V (absolute value) = B, linear in between, rounded (manual). The voice plays that preset's sound parameters (#1 to #20) instead of the knobs; PITCH, DECAY ALL, X, Y and VEL still apply live, and the panel's Output Gain applies (manual). The knobs do not move. The display shows `Pst A:B 01:04` and the last picked slot. A and B are set in the MOD DEST pick mode: after choosing Preset Step, DEPTH steps into "set A" then "set B" with SELECT. Each voice keeps the parameters of the preset it was struck with, so kits are polyphonic (manual).
- **Randomize:** a rising edge at MOD randomizes all exciter and object parameters (#1 to #20, excluding Polyphony, Saturation, Bitcrusher) to uniform random values, **moving the knobs**. The RANDOM button does the same (hardware: encoder press). Keep the previous values for one level of undo via Rack's history (`history::ModuleChange`) when triggered from the button.
- **Morph:** MOD voltage crossfades between the current patch (knobs) at 0 V and a target preset at ±5 V (absolute value), linearly per parameter (manual). Object type and the AR/AHR switch do not morph (manual). Target preset chosen as for Preset Step (DEPTH then SELECT).

### 5.10 Output stage

Sum of voices → **Output Gain** (−20..+40 dB, manual) → **Saturation** (asymmetric: y = tanh(G x + 0.25) − tanh(0.25), G = 10^(sat/20), then divide by a make-up gain so 0 dB sat is unity and high sat stays near ±5 V [est curve]) → **Bitcrusher** (quantise to the chosen bit depth over a ±5 V range, no sample-rate reduction, since the manual only mentions resolution) → **limiter** (look-ahead-free peak limiter: instant attack, 80 ms release, ceiling ±9.5 V, then hard clip at ±10 V; manual mentions "Plonk's output limiter") → OUT.

Run the saturation 2× oversampled (polyphase half-band) to avoid aliasing at high drive.

### 5.11 Display

Line 1: context (preset `01 Kick Wood*` where `*` = modified since load; or `X ▸ R Tone  HIGH`; or the parameter being turned, `R Tone 72`). Line 2: value detail or `Pst A:B 01:04`. When a knob moves, show its name and value for 1.5 s, then return to the preset line. Draw as a Rack `LedDisplay`-style widget with a monospace font converted at build time (or Rack's bundled `ShareTechMono`), updated only from the UI thread via values the engine publishes atomically.

### 5.12 Polyphonic cables (right-click Voices)

- **Voices 1 or 2** (hardware behaviour): TRIG, PITCH, VEL are read as mono (channel 0) and voices are allocated as §5.6.
- **Voices 4 or 8**: if TRIG is a polyphonic cable, channel c triggers voice c (with its own PITCH c and VEL c, falling back to channel 0 if those cables have fewer channels). If TRIG is mono, allocate round-robin with oldest-steal over the chosen voice count. OUT stays a mono mix; add a right-click "Poly out" option that sends voice c to OUT channel c.
- CPU: at 8 voices × 64 partials PLINK may reach ~6%; that is acceptable, note it in the manual and suggest 32 partials for big poly use.

### 5.13 Calibration

None needed (hardware has a 1 V/oct calibration routine; Rack voltages are exact). Not implemented.

### 5.14 Presets

- 128 slots, each holding parameters #1 to #22 (sound + octave/quantize) and a name of up to 12 characters. Stored in the patch JSON (`bank` array). Rack's own module presets (right-click → Preset) also work and store the bank too.
- **LOAD:** press LOAD, scroll with SELECT (the display previews each preset by playing it on the next trigger, as the hardware previews), press LOAD again or SELECT-click (use a small push button labelled OK beside SELECT) to load, any other button cancels and restores the previous sound. A star marks the loaded slot.
- **SAVE:** press SAVE, scroll to a slot, press SAVE again to save with the current name. Naming: right-click → "Rename preset…" opens a Rack text field menu item (character-by-character encoder naming is not needed in Rack).
- **PREV / NEXT:** load the previous/next slot directly.
- **Init preset** (right-click): loads the defaults in §5.2.
- **Factory bank (`FactoryPresets.hpp`):** 32 presets of our own, slots 1 to 32, rest Init. Slots 1, 5, 9 and 13 are **kits** as on the hardware: they set MOD = Preset Step with A:B covering the next three slots (kick, snare, hat, perc), so a step sequencer's CV into MOD plays a kit from one PLINK. Suggested list (tune each by ear in a render test; values are starting points, using the §5.2 order: type, mix, stiff, dens, n atk, n dec, lp, lp env, lp q, hp, hp env, hp q, r dec, pos, tone, inharm, lowcut, penv amt, penv dec, env type, octave):

| Slot | Name | Starting values |
|---|---|---|
| 1 | Kit Wood | Kit: Preset Step A:B 2:4 (slot 1 itself = slot 2's sound) |
| 2 | Kick Skin | Drumhead, 10, 50, 127, 0, 20, 60, 0, 0, 0, 0, 0, 60, 5, 20, 0, 0, +40, 25, AR, −2 |
| 3 | Snare Wire | Drumhead, 85, 90, 115, 0, 55, 110, −10, 10, 40, 0, 10, 45, 30, 60, −10, 10, +8, 10, AR, −1 |
| 4 | Hat Plate | Plate, 110, 127, 127, 0, 25, 127, 0, 0, 90, 0, 20, 30, 50, 120, +40, 70, 0, 10, AR, +1 |
| 5 | Kit Metal | Kit: A:B 6:8 |
| 6 | Kick Beam | Beam, 0, 30, 127, 0, 10, 90, 0, 0, 0, 0, 0, 55, 0, 40, −20, 0, +50, 20, AR, −2 |
| 7 | Clank | Plate, 40, 110, 127, 0, 15, 100, 0, 0, 20, 0, 0, 60, 70, 110, +30, 20, 0, 10, AR, 0 |
| 8 | Tick | String, 60, 127, 127, 0, 5, 127, 0, 0, 70, 0, 0, 15, 90, 100, +60, 60, 0, 10, AR, +2 |
| 9 | Kit Hand | Kit: A:B 10:12 |
| 10 | Conga | Drumhead, 5, 70, 127, 0, 10, 80, 0, 0, 0, 0, 0, 70, 40, 50, 0, 0, +6, 15, AR, 0 |
| 11 | Clap Noise | Membrane, 120, 64, 60, 0, 40, 100, 0, 30, 50, 0, 0, 25, 60, 70, 0, 20, 0, 10, AR, 0 |
| 12 | Shaker | Membrane, 127, 64, 90, 30, 30, 127, 0, 0, 100, 0, 0, 10, 60, 100, 0, 60, 0, 10, AHR, +1 |
| 13 | Kit Glass | Kit: A:B 14:16 |
| 14 | Glass Kick | Beam, 0, 60, 127, 0, 10, 80, 0, 0, 0, 0, 0, 70, 20, 115, −30, 0, +30, 30, AR, −1 |
| 15 | Glass Hit | Plate, 30, 120, 127, 0, 10, 110, 0, 0, 40, 0, 0, 90, 30, 127, +20, 20, 0, 10, AR, +1 |
| 16 | Shard | Beam, 70, 127, 100, 0, 8, 127, 0, 20, 80, 0, 30, 40, 100, 120, +63, 50, 0, 10, AR, +2 |
| 17 | Marimba | Marimba, 0, 60, 127, 0, 10, 80, 0, 0, 0, 0, 0, 85, 25, 30, 0, 0, 0, 10, AR, 0 |
| 18 | Vibe Bar | Beam, 0, 70, 127, 0, 10, 80, 0, 0, 0, 0, 0, 110, 20, 110, −5, 0, 0, 10, AR, 0 |
| 19 | Kalimba | Beam, 10, 90, 127, 0, 10, 100, 0, 0, 0, 0, 0, 80, 10, 90, +12, 0, 0, 10, AR, +1 |
| 20 | Nylon Pluck | String, 0, 80, 127, 0, 10, 90, 0, 0, 0, 0, 0, 75, 30, 25, 0, 0, 0, 10, AR, 0 |
| 21 | Steel String | String, 0, 110, 127, 0, 10, 110, 0, 0, 0, 0, 0, 100, 20, 115, +4, 0, 0, 10, AR, 0 |
| 22 | Bowed String | String, 127, 64, 75, 60, 70, 70, 0, 20, 10, 0, 0, 100, 25, 90, 0, 0, 0, 10, AHR, 0 |
| 23 | Tom Low | Drumhead, 10, 60, 127, 0, 10, 80, 0, 0, 0, 0, 0, 70, 20, 40, 0, 0, +20, 40, AR, −1 |
| 24 | Tom High | Drumhead, 10, 70, 127, 0, 10, 80, 0, 0, 0, 0, 0, 65, 20, 45, 0, 0, +20, 35, AR, 0 |
| 25 | Gong | Plate, 15, 30, 127, 0, 20, 70, 0, 0, 0, 0, 0, 127, 40, 120, +10, 0, −8, 60, AR, −2 |
| 26 | Bell | Plate, 0, 110, 127, 0, 10, 110, 0, 0, 0, 0, 0, 115, 15, 125, −20, 0, 0, 10, AR, +1 |
| 27 | Cowbell-ish | Plate, 10, 120, 127, 0, 10, 100, 0, 0, 30, 0, 0, 50, 40, 110, −40, 30, 0, 10, AR, +1 |
| 28 | Log Drum | Beam, 0, 40, 127, 0, 10, 60, 0, 0, 0, 0, 0, 60, 30, 20, −10, 0, +4, 15, AR, −1 |
| 29 | Water Drop | Membrane, 0, 127, 127, 0, 5, 127, 0, 0, 0, 0, 0, 40, 60, 90, 0, 0, +63, 6, AR, +1 |
| 30 | Zap | Drumhead, 0, 127, 127, 0, 5, 127, 0, 0, 0, 0, 0, 50, 10, 80, 0, 0, +63, 40, AR, +1 |
| 31 | Rain Plate | Plate, 127, 64, 25, 0, 90, 90, 0, 0, 20, 0, 0, 90, 60, 100, 0, 10, 0, 10, AHR, 0 |
| 32 | Wind Tube | Membrane, 127, 64, 127, 70, 90, 60, −20, 50, 20, 0, 30, 100, 64, 80, 0, 0, 0, 10, AHR, 0 |

(A kit slot stores its own first-member sound so the slot plays something when loaded, plus the MOD = Preset Step setting and A:B. When a kit slot is loaded, MOD's assignment is set from the preset; when any non-kit preset is loaded, the X/Y/MOD assignments are left as they are [est; the manual doesn't say whether assignments are stored per preset; storing them only in kits keeps kits working and avoids surprises].)

Write a host-side render test (`tests/render_presets.cpp`) that renders 2 s of every factory preset to WAV in a temp folder and checks: peak between −12 and −0.5 dBFS (at ±5 V = 0 dBFS reference), no NaN, decays to below −60 dB by the end for presets with Object Decay < 100. Attach a few renders to the hand-over so Hardy can hear them.

### 5.15 Behaviour tests (doctest, on `PlinkEngine`)

1. String, Position 127: even harmonics are at least 40 dB below odd ones in an FFT of the first 200 ms.
2. Drumhead partial frequencies match the Bessel ratio table within 0.1% (FFT peak picking at 48 kHz, 1 s render, fundamental 200 Hz).
3. Inharmonicity +63 raises partial 5's ratio, −64 lowers it, partial 1 unchanged.
4. Tone 0 vs 127: the 10th partial's T60 is at least 10× shorter at Tone 0.
5. Low Cut 127: fundamental at least 20 dB down relative to Low Cut 0.
6. PITCH latch: changing PITCH CV while TRIG is low doesn't change a ringing note; with TRIG held high it does.
7. 1 V/oct: +1 V doubles f_1 within 1 cent over −3..+3 V (Quantize off).
8. Pitch envelope +63: f_1 starts 1 octave up and reaches nominal after PEnv Decay time, linearly in octaves.
9. Polyphony 2: three triggers → the first voice is stolen by the third; polyphony 1 → each trigger chokes the previous within 5 ms.
10. Preset Step: MOD at 0 V plays A, at 5 V plays B, at 2.5 V the middle slot (A:B 1:3 → 2).
11. Morph: MOD 0 V → knob values, 5 V → target values, 2.5 V → midpoint (per parameter), object type unchanged.
12. Randomize never changes Polyphony, Saturation, Bitcrusher; all values stay in range.
13. Depth LOW at +5 V adds 16 units, FULL adds 128 (clamped).
14. AHR noise envelope holds while TRIG is high and decays after release.
15. Output never exceeds ±10 V with Output Gain +40 dB, Saturation 60 dB and 8 voices hit at once.
16. Fuzz: 10 minutes of random parameter changes and triggers at audio rate: no NaN/inf, no denormal slowdown (flush to zero on).

---

## 6. TETRA X, End-of-stage expander (6 HP)

### 6.1 Layout

Four rows, one per channel, labelled 1 to 4: **EOR** and **EOF** output jacks with a small LED each. Bottom: **POLY EOR** and **POLY EOF** (4 channels each). Top: a CONNECTED LED (lit when TETRA is directly on the left).

### 6.2 Gate definitions (manual, firmware 1.4.5)

| Mode | EOR | EOF |
|---|---|---|
| AD | High at the end of rise (peak), stays high until the end of fall | Low from the peak; high again at the end of fall. (So EOF is high while idle) |
| AHR | High at the peak, stays high until the end of release | Low from the start of release (after hold); high again at the end of release |
| CYCLE | High at the peak, low at the end of the cycle | Low at the peak, high again at the end of the cycle |
| BURST | High at the start of each pulse for the first half of the pulse | 0 V during the whole burst, high at the end of the burst until the next burst |
| LFO | High from half-way through the cycle, low at reset/cycle start | High for the first half of the cycle, low for the second |
| LFV | High while the output is moving down | High while it is moving up |

Initial state before any trigger: AD/AHR EOR low, EOF high (idle at the end of fall) [est reading]. Gate level 10 V [est].

### 6.3 Tests

One test per table row against `TetraEngine` stage flags, plus: expander unplugged → 0 V outputs; expander to the right of a non-TETRA → 0 V.

---

## 7. Leaning on existing modules (document in the manual)

PLINK is a full voice, TETRA is pure modulation. For the patch recipes use free Library modules:
- **Sequencing PLINK:** VCV SEQ-3 or any step sequencer for TRIG, PITCH and a CV row into MOD (Preset Step kits).
- **Stereo and space:** VCV Mixer, Valley Plateau (free reverb) for the long-decay metallic presets.
- **VCA/LPG for TETRA as an envelope source:** VCV VCA, Bogaudio LPG.
- **Oscillators for TETRA LFO/burst audio tricks:** VCV VCO.
Say in each recipe which modules are needed and why.

---

## 8. General tests

- Plugin loads; modules appear when searching "TETRA", "PLINK", "Tetrax", "function generator", "envelope", "burst", "percussion", "physical model", "modal", "marimba".
- All params have tooltips with units; all ports named; POLY OUT reports 4 channels.
- Save/load restores every button state, menu choice and PLINK's full bank (including renamed presets).
- Duplicating PLINK copies its bank.
- Sample-rate change 44.1 → 96 kHz keeps times, rates and PLINK partial frequencies correct within 1%.
- CPU within §0 budgets on the Mac build, measured with Rack's meter.
- TETRA X works only directly to the right of TETRA.

---

## 9. Manual (`docs/MANUAL.md` and `docs/Tetrax-Manual.pdf`)

Same structure as the Silverbox and Tudor manuals: what each module is (one line saying it's inspired by the Intellijel Quadrax / Qx / Plonk, a living maker's current product, and that this is an independent behavioural re-imagining with its own names, and for PLINK that the physical models are our own and not AAS's), a labelled panel drawing, every control in plain English, the mode and colour tables, the TETRA X gate table, PLINK's parameter table with ranges, and **patch recipes** (modules needed and settings for each):

1. **Four-stage ring:** all four TETRA channels AD, each LINK = EOF, FIRE once: a self-running four-segment loop. Different RISE/FALL per channel makes an irregular complex LFO; mix the four outs.
2. **Burst ratchets:** TETRA ch1 BURST into a VCA on a drum, rate ~30 Hz, length 150 ms, SHAPE CCW (fading rolls).
3. **Audio-rate bursts as a voice:** BURST rate high, length long, ALT SHAPE, pitch via RISE CV: buzzy tones with an envelope.
4. **Clocked LFO family:** clock into TRIG 1, channels 2 to 4 LINK = TRIG in LFO mode at different ratios: four phase-locked related LFOs.
5. **LFV drift:** LFO + ALT, slow rate, VARIANCE below noon: a drunk modulation for PLINK's Position and Tone (TETRA OUT → PLINK X and Y).
6. **Envelope-follows-gate cascade:** ch1 AHR from a gate, ch2 LINK = EOR (fires at the peak), ch3 LINK = EOF: attack, punch and tail as three separate envelopes.
7. **PLINK kit from one sequencer:** load Kit Wood, sequencer gate → TRIG, a CV row (0, 1.67, 3.33, 5 V) → MOD: kick, snare, hat, perc.
8. **Marimba melody:** Marimba preset, sequencer pitch → PITCH, accents → VEL.
9. **Morph between two sounds:** MOD = Morph target Glass Hit, TETRA ch1 LFV into MOD: each hit lands somewhere between wood and glass.
10. **Random hits:** MOD = Randomize, a slow clock divided by 8 into MOD, steady triggers on TRIG: a new instrument every bar.
11. **Bowed and blown:** Bowed String / Wind Tube with AHR noise envelope and long gates; X = N Density for breathiness.
12. **FM'd objects:** X = R Pitch, an audio VCO into X: metallic clangs; set DEPTH LOW for subtle shimmer.
13. **TETRA meets PLINK:** TETRA ch1 CYCLE drives TRIG via TETRA X EOR; ch2 BURST into PLINK MOD set to Choke Res for stuttering decays.

Build the PDF with `scripts/make_manual_pdf.sh` (as in the Silverbox repo). Copy the finished PDF to `/mnt/project-files/quadrax-plonk-vcv/Tetrax-Manual.pdf` if that folder is reachable.

---

## 10. Order of work

1. Repo skeleton, plugin.json, CI (Mac arm64), `make test` harness.
2. `Curves`, `TetraChannel`, `TetraEngine` + tests §4.12 and §6.3.
3. `make_mode_tables.py` → `ModeTables.hpp`; `ModalBank`, `Exciter`, `PlinkVoice`, `PlinkEngine`, presets + tests §5.15; render test and level calibration per object.
4. Panels (`make_panels.py`), module wrappers, expander messaging, display widget, menus, persistence.
5. General tests §8, CI green, tag `v2.0.0`.
6. Manual and PDF, README with install steps.
7. Hand-over: what was built, every [est] value used, every ambiguity decided, CI run link, release link, a few preset renders.

---

## 11. Accuracy ledger (values not in the manuals, all [est])

| Item | Value used | Basis |
|---|---|---|
| Gate threshold | 1.5 V high / 0.5 V low | Plonk MOD gate figure; Quadrax gives none |
| TETRA time law | 0.05 ms × 1000^(2p) to noon, 50 ms × 400^(2p−1) after | Manual gives 0.05 ms / 50 ms / 20 s points only |
| CV range extension | p clamped to [−0.15, 1.15] | Manual says CV extends range, no numbers |
| Rate law (BURST, LFO) | 0.05 Hz × 66660^p | Manual endpoints 0.05 Hz and ~3.33 kHz |
| Curve law | Exponential with k = 10 |s| | Manual: log / lin / exp, "more extreme" since 1.1 |
| Dead zone | 2% each end | 1.2.0 had 5%, 1.2.1 "reduced" |
| AD/CYCLE retrigger | Restart rise from current level | Not stated |
| Burst pulses | N = floor(L × f), evenly spaced | Manual: pulses evenly spaced in the length |
| Burst square/sine switch | Square within 10% of each end | Manual: square at the extremes, sine through the middle |
| Burst ALT envelope | Flat | Manual describes ALT waveform only |
| LFO clock ratio table | 17 steps, 1/64 to 64 | Manual gives ends and noon only |
| LFO base shapes | sine, triangle, ramp up, square, ramp down | Manual's chart is an image |
| LFO MORPH | Phase warp (pulse width / skew) | Manual: "variants of the selected waveshape" |
| LFV variance and slew | §4.9 formulas | Manual describes behaviour only |
| Link EOR/EOF pulse | 1 ms internal gate | Not stated |
| Ring evaluation | Ch1 reads Ch4 one sample late | Needed to break the loop |
| TETRA X gate level | 10 V | Manual gives only high / 0 V |
| TETRA X idle state | AD/AHR EOF high when idle | Reading of the manual's definitions |
| PLINK physical ranges | All mappings in §5.2 table | Manual gives 0..127 scales only |
| Mallet model | Raised-cosine pulse 8 ms to 0.15 ms + low-pass | Manual: stiffer = shorter, crisper |
| Noise density | 5 Hz to 24 kHz dust | Manual: clicks at low, white noise at high |
| Mode ratio tables | Physics formulas in §5.5 | AAS models unpublished |
| Marimba ratios | 1 : 4 : 10 then beam series | Marimba tuning practice |
| Membrane / Plate aspect ratios | 1 : 1.37 / 1 : 1.27 | Not stated |
| Inharmonicity warp | ρ^(1 + 0.35 i) | Manual: compress / spread |
| Tone law | T60_k ∝ (f_1/f_k)^(2.2 − 2 Tone/127) | Manual: wood to metal by partial decay |
| Low Cut law | z²/(1+z²), cut ratio up to 16 | Manual: attenuates low partials |
| Master DECAY curve | d² factor on object and noise decay | Manual: scales both, CW = 100% |
| Choke time | 15 ms | Manual: "fast envelope" |
| Accent | +6 dB, +12 stiffness | Manual: accent only |
| Dynamics brightness | ± 8 stiffness units | Physically sensible |
| R Pitch depth | FULL = ±24 semitones | Manual: FM via R Pitch, no units |
| Polyphony-1 choke fade | 3 ms | Not stated |
| Saturation curve | Biased tanh, 2× oversampled | Manual: asymmetric curve, 0 to 60 dB |
| Limiter | Ceiling 9.5 V, 80 ms release | Manual mentions a limiter only |
| X/Y/MOD default destinations | R Tone / R Position / R Decay | Not stated |
| Assignments stored with presets | Only for kit presets | Manual silent |
| Factory presets | Our own 32 | Hardware's Richard Devine/AAS set is not public and not ours to copy |

Each value lives in `src/dsp/Constants.hpp` (PLINK tables in `ModeTables.hpp`). If a render or listening test suggests a better value, change it there and note it in DECISIONS.
