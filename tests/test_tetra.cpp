// TETRA behaviour tests (spec §4.12, §6.3).
#include "check.hpp"
#include "../src/dsp/Tetra.hpp"

using namespace tx;
static const float SR = 48000.f;

static TetraEngine makeEngine() { TetraEngine e; e.setRate(SR); return e; }

// Run n samples with channel gates from a function of the sample index.
template <class F>
static void run(TetraEngine& e, int n, F gates, std::vector<float>* rec = nullptr, int ch = 0) {
	for (int i = 0; i < n; i++) {
		TetraIn in;
		gates(i, in);
		e.process(in);
		if (rec) rec->push_back(e.out[ch]);
	}
}


TEST("tetra: time law endpoints") {
	CHECK_NEAR(timeLaw(0.f), 0.00005, 0.00005 * 0.01);
	CHECK_NEAR(timeLaw(0.5f), 0.05, 0.05 * 0.01);
	CHECK_NEAR(timeLaw(1.f), 20.0, 20.0 * 0.01);
	CHECK_NEAR(rateLaw(0.f), 0.05, 0.0005);
	CHECK_NEAR(rateLaw(1.f), 3333.0, 33.0);
}

TEST("tetra: AD at noon peaks at 50 ms and ends at 100 ms") {
	auto e = makeEngine();
	e.set[0].mode = MODE_AD; e.set[0].rise = 0.5f; e.set[0].fall = 0.5f; e.set[0].shape = 0.5f;
	std::vector<float> rec;
	run(e, (int) (0.15f * SR), [](int i, TetraIn& in) { in.gate[0] = i < 10; in.patched[0] = true; }, &rec);
	int peak = (int) (std::max_element(rec.begin(), rec.end()) - rec.begin());
	CHECK_NEAR(rec[peak], 10.0, 0.01);
	CHECK_NEAR(peak / SR, 0.05, 0.0005);
	int end = -1;
	for (size_t i = peak; i < rec.size(); i++) if (rec[i] <= 0.f) { end = (int) i; break; }
	CHECK_NEAR(end / SR, 0.1, 0.001);
}

TEST("tetra: SHAPE noon is linear, CW below the line, CCW above") {
	for (int k = 1; k < 10; k++) {
		float x = k / 10.f;
		CHECK_NEAR(curve(x, 0.f), x, 1e-6);
		CHECK(curve(x, 1.f) < x);
		CHECK(curve(x, -1.f) > x);
	}
}

TEST("tetra: AHR holds while the gate is high, short gate falls early") {
	auto e = makeEngine();
	e.set[0].mode = MODE_AHR; e.set[0].rise = 0.3f; e.set[0].fall = 0.3f;
	std::vector<float> rec;
	int gateLen = (int) SR;
	run(e, (int) (1.2f * SR), [&](int i, TetraIn& in) { in.gate[0] = i < gateLen; in.patched[0] = true; }, &rec);
	CHECK_NEAR(rec[(int) (0.5f * SR)], 10.0, 1e-3);
	CHECK_NEAR(rec[gateLen - 2], 10.0, 1e-3);
	CHECK(rec[gateLen + (int) (0.002f * SR)] < 10.f);
	// 5 ms gate, 50 ms rise: peaks below max, then falls
	auto e2 = makeEngine();
	e2.set[0].mode = MODE_AHR; e2.set[0].rise = 0.5f; e2.set[0].fall = 0.5f;
	std::vector<float> r2;
	run(e2, (int) (0.2f * SR), [](int i, TetraIn& in) { in.gate[0] = i < 240; in.patched[0] = true; }, &r2);
	float mx = *std::max_element(r2.begin(), r2.end());
	CHECK(mx < 5.f);
	CHECK(r2.back() < 0.01f);
}

