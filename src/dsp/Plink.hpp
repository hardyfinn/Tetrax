// PLINK modal percussion voice engine (spec §5). Plain C++17, no Rack includes.
#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <string>
#include "Constants.hpp"
#include "ModeTables.hpp"
#include "Tetra.hpp"   // clampf, Rng

namespace tx {

// ---------------------------------------------------------------- parameters (§5.2)
enum PlinkParam {
	P_MIX, P_STIFF, P_DENS, P_NATK, P_NDEC, P_LPCUT, P_LPENV, P_LPQ, P_HPCUT, P_HPENV, P_HPQ,
	P_RDEC, P_RPOS, P_RTONE, P_RINH, P_RLOWCUT, P_PENVAMT, P_PENVDEC,
	P_TYPE, P_ENVTYPE, P_OCTAVE, P_QUANT, P_GAIN,
	NP
};
constexpr int NP_CONT = P_TYPE;   // continuous, morphable, modulatable parameters

enum ObjectType { OBJ_STRING, OBJ_BEAM, OBJ_MARIMBA, OBJ_DRUMHEAD, OBJ_MEMBRANE, OBJ_PLATE, OBJ_COUNT };

struct ParamInfo { const char* name; const char* shortName; float lo, hi, def; };
inline const ParamInfo& paramInfo(int i) {
	static const ParamInfo info[NP] = {
		{"Mallet:Noise mix", "Mallet:Noise", 0, 127, 0},
		{"Mallet stiffness", "M Stiffness", 0, 127, 64},
		{"Noise density", "N Density", 0, 127, 127},
		{"Noise attack", "N Attack", 0, 127, 0},
		{"Noise decay", "N Decay", 0, 127, 40},
		{"Noise LP cutoff", "N LP Cutoff", 0, 127, 127},
		{"Noise LP envelope", "N LP Env", -64, 63, 0},
		{"Noise LP resonance", "N LP Q", 0, 127, 0},
		{"Noise HP cutoff", "N HP Cutoff", 0, 127, 0},
		{"Noise HP envelope", "N HP Env", -64, 63, 0},
		{"Noise HP resonance", "N HP Q", 0, 127, 0},
		{"Object decay", "R Decay", 0, 127, 80},
		{"Strike position", "R Position", 0, 127, 20},
		{"Tone (material)", "R Tone", 0, 127, 64},
		{"Inharmonicity", "R Inharmonic", -64, 63, 0},
		{"Low cut", "R Low Cut", 0, 127, 0},
		{"Pitch envelope amount", "R PEnv Amt", -64, 63, 0},
		{"Pitch envelope decay", "R PEnv Decay", 0, 127, 30},
		{"Object type", "R Type", 0, 5, OBJ_DRUMHEAD},
		{"Noise envelope type", "N Env Type", 0, 1, 0},
		{"Octave", "Octave", -3, 3, -1},
		{"Quantize", "Quantize", 0, 1, 0},
		{"Output gain", "Output Gain", -20, 40, 0},
	};
	return info[i];
}
inline const char* objectName(int t) {
	static const char* n[OBJ_COUNT] = {"String", "Beam", "Marimba", "Drumhead", "Membrane", "Plate"};
	return n[std::max(0, std::min(OBJ_COUNT - 1, t))];
}

// Exponential map of a 0..127 value onto lo..hi.
inline float expMap(float v, float lo, float hi) { return lo * std::pow(hi / lo, clampf(v, 0.f, 127.f) / 127.f); }

// Physical value of a parameter, for tooltips and the engine.
inline float physical(int i, float v) {
	switch (i) {
		case P_STIFF: return expMap(v, MALLET_W_SOFT, MALLET_W_STIFF);
		case P_DENS: return expMap(v, DENS_MIN, DENS_MAX);
		case P_NATK: return expMap(v, NATK_MIN, NATK_MAX);
		case P_NDEC: return expMap(v, NDEC_MIN, NDEC_MAX);
		case P_LPCUT: return expMap(v, NLP_MIN, NLP_MAX);
		case P_HPCUT: return expMap(v, NHP_MIN, NHP_MAX);
		case P_LPENV: case P_HPENV: return clampf(v, -64.f, 63.f) / 64.f * NENV_OCT;
		case P_LPQ: case P_HPQ: return expMap(v, Q_MIN, Q_MAX);
		case P_RDEC: return expMap(v, RDEC_MIN, RDEC_MAX);
		case P_PENVAMT: return clampf(v, -64.f, 63.f) / 64.f;
		case P_PENVDEC: return expMap(v, PENV_DEC_MIN, PENV_DEC_MAX);
		default: return v;
	}
}

inline std::string physicalString(int i, float v) {
	char b[48];
	float p = physical(i, v);
	auto t = [&](float s) { if (s < 1.f) std::snprintf(b, sizeof b, "%.3g ms", s * 1000.f); else std::snprintf(b, sizeof b, "%.3g s", s); };
	switch (i) {
		case P_MIX: std::snprintf(b, sizeof b, "%d:%d", (int) std::lround(100.f - v / 1.27f), (int) std::lround(v / 1.27f)); break;
		case P_STIFF: std::snprintf(b, sizeof b, "%.2f ms pulse", p * 1000.f); break;
		case P_DENS: std::snprintf(b, sizeof b, "%.4g Hz", p); break;
		case P_NATK: case P_NDEC: case P_RDEC: case P_PENVDEC: t(p); break;
		case P_LPCUT: case P_HPCUT: std::snprintf(b, sizeof b, "%.4g Hz", p); break;
		case P_LPENV: case P_HPENV: std::snprintf(b, sizeof b, "%+.2f oct", p); break;
		case P_LPQ: case P_HPQ: std::snprintf(b, sizeof b, "Q %.2f", p); break;
		case P_PENVAMT: std::snprintf(b, sizeof b, "%+.2f oct", p); break;
		case P_RPOS: std::snprintf(b, sizeof b, "%s", v < 8 ? "edge" : (v > 119 ? "middle" : "")); break;
		case P_RTONE: std::snprintf(b, sizeof b, "%s", v < 43 ? "wood" : (v < 86 ? "mixed" : "metal")); break;
		default: b[0] = 0;
	}
	return b;
}

// ---------------------------------------------------------------- modulation (§5.9)
enum Dest {
	DEST_OFF = 0,
	DEST_PARAM0 = 1,                     // DEST_PARAM0 + i for continuous param i
	DEST_RPITCH = DEST_PARAM0 + NP_CONT,
	DEST_SAT, DEST_CRUSH,
	DEST_CHOKE_BOTH, DEST_CHOKE_RES, DEST_CHOKE_NOISE, DEST_PRESET_STEP, DEST_RANDOM, DEST_MORPH,
	DEST_COUNT
};
constexpr int DEST_XY_COUNT = DEST_CRUSH + 1;   // X and Y stop before the special MOD functions

inline std::string destName(int d) {
	if (d == DEST_OFF) return "Off";
	if (d < DEST_RPITCH) return paramInfo(d - DEST_PARAM0).shortName;
	switch (d) {
		case DEST_RPITCH: return "R Pitch";
		case DEST_SAT: return "Saturation";
		case DEST_CRUSH: return "Bitcrusher";
		case DEST_CHOKE_BOTH: return "Choke Both";
		case DEST_CHOKE_RES: return "Choke Res";
		case DEST_CHOKE_NOISE: return "Choke Noise";
		case DEST_PRESET_STEP: return "Preset Step";
		case DEST_RANDOM: return "Randomize";
		case DEST_MORPH: return "Morph";
	}
	return "?";
}

enum Depth { DEPTH_LOW, DEPTH_MED, DEPTH_HIGH, DEPTH_FULL, DEPTH_COUNT };
inline float depthUnits(int d) { static const float u[4] = {16.f, 32.f, 64.f, 128.f}; return u[std::max(0, std::min(3, d))]; }
inline const char* depthName(int d) { static const char* n[4] = {"LOW", "MED", "HIGH", "FULL"}; return n[std::max(0, std::min(3, d))]; }

// ---------------------------------------------------------------- presets (§5.14)
constexpr int BANK_SIZE = 128;
struct Preset {
	float v[NP];
	char name[13];
	bool kit = false;        // kit presets also set MOD = Preset Step with A:B
	int kitA = 0, kitB = 0;  // zero-based slots
	void init() {
		for (int i = 0; i < NP; i++) v[i] = paramInfo(i).def;
		std::snprintf(name, sizeof name, "Init");
		kit = false; kitA = kitB = 0;
	}
	void setName(const std::string& s) { std::snprintf(name, sizeof name, "%s", s.substr(0, 12).c_str()); }
};

// Sets every continuous parameter, the object type and the envelope type to random values
// (§5.9: not Polyphony, Saturation, Bitcrusher, octave or quantize).
inline void randomizeSound(float* v, Rng& r) {
	for (int i = 0; i < NP_CONT; i++) {
		const ParamInfo& pi = paramInfo(i);
		v[i] = std::round(pi.lo + r.uni() * (pi.hi - pi.lo));
	}
	v[P_TYPE] = (float) (r.next() % OBJ_COUNT);
	v[P_ENVTYPE] = (float) (r.next() % 2);
}

// ---------------------------------------------------------------- filters
struct Svf {   // Cytomic TPT state-variable filter
	float ic1 = 0.f, ic2 = 0.f, a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 2.f;
	void set(float fc, float q, float sr) {
		fc = clampf(fc, 10.f, 0.49f * sr);
		float g = std::tan((float) M_PI * fc / sr);
		k = 1.f / q;
		a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
	}
	void reset() { ic1 = ic2 = 0.f; }
	// returns {low, high}
	void process(float v0, float& low, float& high) {
		float v3 = v0 - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		low = v2; high = v0 - k * v1 - v2;
	}
};

// ---------------------------------------------------------------- one voice (§5.4 to §5.6)
struct PlinkVoice {
	bool active = false;
	uint64_t age = 0;          // trigger order
	int channel = 0;           // poly channel it follows
	bool live = true;          // newest voice: follows modulation; others are frozen (§5.6)
	bool usesPreset = false;   // struck by Preset Step: base parameters come from a preset
	float base[NP];            // base parameters at the strike (preset) or knobs
	float p[NP];               // effective parameters (frozen when !live)
	float rpitchSemi = 0.f;
	// pitch
	float pitchCv = 0.f;
	float penvOct = 0.f, penvStep = 0.f;
	// mallet
	int malletN = 0, malletI = 0;
	float malletAmp = 0.f, malletLp = 0.f, malletA = 1.f;
	// noise
	float nenv = 0.f;
	int nstage = 0;            // 0 idle, 1 attack, 2 hold, 3 decay
	bool gate = false;
	Svf hp, lp;
	// gain
	float velGain = 1.f, volLevel = 1.f, stiffOffset = 0.f;
	float fade = 1.f, fadeStep = 0.f;
	bool chokeRes = false, chokeNoise = false;
	// modal bank
	int nModes = 0;
	float re[MAX_MODES], im[MAX_MODES], cr[MAX_MODES], ci[MAX_MODES], gm[MAX_MODES], gn[MAX_MODES];
	float peak = 0.f, quiet = 0.f;
	float lastOut = 0.f;
	// per-block cached values (§5.5 control rate)
	float densProb = 0.f, whiteMix = 0.f, atkStep = 1.f, decMul = 0.f, mixM = 1.f, mixN = 0.f;

