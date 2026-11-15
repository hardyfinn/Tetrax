// Every value here that the manuals do not give is an estimate, marked [est] with the
// spec section it comes from (docs/BUILD-INSTRUCTIONS.md). Change values here only,
// and note any change in docs/DECISIONS.md.
#pragma once

namespace tx {

// ---- shared (§2) ----
constexpr float GATE_HIGH = 1.5f;          // [est] §2 Schmitt high threshold (V)
constexpr float GATE_LOW = 0.5f;           // [est] §2 Schmitt low threshold (V)
constexpr float CV_CLIP = 5.f;             // Quadrax manual: CV beyond ±5 V is clipped

// ---- TETRA (§4) ----
constexpr float T_MIN = 0.00005f;          // manual: ~0.05 ms at full CCW
constexpr float T_NOON = 0.05f;            // manual: ~50 ms at noon
constexpr float T_MAX = 20.f;              // manual: ~20 s at full CW
constexpr float P_LO = -0.15f;             // [est] §4.3 CV range extension below the knob
constexpr float P_HI = 1.15f;              // [est] §4.3 CV range extension above the knob
constexpr float DEAD_ZONE = 0.02f;         // [est] §4.3 knob end dead zone (1.2.1 reduced the 5% one)
constexpr float RATE_MIN = 0.05f;          // manual: 0.05 Hz
constexpr float RATE_MAX = 3333.f;         // manual: about 3.33 kHz
constexpr float CURVE_K = 10.f;            // [est] §4.3 curve steepness at the SHAPE extremes
constexpr float BURST_MIN_LEN = 0.0003f;   // manual: burst length ~0.3 ms at CCW
constexpr float BURST_SQUARE_ZONE = 0.1f;  // [est] §4.7 square within 10% of each SHAPE end
constexpr float LINK_PULSE = 0.001f;       // [est] §4.10 EOR/EOF link gate length (s)
constexpr int CLOCK_RATIOS = 17;           // [est] §4.8 table size
constexpr float CLOCK_TIMEOUT_PERIODS = 4.f; // [est] §4.8 keep running after the clock stops
constexpr float LFV_DRUNK_STEP = 0.5f;     // [est] §4.9 drunken-walk step scale
constexpr float TX_GATE_V = 10.f;          // [est] §6 TETRA X gate level

// ---- PLINK (§5) ----
constexpr float C4_HZ = 261.6256f;
constexpr int MAX_MODES = 64;
constexpr int CONTROL_BLOCK = 32;          // §5.5 control-rate update
constexpr float NYQ_FRACTION = 0.45f;      // §5.5 drop partials above this fraction of the sample rate
constexpr float MALLET_W_SOFT = 0.008f;    // [est] §5.2 mallet pulse width at stiffness 0 (s)
constexpr float MALLET_W_STIFF = 0.00015f; // [est] §5.2 at stiffness 127
constexpr float MALLET_LP_SOFT = 1000.f;   // [est] §5.2 mallet low-pass at stiffness 0 (Hz)
constexpr float MALLET_LP_STIFF = 18000.f; // [est] §5.2 at stiffness 127
constexpr float MALLET_JITTER = 0.03f;     // [est] §5.4 per-hit width variation
constexpr float DENS_MIN = 5.f;            // [est] §5.2 noise impulse rate at 0 (Hz)
constexpr float DENS_MAX = 24000.f;        // [est] §5.2 at 127
constexpr float NATK_MIN = 0.0001f, NATK_MAX = 2.f;   // [est] §5.2 noise attack (s)
constexpr float NDEC_MIN = 0.001f, NDEC_MAX = 5.f;    // [est] §5.2 noise decay (s)
constexpr float NLP_MIN = 40.f, NLP_MAX = 20000.f;    // [est] §5.2 noise LP cutoff (Hz)
constexpr float NHP_MIN = 20.f, NHP_MAX = 16000.f;    // [est] §5.2 noise HP cutoff (Hz)
constexpr float NENV_OCT = 6.f;            // [est] §5.2 filter envelope range (octaves at full)
constexpr float Q_MIN = 0.5f, Q_MAX = 12.f;          // [est] §5.2 filter Q
constexpr float RDEC_MIN = 0.005f, RDEC_MAX = 30.f;  // [est] §5.2 object decay T60 (s)
constexpr float PENV_DEC_MIN = 0.001f, PENV_DEC_MAX = 2.f; // [est] §5.2 pitch-envelope time (s)
constexpr float INHARM_EXP = 0.35f;        // [est] §5.5 ratio warp exponent
constexpr float TONE_ALPHA_WOOD = 2.2f;    // [est] §5.5 partial-decay slope at Tone 0
constexpr float TONE_ALPHA_METAL = 0.2f;   // [est] §5.5 at Tone 127
constexpr float LOWCUT_MAX_RATIO = 16.f;   // [est] §5.5 low-cut corner ratio at 127
constexpr float SPECTRAL_TILT = 0.5f;      // [est] §5.5 partial input gain ∝ ratio^-tilt
constexpr float T60_FLOOR = 0.001f;        // §5.5 shortest partial T60
constexpr float CHOKE_T60 = 0.015f;        // [est] §5.9 choke decay
constexpr float POLY1_FADE = 0.003f;       // [est] §5.6 polyphony-1 choke fade (s)
constexpr float ACCENT_GAIN = 2.f;         // [est] §5.8 accent +6 dB
constexpr float ACCENT_STIFF = 12.f;       // [est] §5.8 accent stiffness offset (units)
constexpr float DYN_STIFF = 16.f;          // [est] §5.8 dynamics stiffness span (units)
constexpr float RPITCH_FULL_SEMI = 24.f;   // [est] §5.9 R Pitch: 128 units = 24 semitones
constexpr float EXCITER_LEAK = 0.0316f;    // [est] §5.4 direct exciter to output (-30 dB)
constexpr float LIMIT_CEIL = 9.5f;         // [est] §5.10 limiter ceiling (V)
constexpr float LIMIT_RELEASE = 0.08f;     // [est] §5.10 limiter release (s)
constexpr float SAT_BIAS = 0.25f;          // [est] §5.10 asymmetric saturation bias
constexpr float VOICE_FREE_DB = -90.f;     // §5.6 voice free threshold
constexpr float OUT_SCALE = 5.f;           // §5.5 nominal output level (V)
constexpr float MEMBRANE_ASPECT = 1.37f;   // [est] §5.5
constexpr float PLATE_ASPECT = 1.27f;      // [est] §5.5

} // namespace tx
