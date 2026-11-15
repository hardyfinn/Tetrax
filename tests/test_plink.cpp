// PLINK behaviour tests (spec §5.15) and the factory render test (§5.14).
#include "check.hpp"
#include "../src/dsp/FactoryPresets.hpp"
#include <cstdio>
#include <memory>

using namespace tx;
static const float SR = 48000.f;

static std::unique_ptr<PlinkEngine> makePlink() {
	auto e = std::make_unique<PlinkEngine>();
	e->setRate(SR);
	loadFactoryBank(e->bank);
	return e;
}

static void hit(PlinkEngine& e, PlinkIn in = PlinkIn()) {
	in.gate[0] = true; e.process(in);
	in.gate[0] = false; e.process(in);
}

static float partialHz(const PlinkVoice& v, int k) { return std::atan2(v.ci[k], v.cr[k]) * SR / (2.f * (float) M_PI); }
static float partialT60(const PlinkVoice& v, int k) {
	float r = std::sqrt(v.cr[k] * v.cr[k] + v.ci[k] * v.ci[k]);
	return -6.9078f / (std::log(r) * SR);
}
static int newestVoice(const PlinkEngine& e) {
	int b = -1; uint64_t a = 0;
	for (int i = 0; i < 8; i++) if (e.voice[i].active && e.voice[i].age >= a) { a = e.voice[i].age; b = i; }
	return b;
}

// Goertzel magnitude of x at frequency f.
static double goertzel(const std::vector<float>& x, double f) {
	double w = 2 * M_PI * f / SR, c = 2 * std::cos(w), s1 = 0, s2 = 0;
	size_t n = x.size();
	for (size_t i = 0; i < n; i++) {
		double win = 0.5 - 0.5 * std::cos(2 * M_PI * i / (n - 1));   // Hann window against leakage
		double s = x[i] * win + c * s1 - s2; s2 = s1; s1 = s;
	}
	return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2);
}

TEST("plink: string struck in the middle loses its even harmonics") {
	auto e = makePlink();
	e->panel.knobs[P_TYPE] = OBJ_STRING; e->panel.knobs[P_RPOS] = 127; e->panel.knobs[P_RDEC] = 90;
	e->panel.knobs[P_OCTAVE] = -1; e->panel.knobs[P_RTONE] = 100;
	std::vector<float> rec;
	PlinkIn in;
	// skip the first 20 ms so the strike's direct click is not counted
	for (int i = 0; i < (int) (0.22f * SR); i++) { in.gate[0] = i < 100; e->process(in); if (i >= (int) (0.02f * SR)) rec.push_back(e->out); }
	double f1 = C4_HZ / 2;
	double odd = goertzel(rec, f1), even = goertzel(rec, 2 * f1), odd3 = goertzel(rec, 3 * f1), even4 = goertzel(rec, 4 * f1);
	INFO("odd %g even %g odd3 %g even4 %g\n", odd, even, odd3, even4);
	CHECK(20 * std::log10(odd / even) > 40);
	CHECK(20 * std::log10(odd3 / even4) > 40);
}

TEST("plink: drumhead partials follow the Bessel ratios") {
	auto e = makePlink();
	e->panel.knobs[P_TYPE] = OBJ_DRUMHEAD; e->panel.knobs[P_OCTAVE] = 0; e->panel.pitchKnob = std::log2(200.f / C4_HZ) / 2.f;
	hit(*e);
	const PlinkVoice& v = e->voice[newestVoice(*e)];
	float f1 = partialHz(v, 0);
	CHECK_NEAR(f1, 200.0, 0.2);
	const float expect[6] = {1.f, 1.593f, 2.136f, 2.295f, 2.653f, 2.917f};
	for (int k = 0; k < 6; k++) CHECK_NEAR(partialHz(v, k) / f1, expect[k], expect[k] * 0.001);
}

TEST("plink: inharmonicity spreads and compresses partials") {
	float r5[3];
	int i = 0;
	for (float inh : {-64.f, 0.f, 63.f}) {
		auto e = makePlink();
		e->panel.knobs[P_TYPE] = OBJ_STRING; e->panel.knobs[P_RINH] = inh; e->panel.knobs[P_OCTAVE] = -2;
		hit(*e);
		const PlinkVoice& v = e->voice[newestVoice(*e)];
		r5[i++] = partialHz(v, 4) / partialHz(v, 0);
	}
	CHECK(r5[0] < r5[1] && r5[1] < r5[2]);
	CHECK_NEAR(r5[1], 5.0, 0.005);
}