	void clear() {
		std::memset(re, 0, sizeof re); std::memset(im, 0, sizeof im);
		std::memset(gm, 0, sizeof gm); std::memset(gn, 0, sizeof gn);
		std::memset(cr, 0, sizeof cr); std::memset(ci, 0, sizeof ci);
		hp.reset(); lp.reset(); nenv = 0.f; nstage = 0; malletN = 0; malletLp = 0.f;
		peak = quiet = 0.f; active = false; fade = 1.f; fadeStep = 0.f; lastOut = 0.f;
	}
};

struct PlinkSettings {
	int voices = 2;            // 1, 2, 4, 8 (1 and 2 = hardware polyphony)
	int velMode = 0;           // 0 accent, 1 dynamics, 2 volume
	float gateDelay = 0.f;     // seconds
	int partials = 64;         // 32 or 64
	bool polyOut = false;
};

// Per-sample inputs. Voltages already scaled by the attenuverters.
struct PlinkIn {
	int channels = 1;          // channels on TRIG (1 = mono)
	bool gate[8] = {};
	float pitch[8] = {};       // V/oct
	float vel[8] = {5, 5, 5, 5, 5, 5, 5, 5};
	float x = 0.f, y = 0.f, mod = 0.f;   // CV after attenuverters (V)
	float decayCv = 0.f;
};

// Panel state the engine reads once per control block.
struct PlinkPanel {
	float knobs[NP];           // base sound parameters (panel knobs, 0..127 units etc.)
	float pitchKnob = 0.f;     // -1..1 (= ±2 octaves)
	float decayKnob = 1.f;     // 0..1
	float xKnob = 0.f, yKnob = 0.f;   // -1..1
	float satDb = 0.f;
	int crush = 0;             // 0 off, 1..10 = 10 bit .. 1 bit
	int dest[3] = {DEST_PARAM0 + P_RTONE, DEST_PARAM0 + P_RPOS, DEST_PARAM0 + P_RDEC};   // X, Y, MOD
	int depth[3] = {DEPTH_HIGH, DEPTH_HIGH, DEPTH_HIGH};
	int presetA = 0, presetB = 3, morphTarget = 1;
	PlinkPanel() { for (int i = 0; i < NP; i++) knobs[i] = paramInfo(i).def; }
};

// Half-band FIR for 2x oversampling the saturation (§5.10).
struct HalfBand {
	static constexpr int T = 31;
	float h[T];
	float up[T] = {}, dn[T] = {};
	int ui = 0, di = 0;
	HalfBand() {
		for (int i = 0; i < T; i++) {
			int n = i - T / 2;
			float s = n == 0 ? 0.5f : std::sin((float) M_PI * n / 2.f) / ((float) M_PI * n);
			float w = 0.42f - 0.5f * std::cos(2.f * (float) M_PI * i / (T - 1)) + 0.08f * std::cos(4.f * (float) M_PI * i / (T - 1));
			h[i] = s * w;
		}
	}
	static float fir(const float* buf, int idx, const float* h) {
		float acc = 0.f;
		for (int i = 0; i < T; i++) acc += h[i] * buf[(idx + i) % T];
		return acc;
	}
	template <class F>
	float process(float x, F f) {
		float y[2];
		for (int k = 0; k < 2; k++) {
			ui = (ui + T - 1) % T;
			up[ui] = k == 0 ? 2.f * x : 0.f;
			float u = fir(up, ui, h);
			di = (di + T - 1) % T;
			dn[di] = f(u);
			y[k] = fir(dn, di, h);
		}
		return y[1];
	}
	void reset() { std::memset(up, 0, sizeof up); std::memset(dn, 0, sizeof dn); }
};

// ---------------------------------------------------------------- the engine
struct PlinkEngine {
	float sr = 48000.f, dt = 1.f / 48000.f;
	PlinkSettings set;
	PlinkPanel panel;
	Preset bank[BANK_SIZE];
	PlinkVoice voice[8];
	uint64_t counter = 0;
	int blockPos = 0;
	Rng rng;
	// per-channel trigger detection and gate delay
	bool gatePrev[8] = {};
	int pending[8];
	bool modHigh = false;
	// outputs
	float out = 0.f;
	float polyOut[8] = {};
	float voiceLevel[8] = {};
	// requests for the module (UI thread acts on them)
	bool randomizeRequest = false;
	int lastStepSlot = -1;
	// output stage
	HalfBand os;
	float limEnv = 0.f;
	float objGain[OBJ_COUNT] = {1.53f, 2.531f, 2.864f, 0.411f, 0.22f, 1.357f};   // §5.5 level calibration (default hit ≈ 5 V)