TEST("tetra: CYCLE period is rise + fall, trigger restarts the rise") {
	auto e = makeEngine();
	e.set[0].mode = MODE_CYCLE; e.set[0].rise = 0.5f; e.set[0].fall = 0.5f;
	std::vector<float> rec;
	run(e, (int) (0.35f * SR), [](int, TetraIn&) {}, &rec);
	// peaks at 50, 150, 250 ms
	std::vector<int> peaks;
	for (size_t i = 1; i + 1 < rec.size(); i++) if (rec[i] >= 9.999f && rec[i - 1] < 9.999f) peaks.push_back((int) i);
	CHECK(peaks.size() >= 3);
	if (peaks.size() >= 3) CHECK_NEAR((peaks[2] - peaks[1]) / SR, 0.1, 0.001);
	// trigger mid-fall restarts the rise
	auto e2 = makeEngine();
	e2.set[0].mode = MODE_CYCLE; e2.set[0].rise = 0.5f; e2.set[0].fall = 0.5f;
	std::vector<float> r2;
	int trig = (int) (0.075f * SR);
	run(e2, (int) (0.09f * SR), [&](int i, TetraIn& in) { in.gate[0] = i >= trig && i < trig + 10; in.patched[0] = true; }, &r2);
	CHECK(r2[trig + 200] > r2[trig]);
}

TEST("tetra: BURST pulse count, tilt and retrigger") {
	auto e = makeEngine();
	e.set[0].mode = MODE_BURST;
	// rate 100 Hz, length 100 ms
	float pr = std::log(100.f / RATE_MIN) / std::log(RATE_MAX / RATE_MIN);
	e.set[0].rise = pr; e.set[0].fall = 0.5f + 0.5f * std::log(0.1f / T_NOON) / std::log(T_MAX / T_NOON);
	e.set[0].shape = 0.5f;
	std::vector<float> rec;
	std::vector<bool> eorRec;
	for (int i = 0; i < (int) (0.12f * SR); i++) {
		TetraIn in; in.gate[0] = i < 10; in.patched[0] = true;
		e.process(in); rec.push_back(e.out[0]); eorRec.push_back(e.ch[0].eor && e.ch[0].burstActive);
	}
	int pulses = 0;
	for (size_t i = 1; i < eorRec.size(); i++) if (eorRec[i] && !eorRec[i - 1]) pulses++;
	if (eorRec[0]) pulses++;
	CHECK(pulses == 10);
	// noon tilt: biggest pulse near the middle
	int peak = (int) (std::max_element(rec.begin(), rec.end()) - rec.begin());
	CHECK_NEAR(peak / SR, 0.05, 0.011);
	// retrigger OFF ignores a trigger at 30 ms; ON restarts the envelope
	for (int on = 0; on < 2; on++) {
		auto b = makeEngine();
		b.setBurstRetrigger(on);
		b.set[0] = e.set[0];
		int t2 = (int) (0.03f * SR);
		float uAt = 0.f;
		for (int i = 0; i < t2 + 50; i++) {
			TetraIn in; in.patched[0] = true;
			in.gate[0] = i < 10 || (i >= t2 && i < t2 + 10);
			b.process(in);
			uAt = b.ch[0].burstT;
		}
		if (on) CHECK(uAt < 0.002f); else CHECK(uAt > 0.03f);
	}
}

TEST("tetra: LFO free rate and clock sync") {
	auto e = makeEngine();
	e.set[0].mode = MODE_LFO; e.set[0].rise = 1.f; e.set[0].shape = 0.f;
	// free at p = 1 -> ~3.33 kHz; count zero crossings over 0.1 s
	std::vector<float> rec;
	run(e, (int) (0.1f * SR), [](int, TetraIn&) {}, &rec);
	int ups = 0;
	for (size_t i = 1; i < rec.size(); i++) if (rec[i - 1] < 0.f && rec[i] >= 0.f) ups++;
	CHECK(ups > 320 && ups < 340);
	// clocked at noon: follows a 2 Hz clock, phase reset on each edge
	auto c = makeEngine();
	c.set[0].mode = MODE_LFO; c.set[0].rise = 0.5f; c.set[0].shape = 0.f; c.set[0].fall = 0.5f;
	int per = (int) (0.5f * SR);
	for (int i = 0; i < 3 * per + 5; i++) {
		TetraIn in; in.patched[0] = true; in.gate[0] = (i % per) < 10;
		c.process(in);
	}
	CHECK_NEAR(c.ch[0].clockPeriod, 0.5, 0.001);
	CHECK(c.ch[0].phase < 0.001f);
	// ratio 1/4 resets every 4th edge
	auto q = makeEngine();
	q.set[0].mode = MODE_LFO; q.set[0].rise = 5.f / 16.f; q.set[0].shape = 0.f;
	CHECK_NEAR(clockRatio(ratioIndex(5.f / 16.f)), 0.25, 1e-6);
	int resets = 0;
	float lastPh = 0.f;
	for (int i = 0; i < 9 * per; i++) {
		TetraIn in; in.patched[0] = true; in.gate[0] = (i % per) < 10;
		q.process(in);
		if (q.ch[0].phase < lastPh - 0.5f || (q.ch[0].phase < 1e-6f && lastPh > 1e-6f)) resets++;
		lastPh = q.ch[0].phase;
	}
	CHECK(resets >= 2 && resets <= 4);
}

