// TETRA four-channel function generator engine (spec §4). Plain C++17, no Rack includes.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "Constants.hpp"

namespace tx {

inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

// ---------------------------------------------------------------- laws (§4.3)
// Knob dead zone: maps the physical knob to 0..1 with a small flat zone at each end.
inline float deadZone(float knob) { return clampf((knob - DEAD_ZONE) / (1.f - 2.f * DEAD_ZONE), 0.f, 1.f); }

// Time law through the manual's three points: 0.05 ms, 50 ms (noon), 20 s.
inline float timeLaw(float p) {
	p = clampf(p, P_LO, P_HI);
	if (p <= 0.5f) return T_MIN * std::pow(T_NOON / T_MIN, 2.f * p);
	return T_NOON * std::pow(T_MAX / T_NOON, 2.f * (p - 0.5f));
}

// Rate law for BURST and LFO: 0.05 Hz to 3.33 kHz.
inline float rateLaw(float p) {
	p = clampf(p, P_LO, P_HI);
	return RATE_MIN * std::pow(RATE_MAX / RATE_MIN, p);
}

// Curve: s in [-1, 1]; 0 linear, >0 exponential (slow start), <0 logarithmic (fast start).
inline float curve(float x, float s) {
	x = clampf(x, 0.f, 1.f);
	if (std::fabs(s) < 1e-4f) return x;
	if (s > 0.f) {
		float k = CURVE_K * s;
		return (std::exp(k * x) - 1.f) / (std::exp(k) - 1.f);
	}
	return 1.f - curve(1.f - x, -s);
}

// Clock ratio table (§4.8), noon = 1:1.
inline float clockRatio(int i) {
	static const float r[CLOCK_RATIOS] = {1.f / 64, 1.f / 32, 1.f / 16, 1.f / 8, 1.f / 6, 1.f / 4, 1.f / 3, 1.f / 2, 1.f,
	                                      2.f, 3.f, 4.f, 6.f, 8.f, 16.f, 32.f, 64.f};
	return r[std::max(0, std::min(CLOCK_RATIOS - 1, i))];
}
inline int ratioIndex(float p) { return (int) std::lround(clampf(p, 0.f, 1.f) * (CLOCK_RATIOS - 1)); }

// Small fast RNG (xorshift32), per channel.
struct Rng {
	uint32_t s = 2463534242u;
	void seed(uint32_t v) { s = v ? v : 1u; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }   // [0, 1)
	float bi() { return uni() * 2.f - 1.f; }                       // [-1, 1)
};

enum Mode { MODE_AD, MODE_AHR, MODE_CYCLE, MODE_BURST, MODE_LFO, MODE_COUNT };
enum Link { LINK_OFF, LINK_TRIG, LINK_EOR, LINK_EOF, LINK_COUNT };

struct ChannelSettings {
	int mode = MODE_AD;
	bool alt = false;
	int link = LINK_OFF;
	bool tenV = true;
	// Positions after CV: knob (0..1, dead zone already applied) + CV/5 * attenuverter.
	float rise = 0.5f, fall = 0.5f, shape = 0.5f;
	float level = 1.f;       // 0..1
};

// ---------------------------------------------------------------- one channel
struct TetraChannel {
	enum Stage { IDLE, RISE, HOLD, FALL };
	float sr = 48000.f, dt = 1.f / 48000.f;
	Stage stage = IDLE;
	float value = 0.f;       // unipolar 0..1 (envelopes, burst) or bipolar -1..1 (LFO)
	float x = 0.f;           // progress in the current stage
	float from = 0.f;        // level at the start of the current stage
	// burst
	bool burstActive = false;
	float burstT = 0.f, pulsePhase = 0.f;
	// LFO
	float phase = 0.f;
	float clockPeriod = 0.f, sinceClock = 1e9f;
	int edgeCount = 0;
	bool clocked = false;
	// LFV
	float lfvPrev = 0.f, lfvTarget = 0.f, lfvLp = 0.f, lfvOut = 0.f, lfvLast = 0.f;
	bool lfvDown = false;
	Rng rng;
	// flags (§6.2)
	bool eor = false, eof = true;
	int lastMode = -1;

	void setRate(float r) { sr = r; dt = 1.f / r; }
	void reset() {
		stage = IDLE; value = 0.f; x = 0.f; from = 0.f;
		burstActive = false; burstT = 0.f; pulsePhase = 0.f;
		phase = 0.f; clockPeriod = 0.f; sinceClock = 1e9f; edgeCount = 0;
		lfvPrev = lfvTarget = lfvLp = lfvOut = lfvLast = 0.f;
		eor = false; eof = true;
	}