	PlinkEngine() {
		for (auto& b : bank) b.init();
		for (int i = 0; i < 8; i++) { pending[i] = -1; voice[i].clear(); }
		rng.seed(12345u);
		setRate(48000.f);
	}
	void setRate(float r) { sr = r; dt = 1.f / r; }
	void reset() { for (auto& v : voice) v.clear(); os.reset(); limEnv = 0.f; }
	int maxVoices() const { return std::max(1, std::min(8, set.voices)); }

	// ---------------------------------------------------------- parameters with modulation
	// Contribution in parameter units of input s (0 X, 1 Y, 2 MOD) to destination d.
	float modUnits(int s, int d, const PlinkIn& in) const {
		if (panel.dest[s] != d) return 0.f;
		float v = (s == 0) ? panel.xKnob + in.x / 5.f : (s == 1) ? panel.yKnob + in.y / 5.f : in.mod / 5.f;
		return clampf(v, -1.f, 1.f) * depthUnits(panel.depth[s]);
	}
	float modSum(int d, const PlinkIn& in) const { return modUnits(0, d, in) + modUnits(1, d, in) + modUnits(2, d, in); }

	void effective(const float* base, const PlinkIn& in, float* p, float& rpitch) const {
		for (int i = 0; i < NP; i++) p[i] = base[i];
		for (int i = 0; i < NP_CONT; i++) {
			const ParamInfo& pi = paramInfo(i);
			p[i] = clampf(base[i] + modSum(DEST_PARAM0 + i, in), pi.lo, pi.hi);
		}
		rpitch = modSum(DEST_RPITCH, in) * RPITCH_FULL_SEMI / 128.f;
	}