TEST("tetra: LFV stays in range and is centred with variance CW") {
	auto e = makeEngine();
	e.set[0].mode = MODE_LFO; e.set[0].alt = true; e.set[0].rise = 0.6f; e.set[0].fall = 1.f; e.set[0].shape = 0.3f;
	double sum = 0; float mn = 0, mx = 0;
	int n = (int) (600.f * 2000.f); // 10 minutes at a 2 kHz control rate
	e.setRate(2000.f);
	for (int i = 0; i < n; i++) {
		TetraIn in; e.process(in);
		sum += e.out[0]; mn = std::min(mn, e.out[0]); mx = std::max(mx, e.out[0]);
	}
	CHECK(mn >= -5.0001f && mx <= 5.0001f);
	CHECK_NEAR(sum / n, 0.0, 0.3);
	// random settings never leave the range
	Rng r; r.seed(7);
	auto f = makeEngine();
	f.set[1].mode = MODE_LFO; f.set[1].alt = true;
	for (int i = 0; i < 200000; i++) {
		if (i % 1000 == 0) { f.set[1].rise = r.uni(); f.set[1].fall = r.uni(); f.set[1].shape = r.uni(); }
		TetraIn in; f.process(in);
		CHECK(std::fabs(f.out[1]) <= 5.0001f);
		if (std::fabs(f.out[1]) > 5.0001f) break;
	}
}

TEST("tetra: links, ring and TRIG-link gate length") {
	// ch1 AD, ch2 EOF link: ch2 starts when ch1 ends
	auto e = makeEngine();
	for (int c = 0; c < 2; c++) { e.set[c].mode = MODE_AD; e.set[c].rise = 0.4f; e.set[c].fall = 0.4f; }
	e.set[1].link = LINK_EOF;
	int ch1End = -1, ch2Start = -1;
	for (int i = 0; i < (int) (0.2f * SR); i++) {
		TetraIn in; in.patched[0] = true; in.gate[0] = i >= 100 && i < 110;
		e.process(in);
		if (i > 110 && ch1End < 0 && e.ch[0].stage == TetraChannel::IDLE && e.out[0] == 0.f && e.ch[0].eof) ch1End = i;
		if (ch2Start < 0 && e.out[1] > 0.f) ch2Start = i;
	}
	CHECK(ch1End > 0 && ch2Start > 0);
	CHECK(std::abs(ch2Start - ch1End) <= 2);
	// ring of four AD with EOF links self-runs after one FIRE
	auto r = makeEngine();
	for (int c = 0; c < 4; c++) { r.set[c].mode = MODE_AD; r.set[c].rise = 0.35f; r.set[c].fall = 0.35f; r.set[c].link = LINK_EOF; }
	int fires[4] = {};
	for (int i = 0; i < (int) (1.f * SR); i++) {
		TetraIn in; in.gate[0] = i >= 10 && i < 20;
		r.process(in);
		for (int c = 0; c < 4; c++) fires[c] += r.fired[c];
	}
	for (int c = 0; c < 4; c++) CHECK(fires[c] >= 3);
	// TRIG link passes the gate length to an AHR
	auto t = makeEngine();
	t.set[0].mode = MODE_AHR; t.set[1].mode = MODE_AHR; t.set[1].link = LINK_TRIG;
	t.set[1].rise = 0.2f; t.set[1].fall = 0.2f;
	int g = (int) (0.3f * SR);
	float at = 0.f, after = 0.f;
	for (int i = 0; i < g + (int) (0.05f * SR); i++) {
		TetraIn in; in.patched[0] = true; in.gate[0] = i < g;
		t.process(in);
		if (i == g - 5) at = t.out[1];
		if (i == g + (int) (0.04f * SR)) after = t.out[1];
	}
	CHECK_NEAR(at, 10.0, 1e-3);
	CHECK(after < 1.f);
}