TEST("plink: tone sets how fast high partials die") {
	float t10[2];
	int i = 0;
	for (float tone : {0.f, 127.f}) {
		auto e = makePlink();
		e->panel.knobs[P_TYPE] = OBJ_STRING; e->panel.knobs[P_RTONE] = tone; e->panel.knobs[P_OCTAVE] = -2;
		hit(*e);
		t10[i++] = partialT60(e->voice[newestVoice(*e)], 9);
	}
	CHECK(t10[1] / t10[0] >= 10.f);
}

TEST("plink: low cut attenuates the fundamental") {
	float g[2];
	int i = 0;
	for (float lc : {0.f, 127.f}) {
		auto e = makePlink();
		e->panel.knobs[P_TYPE] = OBJ_BEAM; e->panel.knobs[P_RLOWCUT] = lc;
		hit(*e);
		g[i++] = e->voice[newestVoice(*e)].gm[0];
	}
	CHECK(20 * std::log10(g[0] / g[1]) >= 20.f);
}

TEST("plink: pitch CV is latched while the gate is low") {
	auto e = makePlink();
	PlinkIn in;
	in.gate[0] = true; e->process(in);
	in.gate[0] = false;
	for (int i = 0; i < 64; i++) e->process(in);
	int vi = newestVoice(*e);
	float f0 = partialHz(e->voice[vi], 0);
	in.pitch[0] = 1.f;
	for (int i = 0; i < 64; i++) e->process(in);
	CHECK_NEAR(partialHz(e->voice[vi], 0), f0, 0.01);
	// with the gate held, the pitch follows
	auto h = makePlink();
	PlinkIn g; g.gate[0] = true;
	for (int i = 0; i < 64; i++) h->process(g);
	int hv = newestVoice(*h);
	float h0 = partialHz(h->voice[hv], 0);
	g.pitch[0] = 1.f;
	for (int i = 0; i < 64; i++) h->process(g);
	CHECK_NEAR(partialHz(h->voice[hv], 0) / h0, 2.0, 0.002);
}

TEST("plink: 1 V/oct tracking within 1 cent") {
	for (float v = -3.f; v <= 3.f; v += 1.f) {
		auto e = makePlink();
		e->panel.knobs[P_OCTAVE] = 0; e->panel.knobs[P_PENVAMT] = 0;
		PlinkIn in; in.pitch[0] = v;
		hit(*e, in);
		float f = partialHz(e->voice[newestVoice(*e)], 0);
		double cents = 1200.0 * std::log2(f / (C4_HZ * std::pow(2.0, v)));
		CHECK(std::fabs(cents) < 1.0);
	}
}

TEST("plink: pitch envelope starts an octave up and glides linearly") {
	auto e = makePlink();
	e->panel.knobs[P_OCTAVE] = 0; e->panel.knobs[P_PENVAMT] = 63; e->panel.knobs[P_PENVDEC] = 64;
	float T = physical(P_PENVDEC, 64);
	PlinkIn in; in.gate[0] = true; e->process(in);
	int vi = newestVoice(*e);
	CHECK_NEAR(partialHz(e->voice[vi], 0) / C4_HZ, std::pow(2.0, 63.0 / 64.0), 0.01);
	in.gate[0] = false;
	int half = (int) (T * SR / 2);
	for (int i = 0; i < half; i++) e->process(in);
	CHECK_NEAR(e->voice[vi].penvOct, 63.0 / 64.0 / 2, 0.02);
	for (int i = 0; i < half + 100; i++) e->process(in);
	CHECK_NEAR(e->voice[vi].penvOct, 0.0, 1e-6);
}

TEST("plink: two voices steal the oldest; one voice chokes") {
	auto e = makePlink();
	e->panel.knobs[P_RDEC] = 120;
	PlinkIn in;
	hit(*e, in); int a = newestVoice(*e);
	hit(*e, in); int b = newestVoice(*e);
	CHECK(a != b);
	uint64_t ageA = e->voice[a].age;
	hit(*e, in); int c = newestVoice(*e);
	CHECK(c == a);
	CHECK(e->voice[c].age > ageA);
	auto m = makePlink();
	m->set.voices = 1;
	m->panel.knobs[P_RDEC] = 120;
	hit(*m, in); int x = newestVoice(*m);
	hit(*m, in);
	for (int i = 0; i < (int) (0.005f * SR); i++) m->process(in);
	CHECK(!m->voice[x].active);
}