	void morphBase(float* out) const {
		for (int i = 0; i < NP; i++) out[i] = panel.knobs[i];
	}

	// ---------------------------------------------------------- strike
	void strike(int ch, const PlinkIn& in) {
		int nv = maxVoices();
		int vi = -1;
		bool polyCable = (in.channels > 1 && nv > 2);
		if (polyCable) vi = ch % nv;                     // poly cable: channel c -> voice c (§5.12)
		else if (nv == 1) {
			// polyphony 1: alternate slots so the old sound can fade (§5.6)
			int cur = voice[0].active && (!voice[1].active || voice[0].age > voice[1].age) ? 0 : 1;
			vi = 1 - cur;
			if (voice[cur].active) voice[cur].fadeStep = 1.f / std::max(1.f, POLY1_FADE * sr);
		} else {
			uint64_t oldest = UINT64_MAX;
			for (int i = 0; i < nv; i++) if (!voice[i].active) { vi = i; break; }
			if (vi < 0) for (int i = 0; i < nv; i++) if (voice[i].age < oldest) { oldest = voice[i].age; vi = i; }
		}
		PlinkVoice& v = voice[vi];
		if (!polyCable) for (auto& o : voice) o.live = false;
		bool wasActive = v.active;
		v.active = true; v.live = true; v.age = ++counter; v.channel = ch;
		v.fade = 1.f; v.fadeStep = 0.f; v.quiet = 0.f;
		if (!wasActive) { std::memset(v.re, 0, sizeof v.re); std::memset(v.im, 0, sizeof v.im); v.hp.reset(); v.lp.reset(); }
		// base parameters: Preset Step picks a preset by the MOD voltage (§5.9)
		v.usesPreset = false;
		if (panel.dest[2] == DEST_PRESET_STEP) {
			int a = std::max(0, std::min(BANK_SIZE - 1, panel.presetA)), b = std::max(0, std::min(BANK_SIZE - 1, panel.presetB));
			float t = clampf(std::fabs(in.mod) / 5.f, 0.f, 1.f);
			int slot = (int) std::lround(a + t * (b - a));
			std::memcpy(v.base, bank[slot].v, sizeof v.base);
			v.usesPreset = true;
			lastStepSlot = slot;
		} else std::memcpy(v.base, panel.knobs, sizeof v.base);
		float rp;
		currentBase(v, in);
		effective(v.base, in, v.p, rp);
		v.rpitchSemi = rp;
		// velocity (§5.8)
		float vel = in.vel[std::min(ch, 7)];
		v.velGain = 1.f; v.stiffOffset = 0.f; v.volLevel = 1.f;
		// Accent: VEL is normalled to +5 V, so an accented hit is the normal level and an
		// unaccented one is 6 dB quieter and slightly softer.
		if (set.velMode == 0) { if (vel <= GATE_HIGH) { v.velGain = 1.f / ACCENT_GAIN; v.stiffOffset = -ACCENT_STIFF; } }
		else if (set.velMode == 1) { v.velGain = clampf(vel / 5.f, 0.f, 1.2f); v.stiffOffset = (v.velGain - 0.5f) * DYN_STIFF; }
		else v.volLevel = clampf(vel / 5.f, 0.f, 1.f);
		// pitch latch and pitch envelope (§5.5)
		v.pitchCv = in.pitch[std::min(ch, 7)];
		v.penvOct = physical(P_PENVAMT, v.p[P_PENVAMT]);
		v.penvStep = v.penvOct / std::max(1.f, physical(P_PENVDEC, v.p[P_PENVDEC]) * sr);
		// mallet
		float w = physical(P_STIFF, clampf(v.p[P_STIFF] + v.stiffOffset, 0.f, 127.f)) * (1.f + MALLET_JITTER * rng.bi());
		v.malletN = std::max(2, (int) std::lround(w * sr));
		v.malletI = 0;
		v.malletAmp = 2.f / v.malletN;
		float lpHz = expMap(clampf(v.p[P_STIFF] + v.stiffOffset, 0.f, 127.f), MALLET_LP_SOFT, MALLET_LP_STIFF);
		v.malletA = 1.f - std::exp(-2.f * (float) M_PI * std::min(lpHz, 0.45f * sr) / sr);
		// noise envelope
		v.nstage = 1;
		v.gate = true;
		updateVoice(v, in, true);
	}