TEST("tetra: linked LFO reset walks back the chain") {
	auto e = makeEngine();
	for (int c = 0; c < 2; c++) { e.set[c].mode = MODE_LFO; e.set[c].rise = 0.3f; }
	e.set[1].link = LINK_TRIG;
	for (int i = 0; i < 5000; i++) { TetraIn in; e.process(in); }
	CHECK(e.ch[0].phase > 0.01f);
	TetraIn in; in.gate[1] = true; in.patched[1] = true;
	e.process(in);
	CHECK(e.ch[0].phase < 0.01f);
	CHECK(e.ch[1].phase < 0.01f);
}

TEST("tetra: CV extends the time range") {
	// p = 0.5 + 5 V * 1.0 / 5 = 1.5 -> clamped to 1.15
	CHECK_NEAR(timeLaw(1.5f), 20.0 * std::pow(400.0, 0.3), 20.0 * std::pow(400.0, 0.3) * 0.01);
}

TEST("tetra: times hold across sample rates") {
	for (float sr : {44100.f, 96000.f}) {
		TetraEngine e; e.setRate(sr);
		e.set[0].mode = MODE_AD; e.set[0].rise = 0.5f; e.set[0].fall = 0.5f;
		int peak = 0; float mx = -1;
		for (int i = 0; i < (int) (0.12f * sr); i++) {
			TetraIn in; in.patched[0] = true; in.gate[0] = i < 10; e.process(in);
			if (e.out[0] > mx) { mx = e.out[0]; peak = i; }
		}
		CHECK_NEAR(peak / sr, 0.05, 0.0005);
	}
}

TEST("tetra: TETRA X gate table") {
	// AD: idle EOR low, EOF high; during rise EOR low EOF high; during fall EOR high EOF low
	auto e = makeEngine();
	e.set[0].mode = MODE_AD; e.set[0].rise = 0.5f; e.set[0].fall = 0.5f;
	TetraIn idle; idle.patched[0] = true;
	e.process(idle);
	CHECK(!e.ch[0].eor && e.ch[0].eof);
	TetraIn g = idle; g.gate[0] = true;
	e.process(g);
	for (int i = 0; i < 1000; i++) e.process(idle);
	CHECK(!e.ch[0].eor && e.ch[0].eof);
	for (int i = 0; i < 2000; i++) e.process(idle);
	CHECK(e.ch[0].eor && !e.ch[0].eof);
	// AHR: during hold EOR high, EOF high; during release EOF low
	auto a = makeEngine();
	a.set[0].mode = MODE_AHR; a.set[0].rise = 0.2f; a.set[0].fall = 0.5f;
	for (int i = 0; i < 2000; i++) a.process(g);
	CHECK(a.ch[0].eor && a.ch[0].eof);
	for (int i = 0; i < 100; i++) a.process(idle);
	CHECK(a.ch[0].eor && !a.ch[0].eof);
	// LFO: EOF high first half, EOR high second half
	auto l = makeEngine();
	l.set[0].mode = MODE_LFO; l.set[0].rise = 0.5f;
	TetraIn none;
	l.process(none);
	CHECK(!l.ch[0].eor && l.ch[0].eof);
	while (l.ch[0].phase < 0.6f) l.process(none);
	l.process(none);
	CHECK(l.ch[0].eor && !l.ch[0].eof);
	// BURST: EOF low during the burst, high after
	auto b = makeEngine();
	b.set[0].mode = MODE_BURST; b.set[0].rise = 0.5f; b.set[0].fall = 0.4f;
	b.process(g);
	CHECK(!b.ch[0].eof);
	for (int i = 0; i < (int) (0.1f * SR); i++) b.process(idle);
	CHECK(b.ch[0].eof);
}