TEST("plink: preset step picks A, B and the middle") {
	auto e = makePlink();
	e->panel.dest[2] = DEST_PRESET_STEP; e->panel.presetA = 0; e->panel.presetB = 2;
	for (float mv : {0.f, 2.5f, 5.f}) {
		PlinkIn in; in.mod = mv;
		hit(*e, in);
		int expect = mv == 0.f ? 0 : (mv == 2.5f ? 1 : 2);
		CHECK(e->lastStepSlot == expect);
		CHECK_NEAR(e->voice[newestVoice(*e)].base[P_RDEC], e->bank[expect].v[P_RDEC], 1e-6);
	}
}

TEST("plink: morph crossfades parameters but not the object type") {
	auto e = makePlink();
	e->panel.dest[2] = DEST_MORPH; e->panel.morphTarget = 25;   // Bell
	e->panel.knobs[P_TYPE] = OBJ_STRING;
	const Preset& t = e->bank[25];
	for (float mv : {0.f, 2.5f, 5.f}) {
		PlinkIn in; in.mod = mv;
		hit(*e, in);
		const PlinkVoice& v = e->voice[newestVoice(*e)];
		float m = mv / 5.f;
		CHECK_NEAR(v.base[P_RTONE], e->panel.knobs[P_RTONE] + m * (t.v[P_RTONE] - e->panel.knobs[P_RTONE]), 1e-4);
		CHECK(v.base[P_TYPE] == OBJ_STRING);
	}
}

TEST("plink: randomize keeps polyphony, saturation, crusher and ranges") {
	Rng r; r.seed(99);
	for (int n = 0; n < 200; n++) {
		float v[NP];
		for (int i = 0; i < NP; i++) v[i] = paramInfo(i).def;
		v[P_OCTAVE] = 2; v[P_GAIN] = 7;
		randomizeSound(v, r);
		for (int i = 0; i < NP; i++) CHECK(v[i] >= paramInfo(i).lo && v[i] <= paramInfo(i).hi);
		CHECK(v[P_OCTAVE] == 2 && v[P_GAIN] == 7);
	}
	auto e = makePlink();
	e->panel.dest[2] = DEST_RANDOM;
	PlinkIn in; in.mod = 5.f; e->process(in);
	CHECK(e->randomizeRequest);
}

TEST("plink: depth units") {
	auto e = makePlink();
	PlinkIn in;
	e->panel.dest[0] = DEST_PARAM0 + P_RTONE; e->panel.depth[0] = DEPTH_LOW;
	in.x = 5.f;
	CHECK_NEAR(e->modSum(DEST_PARAM0 + P_RTONE, in), 16.0, 1e-5);
	e->panel.depth[0] = DEPTH_FULL;
	CHECK_NEAR(e->modSum(DEST_PARAM0 + P_RTONE, in), 128.0, 1e-5);
	float p[NP], rp;
	e->effective(e->panel.knobs, in, p, rp);
	CHECK(p[P_RTONE] == 127.f);
}

TEST("plink: AHR noise envelope holds while the gate is high") {
	auto e = makePlink();
	e->panel.knobs[P_MIX] = 127; e->panel.knobs[P_ENVTYPE] = 1; e->panel.knobs[P_NATK] = 10; e->panel.knobs[P_NDEC] = 20;
	PlinkIn in; in.gate[0] = true;
	for (int i = 0; i < (int) (0.3f * SR); i++) e->process(in);
	int vi = newestVoice(*e);
	CHECK_NEAR(e->voice[vi].nenv, 1.0, 1e-6);
	in.gate[0] = false;
	for (int i = 0; i < (int) (0.05f * SR); i++) e->process(in);
	CHECK(e->voice[vi].nenv < 0.5f);
}

TEST("plink: output stays within 10 V at maximum drive with 8 voices") {
	auto e = makePlink();
	e->set.voices = 8;
	e->panel.knobs[P_GAIN] = 40; e->panel.satDb = 60; e->panel.knobs[P_RDEC] = 127;
	float mx = 0;
	PlinkIn in; in.channels = 8;
	for (int i = 0; i < (int) (0.5f * SR); i++) {
		for (int c = 0; c < 8; c++) in.gate[c] = (i % 4800) < 10;
		e->process(in);
		mx = std::max(mx, std::fabs(e->out));
	}
	CHECK(mx <= 10.f);
}