	// Base parameters for a voice: knobs (live), its preset (Preset Step) or a morph.
	void currentBase(PlinkVoice& v, const PlinkIn& in) {
		if (v.usesPreset) return;
		std::memcpy(v.base, panel.knobs, sizeof v.base);
		if (panel.dest[2] == DEST_MORPH) {
			const Preset& t = bank[std::max(0, std::min(BANK_SIZE - 1, panel.morphTarget))];
			float m = clampf(std::fabs(in.mod) / 5.f, 0.f, 1.f);
			for (int i = 0; i < NP_CONT; i++) v.base[i] = panel.knobs[i] + m * (t.v[i] - panel.knobs[i]);
		}
	}

	// ---------------------------------------------------------- control-rate update
	float modeWeight(int type, int k, float pos) const {
		float p = clampf(pos / 127.f, 0.f, 1.f);
		switch (type) {
			case OBJ_STRING: { float x = 0.02f + 0.48f * p; return std::fabs(std::sin((k + 1) * (float) M_PI * x)); }
			case OBJ_BEAM: case OBJ_MARIMBA: {
				float x = 0.5f - 0.5f * p;
				float w = std::fabs(std::cos((k + 2) * (float) M_PI * x));
				if (type == OBJ_MARIMBA && (k == 1 || k == 2)) w *= 0.7079f;
				return w;
			}
			case OBJ_DRUMHEAD: {
				float g = p * (MODE_GRID - 1);
				int i0 = std::min(MODE_GRID - 2, (int) g);
				float f = g - i0;
				return WEIGHT_DRUMHEAD[k * MODE_GRID + i0] * (1.f - f) + WEIGHT_DRUMHEAD[k * MODE_GRID + i0 + 1] * f;
			}
			case OBJ_MEMBRANE: case OBJ_PLATE: {
				float x = 0.05f + 0.45f * p;
				int m = type == OBJ_MEMBRANE ? MN_MEMBRANE_M[k] : MN_PLATE_M[k];
				int n = type == OBJ_MEMBRANE ? MN_MEMBRANE_N[k] : MN_PLATE_N[k];
				return std::fabs(std::sin(m * (float) M_PI * x) * std::sin(n * (float) M_PI * x));
			}
		}
		return 1.f;
	}
	static const float* ratios(int type) {
		switch (type) {
			case OBJ_STRING: return RATIO_STRING;
			case OBJ_BEAM: return RATIO_BEAM;
			case OBJ_MARIMBA: return RATIO_MARIMBA;
			case OBJ_DRUMHEAD: return RATIO_DRUMHEAD;
			case OBJ_MEMBRANE: return RATIO_MEMBRANE;
			default: return RATIO_PLATE;
		}
	}

