# Tetrax for VCV Rack

Three modules for VCV Rack 2:

| Module | What it is |
|---|---|
| **TETRA** | Four-channel function generator: AD, AHR, cycling, burst and LFO channels (each with an alternate mode, including a random "vacillator"), linked in a ring |
| **TETRA X** | Expander for TETRA: end-of-rise and end-of-fall gates for each channel |
| **PLINK** | Modal percussion voice: a mallet and noise exciter striking a string, beam, marimba bar, drumhead, membrane or plate, with preset-step kits, morph and randomize |

TETRA and TETRA X are an independent behavioural re-imagining inspired by the Intellijel Quadrax and Qx. PLINK is inspired by the Intellijel Plonk; its physical models are our own modal synthesis. None of this is affiliated with or endorsed by Intellijel or Applied Acoustics Systems.

Manual: [docs/MANUAL.md](docs/MANUAL.md) and [docs/Tetrax-Manual.pdf](docs/Tetrax-Manual.pdf).

## Install

Download `Tetrax-2.x.x-mac-arm64.vcvplugin` from the latest release (or the Actions run artifact) and put it in your Rack user folder's `plugins-mac-arm64` folder, or double-click it with Rack closed. Restart Rack and search for TETRA or PLINK.

## Build

```
export RACK_DIR=/path/to/Rack-SDK
make dist        # the plugin
make test        # engine tests, no SDK needed
make panels      # regenerate panels (needs fonttools)
make render      # render PLINK's factory sounds to build/render/
```

Licence: GPL-3.0-or-later.
