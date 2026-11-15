# Quadrax and Plonk in VCV Rack: research and proposal

Researched 2026-11-15 for Hardy. Both are current Intellijel products, so everything we make uses our own names and panels. Plonk was co-developed with Applied Acoustics Systems (AAS), whose physical models are proprietary, so our version is our own modal synthesis built to behave like the manual describes.

## Sources

- Quadrax manual, firmware 1.4.5, revision 2026.08.10 (intellijel.com/downloads/manuals/quadrax_manual_1.4.5_2026.08.10.pdf). Read in full. Covers the Qx expander.
- Quadrax manual, revision 2019.11.05 (original firmware), for history only.
- Plonk manual, firmware 1.16, revision 2020.11.08 (intellijel.com/downloads/manuals/plonk_manual_v1.16_2020.11.08.pdf), plus the v1.00 manual. Firmware 1.16 is still the newest (product page checked, last modified 2025-10-15).
- Intellijel product pages for both. Sound on Sound and Synthtopia Plonk reviews were found but not needed for the spec.
- Not found anywhere public: schematics, DSP, curve laws, time tables, Plonk's factory preset names, the five LFO wave shapes (they are in an image in the Quadrax manual).

## Quadrax (14 HP) and Qx (4 HP)

Four identical digital function generator channels. Each has RISE, FALL and SHAPE knobs, a TRIG input, an output, a MODE button and a LINK/CV button.

**Modes:** AD (trigger runs the whole envelope), AHR (attack on gate, hold while high, release on fall), CYCLE (free-running, period = rise + fall, TRIG resets), BURST (a train of pulses inside an envelope, rate up to about 3.33 kHz), LFO (±5 V, clockable with ratios 1/64 to 64x). A long press switches each mode to an ALTERNATE version: mirrored curves for AD/AHR/CYCLE, saw-tri-ramp pulses for BURST, and the "LFV" random vacillator for LFO.

**Times:** about 0.05 ms at full CCW, 50 ms at noon, 20 s at full CW. SHAPE goes log, linear (noon) and exponential.

**Linking:** each channel can be fired by the previous channel's TRIG (as a gate), end of rise or end of fall, and channel 1 links from channel 4, so the four make a ring. Linked LFOs phase-lock to the first one.

**CV:** four shared CV inputs (A to D). Any of them can be routed to any channel's RISE, FALL, SHAPE or LEVEL with four attenuation steps and invert, set through a hidden button matrix.

**Utility:** manual trigger per channel, per channel max output of 5 V or 10 V, output attenuation in 20% steps.

**Qx:** an EOR and EOF gate per channel (8 jacks), exactly defined for every mode. No inputs.

**Verdict: very feasible, high faithfulness.** It is fully digital and the manual describes every behaviour. The unknowns (curve law, the in-between time values, LFO wave shapes, clock ratio table, gate voltage) are small and we estimate them. The only real design question is the hidden CV matrix, which works badly in Rack.

## Plonk (12 HP)

A two-voice percussion synth. An **exciter** (a mallet impulse mixed with filtered, enveloped noise) strikes an **object** (String, Beam, Marimba, Drumhead, Membrane or Plate). About 20 parameters live in menus behind a display and encoder; four knobs (PITCH, DECAY, X, Y) are on the panel.

**Exciter:** mallet/noise mix, mallet stiffness, noise density (sparse clicks up to continuous noise), series HP and LP filters with Q and envelope amount, noise attack/decay, AR or AHR envelope.

**Object:** type, decay, strike position (edge to middle), tone (wood to metal: how fast high partials die), inharmonicity (compress or spread partials), low cut, pitch envelope amount (±1 octave) and decay. Polyphony 1 or 2.

**Modulation:** X, Y and MOD inputs each pick any parameter as a destination with four depths (±16/32/64/128 on a 0 to 127 scale). MOD also has special jobs: choke, **preset step** (each hit picks a preset between A and B from the MOD voltage, which is how its drum kits work), **randomize** and **morph** (crossfade the current sound toward a target preset).

**Other:** 1 V/oct pitch sampled at trigger, VEL input with accent/dynamics/volume modes, output gain, asymmetric saturation, bitcrusher, 128 preset slots.

**Verdict: feasible as a behavioural instrument, not a clone.** The control set and the modulation/preset system can be copied exactly. The sound engine cannot, because AAS's models are not published. A modal resonator bank (up to 64 tuned, decaying partials per voice) with physically derived partial ratios for each object type gets very close in kind: this is how AAS's own Chromaphone works in principle. Expect it to sound like a good physical-modelling percussion voice with the same controls, not identical to the hardware.

## What already exists in VCV Rack

| Area | Existing modules | How ours differs |
|---|---|---|
| Function generators | Befaco Rampage (2 ch, Maths-style), Bogaudio AD/DADSRH, Count Modula, many ADSRs | None has Quadrax's 4-channel ring linking, BURST mode, LFV, or the EOR/EOF definitions per mode |
| Burst generators | Audible Instruments ports, Bogaudio, various | Burst as a function-generator mode with tilt and waveform is unusual |
| Physical modelling | Audible Instruments Modal Synthesizer (Elements) and Resonator (Rings), VCV Pluck, Surge XT oscillators | Elements is the closest (exciter + modal resonator) but is a pitched voice with no object types, no preset step/morph, no 2-voice choke. Nothing in the library is a Plonk-style percussion voice |
| Drum voices | Hora Analog Drums, VCV Drums, SV Modular, Prok | Analog-style models, not physical |

So both are worth building. Neither duplicates something good that is already free.

## Proposal: plugin "Tetrax", three modules

| Module | Inspired by | HP | One line |
|---|---|---|---|
| **TETRA** Four-channel function generator | Quadrax | 24 [est] | Four linked envelope/burst/LFO channels with visible per-channel CV |
| **TETRA X** End-of-stage expander | Qx | 6 | EOR and EOF gates for each channel, plus poly versions. Snaps to the right of TETRA |
| **PLINK** Modal percussion voice | Plonk | 30 [est] | Mallet and noise exciter striking six modal objects, X/Y/MOD assignment, preset step and morph |

## Choices (defaults in bold, say if you want any changed)

1. **Names:** plugin **Tetrax**, modules **TETRA, TETRA X, PLINK**. Repo suggestion: an empty `hardyfinn/Tetrax`.
2. **TETRA CV:** **per-channel RISE, FALL, SHAPE and LEVEL CV jacks with attenuverters** (everything visible, the Rack way). Alternative: the hardware's four shared CV inputs with a routing matrix (one CV can drive up to 16 destinations, but routing is hidden behind buttons).
3. **Qx as an expander:** **yes, a separate 6 HP module** that sits to the right, as on the hardware and like your Silverbox extras. Alternative: put the 8 gates on TETRA itself.
4. **PLINK controls:** **every exciter and object parameter is a knob on the panel**, with a small display for presets and X/Y/MOD destinations. Alternative: hardware-style display and encoder menus on a 12 HP panel (smaller, but nothing is MIDI-mappable or visible).
5. **PLINK voices:** **2 voices like the hardware**, plus a right-click option for up to 8 voices driven by polyphonic TRIG/PITCH/VEL cables.
6. **PLINK presets:** **128 internal slots saved with the patch**, needed for preset step and morph, filled with 32 of our own factory sounds including four step-kits in slots 1, 5, 9 and 13 as on the hardware.
7. **Panels:** **clean flat matte off-white with black text and one accent colour per module**, no silver, no gradients or textures.