	float decayFactor(const PlinkIn& in) const {
		float d = clampf(panel.decayKnob + in.decayCv / 5.f, 0.f, 1.f);
		return std::max(0.001f, d * d);
	}

	void updateVoice(PlinkVoice& v, const PlinkIn& in, bool atStrike) {
		if (v.live && !atStrike) {
			currentBase(v, in);
			float rp;
			effective(v.base, in, v.p, rp);
			v.rpitchSemi = rp;
		}
		const float* p = v.p;
		int type = (int) clampf(std::round(p[P_TYPE]), 0.f, OBJ_COUNT - 1.f);
		// pitch
		float knob = clampf(panel.pitchKnob, -1.f, 1.f) * 2.f;
		if (p[P_QUANT] > 0.5f) knob = std::round(knob * 12.f) / 12.f;
		float oct = clampf(std::round(p[P_OCTAVE]), -3.f, 3.f) + knob + v.pitchCv + v.penvOct + v.rpitchSemi / 12.f;
		float f1 = C4_HZ * std::pow(2.f, clampf(oct, -8.f, 7.f));
		// decay, tone, inharmonicity, low cut (§5.5)
		float dfac = decayFactor(in);
		float t60 = physical(P_RDEC, p[P_RDEC]) * dfac;
		if (v.chokeRes) t60 = std::min(t60, CHOKE_T60);
		float alpha = TONE_ALPHA_WOOD + (TONE_ALPHA_METAL - TONE_ALPHA_WOOD) * clampf(p[P_RTONE], 0.f, 127.f) / 127.f;
		float inh = clampf(p[P_RINH], -64.f, 63.f) / 64.f;
		float lc = clampf(p[P_RLOWCUT], 0.f, 127.f);
		float lcRatio = 1.f + (LOWCUT_MAX_RATIO - 1.f) * (lc / 127.f) * (lc / 127.f);
		float lcMix = std::min(1.f, lc / 8.f);
		const float* r = ratios(type);
		int n = std::min(MAX_MODES, set.partials);
		// position weights and their normalisation
		float w[MAX_MODES];
		float e = 0.f;
		for (int k = 0; k < n; k++) { w[k] = modeWeight(type, k, p[P_RPOS]); if (k < 16) e += w[k] * w[k]; }
		float norm = std::min(4.f, std::sqrt(16.f / std::max(e, 1e-3f)) * 0.25f);
		float nyq = NYQ_FRACTION * sr;
		float og = objGain[type];
		int used = 0;
		for (int k = 0; k < n; k++) {
			float rho = std::pow(r[k], 1.f + INHARM_EXP * inh);
			float f = f1 * rho;
			if (f >= nyq) { v.gm[k] = v.gn[k] = 0.f; v.cr[k] = v.ci[k] = 0.f; continue; }
			used = k + 1;
			float edge = clampf((nyq - f) / (0.1f * nyq), 0.f, 1.f);
			float tk = std::max(T60_FLOOR, t60 * std::pow(1.f / rho, alpha));
			float rad = std::exp(-6.9078f / (tk * sr));
			float om = 2.f * (float) M_PI * f / sr;
			v.cr[k] = rad * std::cos(om);
			v.ci[k] = rad * std::sin(om);
			float z = rho / lcRatio;
			float hcut = 1.f + lcMix * (z * z / (1.f + z * z) - 1.f);
			float g = w[k] * norm * hcut * std::pow(rho, -SPECTRAL_TILT) * og * edge;
			v.gm[k] = g;
			v.gn[k] = g * std::sqrt(std::max(1e-6f, 1.f - rad * rad)) * 8.f;
		}
		v.nModes = used;
		// noise filters (envelope applied per block)
		float lpHz = physical(P_LPCUT, p[P_LPCUT]) * std::pow(2.f, physical(P_LPENV, p[P_LPENV]) * v.nenv);
		float hpHz = physical(P_HPCUT, p[P_HPCUT]) * std::pow(2.f, physical(P_HPENV, p[P_HPENV]) * v.nenv);
		v.lp.set(lpHz, physical(P_LPQ, p[P_LPQ]), sr);
		v.hp.set(hpHz, physical(P_HPQ, p[P_HPQ]), sr);
		// noise source and envelope rates
		v.densProb = physical(P_DENS, p[P_DENS]) * dt;
		v.whiteMix = clampf((p[P_DENS] - 120.f) / 7.f, 0.f, 1.f);
		v.atkStep = dt / physical(P_NATK, p[P_NATK]);
		float dec = physical(P_NDEC, p[P_NDEC]) * dfac;
		if (v.chokeNoise) dec = std::min(dec, CHOKE_T60);
		v.decMul = std::exp(-6.9078f * dt / std::max(dec, 0.001f));
		float mixA = clampf(p[P_MIX], 0.f, 127.f) / 127.f * (float) M_PI * 0.5f;
		v.mixM = std::cos(mixA); v.mixN = std::sin(mixA);
	}

