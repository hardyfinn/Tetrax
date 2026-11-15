// PLINK factory bank (spec §5.14): 32 sounds of our own. Slots 1, 5, 9 and 13 are kits:
// they set MOD = Preset Step over the next three slots.
#pragma once
#include "Plink.hpp"

namespace tx {

struct FactoryRow {
	const char* name;
	int type, mix, stiff, dens, natk, ndec, lp, lpenv, lpq, hp, hpenv, hpq, rdec, pos, tone, inh, lowcut, penvamt, penvdec, envtype, octave;
	int kitOf;   // >= 0: this slot is a kit over slots kitOf..kitOf+2 (zero-based) and copies kitOf's sound
};

enum { S_, B_, M_, D_, MB_, P_ };   // String Beam Marimba Drumhead Membrane Plate

inline const FactoryRow* factoryRows(int& n) {
	static const FactoryRow rows[] = {
		{"Kit Wood", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{"Kick Skin", D_, 10, 50, 127, 0, 20, 60, 0, 0, 0, 0, 0, 60, 5, 20, 0, 0, 40, 25, 0, -2, -1},
		{"Snare Wire", D_, 85, 90, 115, 0, 55, 110, -10, 10, 40, 0, 10, 45, 30, 60, -10, 10, 8, 10, 0, -1, -1},
		{"Hat Plate", P_, 110, 127, 127, 0, 25, 127, 0, 0, 90, 0, 20, 30, 50, 120, 40, 70, 0, 10, 0, 1, -1},
		{"Kit Metal", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5},
		{"Kick Beam", B_, 0, 30, 127, 0, 10, 90, 0, 0, 0, 0, 0, 55, 0, 40, -20, 0, 50, 20, 0, -2, -1},
		{"Clank", P_, 40, 110, 127, 0, 15, 100, 0, 0, 20, 0, 0, 60, 70, 110, 30, 20, 0, 10, 0, 0, -1},
		{"Tick", S_, 60, 127, 127, 0, 5, 127, 0, 0, 70, 0, 0, 15, 90, 100, 60, 60, 0, 10, 0, 2, -1},
		{"Kit Hand", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9},
		{"Conga", D_, 5, 70, 127, 0, 10, 80, 0, 0, 0, 0, 0, 70, 40, 50, 0, 0, 6, 15, 0, 0, -1},
		{"Clap Noise", MB_, 120, 64, 92, 0, 40, 100, 0, 30, 50, 0, 0, 25, 60, 70, 0, 20, 0, 10, 0, 0, -1},
		{"Shaker", MB_, 127, 64, 90, 30, 30, 127, 0, 0, 100, 0, 0, 10, 60, 100, 0, 60, 0, 10, 1, 1, -1},
		{"Kit Glass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13},
		{"Glass Kick", B_, 0, 60, 127, 0, 10, 80, 0, 0, 0, 0, 0, 70, 20, 115, -30, 0, 30, 30, 0, -1, -1},
		{"Glass Hit", P_, 30, 120, 127, 0, 10, 110, 0, 0, 40, 0, 0, 90, 30, 127, 20, 20, 0, 10, 0, 1, -1},
		{"Shard", B_, 70, 127, 100, 0, 8, 127, 0, 20, 80, 0, 30, 40, 100, 120, 63, 50, 0, 10, 0, 2, -1},
		{"Marimba", M_, 0, 60, 127, 0, 10, 80, 0, 0, 0, 0, 0, 85, 25, 30, 0, 0, 0, 10, 0, 0, -1},
		{"Vibe Bar", B_, 0, 70, 127, 0, 10, 80, 0, 0, 0, 0, 0, 110, 20, 110, -5, 0, 0, 10, 0, 0, -1},
		{"Kalimba", B_, 10, 90, 127, 0, 10, 100, 0, 0, 0, 0, 0, 80, 10, 90, 12, 0, 0, 10, 0, 1, -1},
		{"Nylon Pluck", S_, 0, 80, 127, 0, 10, 90, 0, 0, 0, 0, 0, 75, 30, 25, 0, 0, 0, 10, 0, 0, -1},
		{"Steel String", S_, 0, 110, 127, 0, 10, 110, 0, 0, 0, 0, 0, 100, 20, 115, 4, 0, 0, 10, 0, 0, -1},
		{"Bowed String", S_, 127, 64, 75, 60, 70, 70, 0, 20, 10, 0, 0, 100, 25, 90, 0, 0, 0, 10, 1, 0, -1},
		{"Tom Low", D_, 10, 60, 127, 0, 10, 80, 0, 0, 0, 0, 0, 70, 20, 40, 0, 0, 20, 40, 0, -1, -1},
		{"Tom High", D_, 10, 70, 127, 0, 10, 80, 0, 0, 0, 0, 0, 65, 20, 45, 0, 0, 20, 35, 0, 0, -1},
		{"Gong", P_, 15, 30, 127, 0, 20, 70, 0, 0, 0, 0, 0, 127, 40, 120, 10, 0, -8, 60, 0, -2, -1},
		{"Bell", P_, 0, 110, 127, 0, 10, 110, 0, 0, 0, 0, 0, 115, 15, 125, -20, 0, 0, 10, 0, 1, -1},
		{"Cowbell-ish", P_, 10, 120, 127, 0, 10, 100, 0, 0, 30, 0, 0, 50, 40, 110, -40, 30, 0, 10, 0, 1, -1},
		{"Log Drum", B_, 0, 40, 127, 0, 10, 60, 0, 0, 0, 0, 0, 60, 30, 20, -10, 0, 4, 15, 0, -1, -1},
		{"Water Drop", MB_, 0, 127, 127, 0, 5, 127, 0, 0, 0, 0, 0, 40, 60, 90, 0, 0, 63, 6, 0, 1, -1},
		{"Zap", D_, 0, 127, 127, 0, 5, 127, 0, 0, 0, 0, 0, 50, 10, 80, 0, 0, 63, 40, 0, 1, -1},
		{"Rain Plate", P_, 127, 64, 25, 0, 90, 90, 0, 0, 20, 0, 0, 90, 60, 100, 0, 10, 0, 10, 1, 0, -1},
		{"Wind Tube", MB_, 127, 64, 127, 70, 90, 60, -20, 50, 20, 0, 30, 100, 64, 80, 0, 0, 0, 10, 1, 0, -1},
	};
	n = (int) (sizeof rows / sizeof rows[0]);
	return rows;
}

inline void rowToPreset(const FactoryRow& r, Preset& p) {
	p.init();
	p.setName(r.name);
	p.v[P_TYPE] = (float) r.type; p.v[P_MIX] = (float) r.mix; p.v[P_STIFF] = (float) r.stiff;
	p.v[P_DENS] = (float) r.dens; p.v[P_NATK] = (float) r.natk; p.v[P_NDEC] = (float) r.ndec;
	p.v[P_LPCUT] = (float) r.lp; p.v[P_LPENV] = (float) r.lpenv; p.v[P_LPQ] = (float) r.lpq;
	p.v[P_HPCUT] = (float) r.hp; p.v[P_HPENV] = (float) r.hpenv; p.v[P_HPQ] = (float) r.hpq;
	p.v[P_RDEC] = (float) r.rdec; p.v[P_RPOS] = (float) r.pos; p.v[P_RTONE] = (float) r.tone;
	p.v[P_RINH] = (float) r.inh; p.v[P_RLOWCUT] = (float) r.lowcut; p.v[P_PENVAMT] = (float) r.penvamt;
	p.v[P_PENVDEC] = (float) r.penvdec; p.v[P_ENVTYPE] = (float) r.envtype; p.v[P_OCTAVE] = (float) r.octave;
	p.v[P_QUANT] = 0.f;
}

// Fills the bank: 32 factory sounds, the rest Init.
inline void loadFactoryBank(Preset* bank) {
	int n;
	const FactoryRow* rows = factoryRows(n);
	for (int i = 0; i < BANK_SIZE; i++) bank[i].init();
	// Output gain per slot (dB), measured so each accented hit peaks near 3.5 V (§5.14 render test).
	static const float gains[32] = {-4.0f, -4.5f, -8.0f, 1.5f, -1.0f, -1.0f, -6.0f, -6.0f, -2.0f, -2.0f, 1.0f, 7.0f, -3.0f, -3.5f, -7.5f, -4.5f, -1.5f, -3.5f, -2.0f, -3.5f, -8.0f, 11.5f, -1.5f, -1.5f, 0.5f, -9.5f, -11.0f, 0.0f, -10.0f, -10.5f, 22.0f, 7.5f};
	for (int i = 0; i < n; i++) if (rows[i].kitOf < 0) { rowToPreset(rows[i], bank[i]); bank[i].v[P_GAIN] = gains[i]; }
	for (int i = 0; i < n; i++) {
		if (rows[i].kitOf < 0) continue;
		int a = rows[i].kitOf;
		bank[i] = bank[a];
		bank[i].setName(rows[i].name);
		bank[i].kit = true;
		bank[i].kitA = a;
		bank[i].kitB = a + 2;
		bank[i].v[P_GAIN] = gains[i];
	}
}

} // namespace tx