TEST("plink: fuzz with random parameters stays finite") {
	auto e = makePlink();
	e->set.voices = 4;
	Rng r; r.seed(4242);
	bool ok = true;
	PlinkIn in;
	int n = (int) (60.f * SR);   // one minute here; the spec's ten minutes run with -v --long
	for (int i = 0; i < n && ok; i++) {
		if (i % 480 == 0) {
			randomizeSound(e->panel.knobs, r);
			e->panel.knobs[P_OCTAVE] = (float) ((int) (r.next() % 7) - 3);
			e->panel.satDb = r.uni() * 60.f; e->panel.crush = (int) (r.next() % 11);
			e->panel.dest[0] = (int) (r.next() % DEST_XY_COUNT);
			e->panel.dest[2] = (int) (r.next() % DEST_COUNT);
			e->panel.presetA = (int) (r.next() % 32); e->panel.presetB = (int) (r.next() % 32);
		}
		in.x = r.bi() * 10.f; in.mod = r.bi() * 10.f; in.pitch[0] = r.bi() * 5.f;
		in.gate[0] = (r.next() % 2000) == 0 ? !in.gate[0] : in.gate[0];
		e->process(in);
		if (!std::isfinite(e->out) || std::fabs(e->out) > 10.f) ok = false;
		e->randomizeRequest = false;
	}
	CHECK(ok);
}

TEST("plink: factory presets render in range and decay") {
	auto e = makePlink();
	int bad = 0;
	for (int s = 0; s < 32; s++) {
		for (int i = 0; i < NP; i++) e->panel.knobs[i] = e->bank[s].v[i];
		e->panel.dest[2] = DEST_PARAM0 + P_RDEC;
		e->reset();
		std::vector<float> rec;
		float pk = 0, tail = 0;
		int n = (int) (2.f * SR);
		PlinkIn in;
		for (int i = 0; i < n; i++) {
			// AHR sounds (bowed, blown) are measured with a 1 s gate, struck sounds with 50 ms
			in.gate[0] = i < (int) ((e->bank[s].v[P_ENVTYPE] > 0.5f ? 1.f : 0.05f) * SR);
			e->process(in);
			float a = std::fabs(e->out);
			pk = std::max(pk, a);
			if (i > n - 4800) tail = std::max(tail, a);
			if (g_render) rec.push_back(e->out);
		}
		float db = 20 * std::log10(pk / 5.f), tdb = 20 * std::log10(tail / std::max(pk, 1e-9f) + 1e-12f);
		INFO("%2d %-13s peak %5.1f dBFS tail %6.1f dB\n", s + 1, e->bank[s].name, db, tdb);
		if (!(db > -12.f && db < 3.f)) { bad++; std::printf("  preset %d %s peak %.1f dBFS\n", s + 1, e->bank[s].name, db); }
		if (e->bank[s].v[P_RDEC] < 100 && e->bank[s].v[P_ENVTYPE] < 0.5f && tdb > -60.f) { bad++; std::printf("  preset %d %s tail %.1f dB\n", s + 1, e->bank[s].name, tdb); }
		if (g_render) {
			char path[256];
			std::snprintf(path, sizeof path, "build/render/%02d-%s.wav", s + 1, e->bank[s].name);
			for (char* c = path + 13; *c; c++) if (*c == ' ') *c = '_';
			FILE* f = std::fopen(path, "wb");
			if (f) {
				int bytes = (int) rec.size() * 2;
				auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
				auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
				std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
				w32(16); w16(1); w16(1); w32(48000); w32(96000); w16(2); w16(16);
				std::fwrite("data", 1, 4, f); w32(bytes);
				for (float v : rec) w16((uint16_t) (int16_t) std::lround(clampf(v / 10.f, -1.f, 1.f) * 32767.f));
				std::fclose(f);
			}
		}
	}
	CHECK(bad == 0);
}

TEST("plink: partial frequencies hold across sample rates") {
	for (float sr : {44100.f, 96000.f}) {
		PlinkEngine e; e.setRate(sr); loadFactoryBank(e.bank);
		e.panel.knobs[P_OCTAVE] = 0;
		PlinkIn in; in.gate[0] = true; e.process(in);
		const PlinkVoice& v = e.voice[newestVoice(e)];
		float f = std::atan2(v.ci[0], v.cr[0]) * sr / (2.f * (float) M_PI);
		CHECK_NEAR(f, C4_HZ, C4_HZ * 0.01);
	}
}