	// Inputs per sample: gate level, rising edge, whether a source (jack or link) is present,
	// and a phase reset request (LFO chain reset).
	float process(const ChannelSettings& s, bool gate, bool rise, bool sourcePresent, bool lfoReset) {
		if (s.mode != lastMode) {
			// Mode change: keep the current level for envelopes, restart oscillators.
			if (s.mode == MODE_LFO) { phase = 0.f; lfvPrev = lfvTarget = lfvLp = lfvOut = 0.f; }
			if (lastMode == MODE_LFO) { stage = IDLE; value = 0.f; }
			burstActive = false;
			stage = (s.mode == MODE_CYCLE) ? RISE : IDLE;
			from = std::max(0.f, value); x = 0.f;
			lastMode = s.mode;
		}
		switch (s.mode) {
			case MODE_AD: case MODE_AHR: case MODE_CYCLE: return envelope(s, gate, rise);
			case MODE_BURST: return burst(s, rise, sourcePresent);
			default: return s.alt ? lfv(s, rise, sourcePresent, lfoReset) : lfo(s, rise, sourcePresent, lfoReset);
		}
	}

	float riseCurve(const ChannelSettings& s, float t) const {
		float sh = clampf(s.shape, 0.f, 1.f) * 2.f - 1.f;
		if (!s.alt) return curve(t, sh);
		return curve(t, sh >= 0.f ? std::fabs(sh) : -std::fabs(sh));
	}
	float fallCurve(const ChannelSettings& s, float t) const {
		float sh = clampf(s.shape, 0.f, 1.f) * 2.f - 1.f;
		if (!s.alt) return curve(t, sh);
		return curve(t, sh >= 0.f ? -std::fabs(sh) : std::fabs(sh));
	}

	void startRise() { from = clampf(value, 0.f, 1.f); stage = RISE; x = 0.f; }

	float envelope(const ChannelSettings& s, bool gate, bool rise) {
		float tr = timeLaw(s.rise), tf = timeLaw(s.fall);
		if (rise) startRise();
		if (s.mode == MODE_AHR && !gate && (stage == RISE || stage == HOLD)) {
			from = value; stage = FALL; x = 0.f;
		}
		if (s.mode == MODE_CYCLE && stage == IDLE) startRise();
		switch (stage) {
			case IDLE: value = 0.f; break;
			case RISE: {
				float dur = std::max(dt, tr * (1.f - from));
				x += dt / dur;
				if (x >= 1.f) {
					value = 1.f; x = 0.f; from = 1.f;
					stage = (s.mode == MODE_AHR) ? HOLD : FALL;
				} else value = from + (1.f - from) * riseCurve(s, x);
				break;
			}
			case HOLD: value = 1.f; break;
			case FALL: {
				x += dt / std::max(dt, tf);
				if (x >= 1.f) {
					value = 0.f; x = 0.f;
					if (s.mode == MODE_CYCLE) startRise(); else stage = IDLE;
				} else value = from * (1.f - fallCurve(s, x));
				break;
			}
		}
		// §6.2 flags
		if (s.mode == MODE_AHR) { eor = (stage == HOLD || stage == FALL); eof = (stage != FALL); }
		else { eor = (stage == FALL); eof = !eor; }
		if (s.mode == MODE_CYCLE && stage == IDLE) { eor = false; eof = true; }
		return value;
	}

	float burst(const ChannelSettings& s, bool rise, bool sourcePresent, bool retrigger = false) {
		(void) retrigger;
		float f = std::min(rateLaw(s.rise), 0.45f * sr);
		float L = std::max(BURST_MIN_LEN, timeLaw(s.fall));
		bool start = rise && (!burstActive || burstRetrigger);
		if (start) {
			if (!burstActive) pulsePhase = 0.f;
			burstActive = true; burstT = 0.f;
		}
		if (!burstActive && !sourcePresent) { burstActive = true; burstT = 0.f; pulsePhase = 0.f; } // free-running
		if (!burstActive) { value = 0.f; eor = false; eof = true; return value; }
		int N = std::max(1, (int) std::floor(L * f));
		float P = L / (float) N;
		float u = clampf(burstT / L, 0.f, 1.f);
		float sh = clampf(s.shape, 0.f, 1.f);
		float w;
		float ph = pulsePhase;
		if (!s.alt) {
			if (sh < BURST_SQUARE_ZONE || sh > 1.f - BURST_SQUARE_ZONE) w = ph < 0.5f ? 1.f : 0.f;
			else w = 0.5f - 0.5f * std::cos(2.f * (float) M_PI * ph);
		} else {
			// saw (CCW) -> triangle (noon) -> ramp (CW): peak position = SHAPE
			float pk = sh;
			if (pk <= 1e-4f) w = 1.f - ph;
			else if (pk >= 1.f - 1e-4f) w = ph;
			else w = ph < pk ? ph / pk : (1.f - ph) / (1.f - pk);
		}
		float e = 1.f;
		if (!s.alt) {
			float up = sh;
			if (up <= 1e-4f) e = 1.f - u;
			else if (up >= 1.f - 1e-4f) e = u;
			else e = u < up ? u / up : (1.f - u) / (1.f - up);
		}
		value = clampf(w * e, 0.f, 1.f);
		eor = ph < 0.5f;
		eof = false;
		pulsePhase += dt / P;
		if (pulsePhase >= 1.f) pulsePhase -= std::floor(pulsePhase);
		burstT += dt;
		if (burstT >= L) { burstActive = false; pulsePhase = 0.f; burstT = 0.f; value = 0.f; eof = true; eor = false; }
		return value;
	}
	bool burstRetrigger = false;   // set from the menu (§4.7)

