# Decisions made during the build

Ambiguities in the spec or the manuals, and how they were settled. Estimated numbers live in `src/dsp/Constants.hpp`; this file records the choices that are not just a number.

## Names (Hardy, 2026-11-15)

- Plugin `Tetrax` (the spec's working name was Tessera). Repo `hardyfinn/tetrax`.
- The percussion voice is `PLINK` (the spec's working name was TOCK).
- TETRA and TETRA X keep their names. The expander's slug is `TETRAX`, which is also the plugin slug; Rack allows this because module slugs only need to be unique within a plugin.

## TETRA

- **Panel width:** 30 HP, not the spec's estimated 24 HP. Four 6.8 HP channel columns with RISE, FALL, SHAPE, LEVEL, five buttons, four CV jacks with attenuverters, TRIG and OUT do not fit in less without breaking the spacing rules, plus a strip for POLY TRIG and POLY OUT.
- **Test harness:** the Silverbox-style dependency-free `tests/check.hpp` harness, not doctest. Same `make test`.
- **Burst when TRIG is unpatched and the channel is not linked:** free-runs (a new burst starts as soon as one ends). The Quadrax manual says an unpatched TRIG means free-running.
- **ALT is remembered per mode:** switching MODE shows each mode's own ALT state.
- **Linked LFOs count as clocked:** an LFO channel whose LINK is on uses the link as its clock (§4.8 "TRIG patched or linked").
- **Mode change:** envelopes keep their level; cycling and LFO restart.

## PLINK

- **Output gain is a preset parameter.** The Plonk manual says that when stepping presets "the current preset's output gain applies", so each preset stores its own gain and the GAIN trimpot moves when a preset loads. The factory gains were measured by rendering every sound (accented hit peaks near 3.5 V).
- **Accent is the normal level.** VEL is normalled to +5 V, so with nothing patched every hit is accented (manual). An unaccented hit is 6 dB quieter and slightly softer, rather than an accented one being 6 dB louder, so the factory sounds are calibrated at the level you get with VEL unpatched.
- **Kits:** slots 1, 5, 9 and 13 set MOD = Preset Step over the next three slots (kick, snare, hat style sounds). The kit slot plays its first member's sound when loaded without MOD. Loading a non-kit preset while MOD is on Preset Step moves MOD back to R Decay.
- **Noise input to the resonators is scaled by each partial's bandwidth**, so a long object decay does not make noise-driven (bowed, blown) sounds explode in level. A mallet strike is not scaled this way.
- **Level calibration per object** (`objGain` in `Plink.hpp`): each object type's default hit peaks at about 5 V.
- **Marimba ratios:** 1 : 4 : 10, then the beam series scaled to continue from 10 (17.4, 25.9, 36.2 …). The spec's formula said "×10/8.933"; the intended result, given in its first draft as 17.4, 26.0, 36.5, is what was implemented.
- **Exciter leak:** a small direct click from the mallet (the pulse at unit peak, -30 dB) reaches the output so very short decays still have an attack.
- **Level calibration of sustained sounds:** AHR presets (Bowed String, Rain Plate, Wind Tube, Shaker) were measured with a 1 s gate, and noise-driven presets use the median of eight random seeds.
- **Spectral tilt:** each partial's input is scaled by ratio^-0.5, so higher partials are a little quieter at the strike, as in real struck objects.
- **SELECT** is an ordinary knob read as relative steps (turning it by one unit moves one item), so it never needs re-centring in practice.
- **LOAD preview** loads the browsed preset into the knobs straight away (so you hear it on the next hit); any other button cancels and restores the sound you had.
- **Poly out** (right-click) sends one channel per voice after the output gain, without saturation, crusher or limiter.
- **Default sound** for a new PLINK is factory slot 17, Marimba, so the knobs change what you hear (a kit preset would ignore the knobs until MOD is changed).
- **Calibration** (hardware 1 V/oct routine) is not needed in Rack and is not implemented.

## Not on the panels

- TETRA X has one CONNECTED LED instead of a small LED per jack, to keep the 6 HP panel clean.