	// ---------------------------------------------------------- per-sample voice
	float runVoice(PlinkVoice& v, const PlinkIn& in) {
		const float* p = v.p;
		// pitch follows CV while the gate is high (§5.5)
		if (v.live && v.gate) v.pitchCv = in.pitch[std::min(v.channel, 7)];
		if (v.penvOct != 0.f) {
			float nv = v.penvOct - v.penvStep;
			if ((v.penvStep > 0.f && nv < 0.f) || (v.penvStep < 0.f && nv > 0.f)) nv = 0.f;
			v.penvOct = nv;
		}
		// mallet
		float xm = 0.f;
		if (v.malletN > 0 && v.malletI < v.malletN) {
			float ph = (float) v.malletI / v.malletN;
			xm = v.malletAmp * (0.5f - 0.5f * std::cos(2.f * (float) M_PI * ph));
			v.malletI++;
		}
		v.malletLp += v.malletA * (xm - v.malletLp);
		xm = v.malletLp;
		// noise source (§5.4)
		float xn = 0.f;
		if (v.nstage != 0 || v.nenv > 1e-6f) {
			float dust = 0.f;
			if (rng.uni() < v.densProb) dust = (rng.uni() < 0.5f ? -1.f : 1.f) * (0.5f + 0.5f * rng.uni());
			float src = v.whiteMix > 0.f ? (1.f - v.whiteMix) * dust + v.whiteMix * rng.bi() : dust;
			float lo, hi, lo2, hi2;
			v.hp.process(src, lo, hi);
			v.lp.process(hi, lo2, hi2);
			switch (v.nstage) {
				case 1:
					v.nenv += v.atkStep;
					if (v.nenv >= 1.f) { v.nenv = 1.f; v.nstage = (p[P_ENVTYPE] > 0.5f && v.gate) ? 2 : 3; }
					break;
				case 2: if (!v.gate) v.nstage = 3; break;
				case 3: v.nenv *= v.decMul; if (v.nenv < 1e-5f) { v.nenv = 0.f; v.nstage = 0; } break;
				default: v.nenv *= 0.99f; break;
			}
			xn = lo2 * v.nenv * 0.5f;
		}
		float am = v.mixM * v.velGain, an = v.mixN * v.velGain;
		float em = xm * am, en = xn * an;
		// modal bank
		float acc = 0.f;
		const int n = v.nModes;
		for (int k = 0; k < n; k++) {
			float in0 = v.gm[k] * em + v.gn[k] * en;
			float nr = v.cr[k] * v.re[k] - v.ci[k] * v.im[k] + in0;
			float ni = v.cr[k] * v.im[k] + v.ci[k] * v.re[k];
			v.re[k] = nr; v.im[k] = ni;
			acc += ni;
		}
		float o = (acc + EXCITER_LEAK * (em * 0.5f * (float) v.malletN + en)) * OUT_SCALE * v.volLevel;   // leak: mallet pulse at unit peak
		if (set.velMode == 2 && v.live) v.volLevel = clampf(in.vel[std::min(v.channel, 7)] / 5.f, 0.f, 1.f);
		if (v.fadeStep > 0.f) {
			v.fade -= v.fadeStep;
			if (v.fade <= 0.f) { v.clear(); return 0.f; }
			o *= v.fade;
		}
		// voice free detection (§5.6)
		float a = std::fabs(o);
		v.peak = std::max(a, v.peak * 0.9995f);
		if (v.peak < 1.6e-4f && v.nstage == 0 && v.malletI >= v.malletN) {
			v.quiet += dt;
			if (v.quiet > 0.05f) { v.clear(); return 0.f; }
		} else v.quiet = 0.f;
		v.lastOut = o;
		return o;
	}