	// Clock handling shared by LFO and LFV. Returns the cycle rate in Hz; may reset phase.
	float lfoRate(const ChannelSettings& s, bool rise, bool sourcePresent, bool lfoReset, bool& resetPhase) {
		resetPhase = lfoReset;
		sinceClock += dt;
		clocked = sourcePresent;
		if (!clocked) { edgeCount = 0; return std::min(rateLaw(s.rise), 0.45f * sr); }
		float ratio = clockRatio(ratioIndex(s.rise));
		if (rise) {
			if (sinceClock < 1e8f && sinceClock > 0.f) clockPeriod = sinceClock;
			sinceClock = 0.f;
			if (ratio >= 1.f) resetPhase = true;
			else {
				int n = (int) std::lround(1.f / ratio);
				if (edgeCount % n == 0) resetPhase = true;
				edgeCount++;
			}
		}
		if (lfoReset) edgeCount = 1;
		if (clockPeriod <= 0.f) return std::min(rateLaw(s.rise), 0.45f * sr);
		return std::min(ratio / clockPeriod, 0.45f * sr);   // keeps running at the last rate if the clock stops
	}

	static float baseShape(int i, float ph) {
		switch (i) {
			case 0: return std::sin(2.f * (float) M_PI * ph);                                  // sine
			case 1: return ph < 0.25f ? 4.f * ph : (ph < 0.75f ? 2.f - 4.f * ph : 4.f * ph - 4.f); // triangle
			case 2: return ph < 0.5f ? 2.f * ph : 2.f * ph - 2.f;                               // ramp up
			case 3: return ph < 0.5f ? 1.f : -1.f;                                               // square
			default: return ph < 0.5f ? -2.f * ph : 2.f - 2.f * ph;                             // ramp down
		}
	}

	float lfo(const ChannelSettings& s, bool rise, bool sourcePresent, bool lfoReset) {
		bool rp;
		float f = lfoRate(s, rise, sourcePresent, lfoReset, rp);
		if (rp) phase = 0.f;
		// MORPH: phase warp (§4.8)
		float m = clampf(s.fall, 0.02f, 0.98f);
		float pw = phase < m ? phase * 0.5f / m : 0.5f + (phase - m) * 0.5f / (1.f - m);
		float sp = clampf(s.shape, 0.f, 1.f) * 4.f;
		int i0 = std::min(3, (int) sp);
		float fr = sp - i0;
		value = (1.f - fr) * baseShape(i0, pw) + fr * baseShape(i0 + 1, pw);
		eor = phase >= 0.5f;
		eof = phase < 0.5f;
		phase += f * dt;
		if (phase >= 1.f) phase -= std::floor(phase);
		return value;
	}

	void lfvNewTarget(const ChannelSettings& s) {
		float v = clampf(s.fall, 0.f, 1.f);
		lfvPrev = lfvTarget;
		if (v > 0.5f) lfvTarget = rng.bi() * (v - 0.5f) * 2.f;
		else if (v < 0.5f) {
			lfvTarget += rng.bi() * (0.5f - v) * 2.f * LFV_DRUNK_STEP;
			if (lfvTarget > 1.f) lfvTarget = 2.f - lfvTarget;
			if (lfvTarget < -1.f) lfvTarget = -2.f - lfvTarget;
		}
		lfvTarget = clampf(lfvTarget, -1.f, 1.f);
	}

	float lfv(const ChannelSettings& s, bool rise, bool sourcePresent, bool lfoReset) {
		bool rp;
		float f = lfoRate(s, rise, sourcePresent, lfoReset, rp);
		if (rp) { phase = 0.f; lfvNewTarget(s); }
		float lin = lfvPrev + (lfvTarget - lfvPrev) * phase;
		float cosi = lfvPrev + (lfvTarget - lfvPrev) * (0.5f - 0.5f * std::cos((float) M_PI * phase));
		float sl = clampf(s.shape, 0.f, 1.f);
		float raw = (1.f - sl) * lin + sl * cosi;
		float fc = 0.5f * f;
		float a = 1.f - std::exp(-2.f * (float) M_PI * fc * dt);
		lfvLp += a * (raw - lfvLp);
		value = (1.f - sl) * raw + sl * lfvLp;
		float d = value - lfvLast;
		if (d < -1e-7f) lfvDown = true; else if (d > 1e-7f) lfvDown = false;
		lfvLast = value;
		eor = lfvDown;
		eof = !lfvDown;
		phase += f * dt;
		if (phase >= 1.f) { phase -= std::floor(phase); lfvNewTarget(s); }
		return value;
	}
};

// ---------------------------------------------------------------- engine (§4.10)
struct TetraIn {
	bool gate[4] = {};        // own TRIG jack (or poly channel) OR FIRE, already Schmitt-triggered
	bool patched[4] = {};     // TRIG (or POLY TRIG channel) connected
};

struct TetraEngine {
	TetraChannel ch[4];
	ChannelSettings set[4];
	float out[4] = {};        // volts
	bool effGate[4] = {}, prevEffGate[4] = {};
	bool ownPrev[4] = {};
	bool eorPrev[4] = {}, eofPrev[4] = {true, true, true, true};
	float eorPulse[4] = {}, eofPulse[4] = {};   // remaining seconds of link pulse
	bool fired[4] = {};       // effective rising edge this sample (for LEDs)
	float sr = 48000.f;

	TetraEngine() { for (int i = 0; i < 4; i++) ch[i].rng.seed(0x9E3779B9u * (i + 1)); }
	void setRate(float r) { sr = r; for (auto& c : ch) c.setRate(r); }
	void reset() { for (auto& c : ch) c.reset(); for (int i = 0; i < 4; i++) { effGate[i] = prevEffGate[i] = ownPrev[i] = false; eorPulse[i] = eofPulse[i] = 0.f; } }
	void setBurstRetrigger(bool on) { for (auto& c : ch) c.burstRetrigger = on; }

	static int prevIdx(int c) { return (c + 3) & 3; }

	void process(const TetraIn& in) {
		float dt = 1.f / sr;
		// Own rising edges and LFO chain resets (§4.8): a reset into channel c also resets
		// every earlier channel linked into it.
		bool ownRise[4], chainReset[4] = {};
		for (int c = 0; c < 4; c++) { ownRise[c] = in.gate[c] && !ownPrev[c]; ownPrev[c] = in.gate[c]; }
		for (int c = 0; c < 4; c++) {
			if (!ownRise[c]) continue;
			int j = c;
			for (int n = 0; n < 3 && set[j].link != LINK_OFF; n++) { j = prevIdx(j); chainReset[j] = true; }
		}
		bool newEff[4];
		for (int c = 0; c < 4; c++) {
			int p = prevIdx(c);
			// Channel 1 reads channel 4's state from the previous sample (ring, §4.10).
			bool srcGate = (c == 0) ? prevEffGate[p] : effGate[p];
			bool linkGate = false;
			switch (set[c].link) {
				case LINK_TRIG: linkGate = srcGate; break;
				case LINK_EOR: linkGate = eorPulse[p] > 0.f; break;
				case LINK_EOF: linkGate = eofPulse[p] > 0.f; break;
				default: break;
			}
			newEff[c] = in.gate[c] || linkGate;
			bool rise = newEff[c] && !effGate[c];
			bool src = in.patched[c] || set[c].link != LINK_OFF;
			fired[c] = rise;
			effGate[c] = newEff[c];
			float v = ch[c].process(set[c], newEff[c], rise, src, chainReset[c]);
			// Link pulses from this channel's stage flags.
			bool eorRise = ch[c].eor && !eorPrev[c], eofRise = ch[c].eof && !eofPrev[c];
			eorPrev[c] = ch[c].eor; eofPrev[c] = ch[c].eof;
			eorPulse[c] = eorRise ? LINK_PULSE : std::max(0.f, eorPulse[c] - dt);
			eofPulse[c] = eofRise ? LINK_PULSE : std::max(0.f, eofPulse[c] - dt);
			float lvl = clampf(set[c].level, 0.f, 1.f);
			if (set[c].mode == MODE_LFO) out[c] = v * 5.f * lvl;
			else out[c] = v * (set[c].tenV ? 10.f : 5.f) * lvl;
		}
		for (int c = 0; c < 4; c++) prevEffGate[c] = effGate[c];
	}
};

} // namespace tx