	// ---------------------------------------------------------- output stage (§5.10)
	float saturate(float x) const {
		float sat = clampf(panel.satDb + modSum(DEST_SAT, lastIn) * 60.f / 127.f, 0.f, 60.f);
		float G = std::pow(10.f, sat / 20.f);
		float xn = x / 5.f;
		float y = (std::tanh(G * xn + SAT_BIAS) - std::tanh(SAT_BIAS)) / (std::tanh(G + SAT_BIAS) - std::tanh(SAT_BIAS));
		float wet = std::min(1.f, sat / 6.f);
		return 5.f * (xn + wet * (y - xn));
	}
	float crushBits() const {
		float c = panel.crush + modSum(DEST_CRUSH, lastIn) * 10.f / 127.f;
		return clampf(c, 0.f, 10.f);
	}

	PlinkIn lastIn;

	void process(const PlinkIn& in) {
		lastIn = in;
		// MOD special functions (§5.9)
		int md = panel.dest[2];
		bool mh = modHigh ? in.mod > GATE_LOW : in.mod >= GATE_HIGH;
		bool modRise = mh && !modHigh;
		modHigh = mh;
		if (md == DEST_RANDOM && modRise) randomizeRequest = true;
		bool cRes = mh && (md == DEST_CHOKE_BOTH || md == DEST_CHOKE_RES);
		bool cNoise = mh && (md == DEST_CHOKE_BOTH || md == DEST_CHOKE_NOISE);
		// triggers with optional gate delay
		int nch = std::max(1, std::min(8, in.channels));
		int delay = (int) std::lround(set.gateDelay * sr);
		for (int c = 0; c < nch; c++) {
			bool g = in.gate[c];
			if (g && !gatePrev[c]) pending[c] = delay;
			gatePrev[c] = g;
			if (pending[c] == 0) { strike(c, in); pending[c] = -1; }
			else if (pending[c] > 0) pending[c]--;
		}
		// gates held (AHR, pitch follow)
		int nv = std::max(2, maxVoices());
		for (int i = 0; i < nv; i++) {
			PlinkVoice& v = voice[i];
			if (!v.active) continue;
			v.gate = in.gate[std::min(v.channel, 7)] && v.live;
			bool crChange = v.chokeRes != cRes || v.chokeNoise != cNoise;
			v.chokeRes = cRes; v.chokeNoise = cNoise;
			if (crChange) updateVoice(v, in, false);
		}
		if (++blockPos >= CONTROL_BLOCK) {
			blockPos = 0;
			for (int i = 0; i < nv; i++) if (voice[i].active) updateVoice(voice[i], in, false);
		}
		float sum = 0.f;
		for (int i = 0; i < 8; i++) {
			float o = 0.f;
			if (i < nv && voice[i].active) o = runVoice(voice[i], in);
			voiceLevel[i] = voice[i].active ? voice[i].peak : 0.f;
			polyOut[i] = o;
			sum += o;
		}
		// output stage
		float g = std::pow(10.f, clampf(panel.knobs[P_GAIN], -20.f, 40.f) / 20.f);
		float x = sum * g;
		float satAmt = panel.satDb + modSum(DEST_SAT, in) * 60.f / 127.f;
		if (satAmt > 0.01f) x = os.process(x, [this](float u) { return saturate(u); });
		float cb = crushBits();
		if (cb >= 0.5f) {
			int bits = 11 - (int) std::lround(cb);   // 1 -> 10 bit, 10 -> 1 bit
			float q = std::pow(2.f, (float) (bits - 1));
			x = std::round(clampf(x / 5.f, -1.f, 1.f) * q) / q * 5.f;
		}
		float a = std::fabs(x);
		float rel = std::exp(-dt / LIMIT_RELEASE);
		limEnv = a > limEnv ? a : limEnv * rel + (1.f - rel) * a;
		if (limEnv > LIMIT_CEIL) x *= LIMIT_CEIL / limEnv;
		x = clampf(x, -10.f, 10.f);
		if (!std::isfinite(x)) { reset(); x = 0.f; }
		out = x;
		for (int i = 0; i < 8; i++) polyOut[i] = clampf(polyOut[i] * g, -10.f, 10.f);
	}
};

} // namespace tx
