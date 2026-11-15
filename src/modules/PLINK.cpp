// PLINK modal percussion voice (spec §5).
#include "../plugin.hpp"
#include "../dsp/FactoryPresets.hpp"
#include "../generated/Layout.hpp"
#include <osdialog.h>

using tx::PlinkParam;

// Knob order on the panel = engine parameter order 0..17 (§5.2).
struct SoundKnobQuantity : ParamQuantity {
	int pi = 0;
	std::string getDisplayValueString() override {
		float v = getValue();
		std::string ph = tx::physicalString(pi, v);
		std::string s = string::f("%d", (int) std::lround(v));
		return ph.empty() ? s : s + " (" + ph + ")";
	}
};

struct PLINK : Module {
	enum ParamId {
		MIX_PARAM, STIFF_PARAM, DENS_PARAM, NATK_PARAM, NDEC_PARAM, LPCUT_PARAM, LPENV_PARAM, LPQ_PARAM, HPCUT_PARAM,
		HPENV_PARAM, HPQ_PARAM, RDEC_PARAM, RPOS_PARAM, RTONE_PARAM, RINH_PARAM, RLOWCUT_PARAM, PENVAMT_PARAM, PENVDEC_PARAM,
		ENVTYPE_PARAM, TYPE_PARAM,
		XSET_PARAM, YSET_PARAM, MODSET_PARAM, DEPTH_PARAM, OCTDN_PARAM, OCTUP_PARAM, QUANT_PARAM,
		LOAD_PARAM, SAVE_PARAM, PREV_PARAM, NEXT_PARAM, RANDOM_PARAM, OK_PARAM, TRIGGER_PARAM, SELECT_PARAM,
		PITCH_PARAM, DECAY_PARAM, X_PARAM, Y_PARAM, XATT_PARAM, YATT_PARAM, MODATT_PARAM, DECAYATT_PARAM,
		GAIN_PARAM, SAT_PARAM, CRUSH_PARAM,
		PARAMS_LEN
	};
	enum InputId { X_INPUT, Y_INPUT, MOD_INPUT, DECAY_INPUT, PITCH_INPUT, TRIG_INPUT, VEL_INPUT, INPUTS_LEN };
	enum OutputId { OUT_OUTPUT, OUTPUTS_LEN };
	enum LightId {
		XSET_LIGHT, YSET_LIGHT, MODSET_LIGHT, QUANT_LIGHT, LOAD_LIGHT, SAVE_LIGHT, TRIGGER_LIGHT,
		TYPE0_LIGHT, TYPE1_LIGHT, TYPE2_LIGHT, TYPE3_LIGHT, TYPE4_LIGHT, TYPE5_LIGHT, VOICE1_LIGHT, VOICE2_LIGHT,
		LIGHTS_LEN
	};
	enum Ui { UI_NONE, UI_PICK_X, UI_PICK_Y, UI_PICK_MOD, UI_PICK_A, UI_PICK_B, UI_PICK_TARGET, UI_LOAD, UI_SAVE };

	tx::PlinkEngine engine;
	int type = tx::OBJ_DRUMHEAD, octave = -1;
	bool quant = false;
	int currentSlot = 0, browse = 0;
	int ui = UI_NONE, lastInput = 0;
	float backup[tx::NP];
	int backupSlot = 0;
	float lastSelect = NAN;
	// display helpers (written here, read by the widget)
	int touched = -1;
	float touchTime = 0.f;
	float lastKnob[tx::NP];
	float trigFlash = 0.f;

	dsp::BooleanTrigger btn[PARAMS_LEN];
	GateIn trigIn[8];
	dsp::ClockDivider ctl;

	PLINK() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < tx::NP_CONT; i++) {
			const tx::ParamInfo& pi = tx::paramInfo(i);
			auto* q = configParam<SoundKnobQuantity>(MIX_PARAM + i, pi.lo, pi.hi, pi.def, pi.name);
			q->pi = i;
		}
		configSwitch(ENVTYPE_PARAM, 0.f, 1.f, 0.f, "Noise envelope", {"AR (attack, release)", "AHR (holds while TRIG is high)"});
		configButton(TYPE_PARAM, "Object type (String, Beam, Marimba, Drumhead, Membrane, Plate)");
		configButton(XSET_PARAM, "Set X destination (turn SELECT, press again to finish)");
		configButton(YSET_PARAM, "Set Y destination (turn SELECT, press again to finish)");
		configButton(MODSET_PARAM, "Set MOD destination or function (turn SELECT, press again to finish)");
		configButton(DEPTH_PARAM, "Depth (LOW, MED, HIGH, FULL); for Preset Step and Morph: choose the presets");
		configButton(OCTDN_PARAM, "Octave down");
		configButton(OCTUP_PARAM, "Octave up");
		configButton(QUANT_PARAM, "Quantize PITCH knob to semitones");
		configButton(LOAD_PARAM, "Load preset (turn SELECT to preview, press LOAD or OK to load)");
		configButton(SAVE_PARAM, "Save preset (turn SELECT to choose a slot, press SAVE or OK to save)");
		configButton(PREV_PARAM, "Previous preset");
		configButton(NEXT_PARAM, "Next preset");
		configButton(RANDOM_PARAM, "Randomize the sound");
		configButton(OK_PARAM, "OK (confirm load, save or selection)");
		configButton(TRIGGER_PARAM, "Trigger");
		configParam(SELECT_PARAM, 0.f, 127.f, 64.f, "Select (scrolls destinations and presets)");
		getParamQuantity(SELECT_PARAM)->randomizeEnabled = false;
		configParam(PITCH_PARAM, -1.f, 1.f, 0.f, "Pitch", " semitones", 0.f, 24.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 1.f, "Decay (scales object and noise decay)", "%", 0.f, 100.f);
		configParam(X_PARAM, -1.f, 1.f, 0.f, "X (offset for the X destination)", "%", 0.f, 100.f);
		configParam(Y_PARAM, -1.f, 1.f, 0.f, "Y (offset for the Y destination)", "%", 0.f, 100.f);
		configParam(XATT_PARAM, -1.f, 1.f, 1.f, "X CV attenuverter", "%", 0.f, 100.f);
		configParam(YATT_PARAM, -1.f, 1.f, 1.f, "Y CV attenuverter", "%", 0.f, 100.f);
		configParam(MODATT_PARAM, -1.f, 1.f, 1.f, "MOD CV attenuverter", "%", 0.f, 100.f);
		configParam(DECAYATT_PARAM, -1.f, 1.f, 1.f, "Decay CV attenuverter", "%", 0.f, 100.f);
		configParam(GAIN_PARAM, -20.f, 40.f, 0.f, "Output gain", " dB");
		configParam(SAT_PARAM, 0.f, 60.f, 0.f, "Saturation", " dB");
		configParam(CRUSH_PARAM, 0.f, 10.f, 0.f, "Bitcrusher (0 off, 1 = 10 bit … 10 = 1 bit)");
		getParamQuantity(CRUSH_PARAM)->snapEnabled = true;
		configInput(X_INPUT, "X CV (±5 V)");
		configInput(Y_INPUT, "Y CV (±5 V)");
		configInput(MOD_INPUT, "MOD CV / gate (±5 V)");
		configInput(DECAY_INPUT, "Decay CV (±5 V)");
		configInput(PITCH_INPUT, "Pitch (1 V/oct, read while TRIG is high)");
		configInput(TRIG_INPUT, "Trigger / gate (polyphonic with 4 or 8 voices)");
		configInput(VEL_INPUT, "Velocity / accent (0 to 5 V, normalled to 5 V)");
		configOutput(OUT_OUTPUT, "Audio");
		for (int i = 0; i < 6; i++) configLight(TYPE0_LIGHT + i, tx::objectName(i));
		configLight(VOICE1_LIGHT, "Voice 1");
		configLight(VOICE2_LIGHT, "Voice 2");
		configBypass(TRIG_INPUT, OUT_OUTPUT);
		ctl.setDivision(tx::CONTROL_BLOCK);
		engine.setRate(48000.f);
		tx::loadFactoryBank(engine.bank);
		for (int i = 0; i < tx::NP; i++) lastKnob[i] = 0.f;
		loadSlot(16);   // Marimba: a friendly first sound (§5.14)
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setRate(e.sampleRate); }

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		type = tx::OBJ_DRUMHEAD; octave = -1; quant = false;
		engine.set = tx::PlinkSettings();
		engine.panel = tx::PlinkPanel();
		ui = UI_NONE;
		engine.reset();
	}

	// ---------------------------------------------------------- knobs <-> parameter arrays
	void readKnobs(float* v) {
		for (int i = 0; i < tx::NP_CONT; i++) v[i] = params[MIX_PARAM + i].getValue();
		v[tx::P_TYPE] = (float) type;
		v[tx::P_ENVTYPE] = params[ENVTYPE_PARAM].getValue();
		v[tx::P_OCTAVE] = (float) octave;
		v[tx::P_QUANT] = quant ? 1.f : 0.f;
		v[tx::P_GAIN] = params[GAIN_PARAM].getValue();
	}
	void writeKnobs(const float* v) {
		for (int i = 0; i < tx::NP_CONT; i++) params[MIX_PARAM + i].setValue(v[i]);
		type = clamp((int) std::lround(v[tx::P_TYPE]), 0, tx::OBJ_COUNT - 1);
		params[ENVTYPE_PARAM].setValue(v[tx::P_ENVTYPE] > 0.5f ? 1.f : 0.f);
		octave = clamp((int) std::lround(v[tx::P_OCTAVE]), -3, 3);
		quant = v[tx::P_QUANT] > 0.5f;
		params[GAIN_PARAM].setValue(v[tx::P_GAIN]);
	}
	void loadSlot(int s) {
		s = (s % tx::BANK_SIZE + tx::BANK_SIZE) % tx::BANK_SIZE;
		const tx::Preset& p = engine.bank[s];
		writeKnobs(p.v);
		currentSlot = s;
		if (p.kit) {
			engine.panel.dest[2] = tx::DEST_PRESET_STEP;
			engine.panel.presetA = p.kitA; engine.panel.presetB = p.kitB;
		} else if (engine.panel.dest[2] == tx::DEST_PRESET_STEP) {
			engine.panel.dest[2] = tx::DEST_PARAM0 + tx::P_RDEC;
		}
	}
	void saveSlot(int s) {
		tx::Preset& p = engine.bank[s];
		std::string name = engine.bank[currentSlot].name;
		readKnobs(p.v);
		p.setName(name);
		p.kit = engine.panel.dest[2] == tx::DEST_PRESET_STEP;
		p.kitA = engine.panel.presetA; p.kitB = engine.panel.presetB;
		currentSlot = s;
	}
	bool modified() {
		float v[tx::NP];
		readKnobs(v);
		const tx::Preset& p = engine.bank[currentSlot];
		for (int i = 0; i < tx::NP; i++) if (std::fabs(v[i] - p.v[i]) > 0.5f) return true;
		return false;
	}
	void randomize() {
		float v[tx::NP];
		readKnobs(v);
		tx::randomizeSound(v, engine.rng);
		writeKnobs(v);
	}

	// ---------------------------------------------------------- buttons and SELECT (§5.3)
	bool pressed(int id) { return btn[id].process(params[id].getValue() > 0.f); }

	void cancelLoad() {
		if (ui == UI_LOAD) { writeKnobs(backup); currentSlot = backupSlot; }
		ui = UI_NONE;
	}

	void control() {
		// SELECT as relative steps
		float sel = params[SELECT_PARAM].getValue();
		if (std::isnan(lastSelect)) lastSelect = sel;
		int steps = (int) std::lround(sel - lastSelect);
		if (steps != 0) lastSelect += steps;
		auto pickInput = [&](int which, int uiMode) {
			if (ui == uiMode) { ui = UI_NONE; return; }
			cancelLoad();
			ui = uiMode; lastInput = which;
		};
		if (pressed(XSET_PARAM)) pickInput(0, UI_PICK_X);
		if (pressed(YSET_PARAM)) pickInput(1, UI_PICK_Y);
		if (pressed(MODSET_PARAM)) pickInput(2, UI_PICK_MOD);
		if (pressed(DEPTH_PARAM)) {
			int md = engine.panel.dest[2];
			if ((ui == UI_PICK_MOD || ui == UI_PICK_A || ui == UI_PICK_B) && md == tx::DEST_PRESET_STEP)
				ui = ui == UI_PICK_MOD ? UI_PICK_A : (ui == UI_PICK_A ? UI_PICK_B : UI_PICK_MOD);
			else if ((ui == UI_PICK_MOD || ui == UI_PICK_TARGET) && md == tx::DEST_MORPH)
				ui = ui == UI_PICK_MOD ? UI_PICK_TARGET : UI_PICK_MOD;
			else {
				int in = ui == UI_PICK_X ? 0 : ui == UI_PICK_Y ? 1 : ui == UI_PICK_MOD ? 2 : lastInput;
				engine.panel.depth[in] = (engine.panel.depth[in] + 1) % tx::DEPTH_COUNT;
				lastInput = in;
				if (ui == UI_NONE) { touched = -100 - in; touchTime = 1.5f; }
			}
		}
		if (pressed(OCTDN_PARAM)) octave = std::max(-3, octave - 1);
		if (pressed(OCTUP_PARAM)) octave = std::min(3, octave + 1);
		if (pressed(QUANT_PARAM)) quant = !quant;
		if (pressed(TYPE_PARAM)) type = (type + 1) % tx::OBJ_COUNT;
		if (pressed(LOAD_PARAM)) {
			if (ui == UI_LOAD) ui = UI_NONE;
			else { cancelLoad(); readKnobs(backup); backupSlot = currentSlot; browse = currentSlot; ui = UI_LOAD; }
		}
		if (pressed(SAVE_PARAM)) {
			if (ui == UI_SAVE) { saveSlot(browse); ui = UI_NONE; }
			else { cancelLoad(); browse = currentSlot; ui = UI_SAVE; }
		}
		if (pressed(OK_PARAM)) {
			if (ui == UI_SAVE) saveSlot(browse);
			ui = UI_NONE;
		}
		if (pressed(PREV_PARAM)) { cancelLoad(); loadSlot(currentSlot - 1); }
		if (pressed(NEXT_PARAM)) { cancelLoad(); loadSlot(currentSlot + 1); }
		if (pressed(RANDOM_PARAM)) { cancelLoad(); randomize(); }
		if (steps != 0) {
			auto wrap = [](int v, int n) { return ((v % n) + n) % n; };
			switch (ui) {
				case UI_PICK_X: engine.panel.dest[0] = wrap(engine.panel.dest[0] + steps, tx::DEST_XY_COUNT); break;
				case UI_PICK_Y: engine.panel.dest[1] = wrap(engine.panel.dest[1] + steps, tx::DEST_XY_COUNT); break;
				case UI_PICK_MOD: engine.panel.dest[2] = wrap(engine.panel.dest[2] + steps, tx::DEST_COUNT); break;
				case UI_PICK_A: engine.panel.presetA = wrap(engine.panel.presetA + steps, tx::BANK_SIZE); break;
				case UI_PICK_B: engine.panel.presetB = wrap(engine.panel.presetB + steps, tx::BANK_SIZE); break;
				case UI_PICK_TARGET: engine.panel.morphTarget = wrap(engine.panel.morphTarget + steps, tx::BANK_SIZE); break;
				case UI_LOAD: browse = wrap(browse + steps, tx::BANK_SIZE); loadSlot(browse); break;   // preview
				case UI_SAVE: browse = wrap(browse + steps, tx::BANK_SIZE); break;
				default: break;
			}
		}
		if (engine.randomizeRequest) { engine.randomizeRequest = false; randomize(); }
	}

	void process(const ProcessArgs& args) override {
		if (ctl.process()) {
			control();
			tx::PlinkPanel& pn = engine.panel;
			readKnobs(pn.knobs);
			// knob-touch detection for the display
			for (int i = 0; i < tx::NP_CONT; i++) {
				if (std::fabs(pn.knobs[i] - lastKnob[i]) > 0.49f) { touched = i; touchTime = 1.5f; lastKnob[i] = pn.knobs[i]; }
			}
			touchTime = std::max(0.f, touchTime - args.sampleTime * tx::CONTROL_BLOCK);
			pn.pitchKnob = params[PITCH_PARAM].getValue();
			pn.decayKnob = params[DECAY_PARAM].getValue();
			pn.xKnob = params[X_PARAM].getValue();
			pn.yKnob = params[Y_PARAM].getValue();
			pn.satDb = params[SAT_PARAM].getValue();
			pn.crush = (int) std::lround(params[CRUSH_PARAM].getValue());
			updateLights(args.sampleTime * tx::CONTROL_BLOCK);
		}
		tx::PlinkIn in;
		int nch = std::max(1, inputs[TRIG_INPUT].getChannels());
		in.channels = nch;
		bool manual = params[TRIGGER_PARAM].getValue() > 0.f;
		bool velPatched = inputs[VEL_INPUT].isConnected();
		for (int c = 0; c < std::min(nch, 8); c++) {
			bool g = trigIn[c].process(inputs[TRIG_INPUT].getVoltage(c));
			in.gate[c] = g || (c == 0 && manual);
			in.pitch[c] = inputs[PITCH_INPUT].getPolyVoltage(c);
			in.vel[c] = velPatched ? inputs[VEL_INPUT].getPolyVoltage(c) : 5.f;
		}
		in.x = clamp(inputs[X_INPUT].getVoltage(), -5.f, 5.f) * params[XATT_PARAM].getValue();
		in.y = clamp(inputs[Y_INPUT].getVoltage(), -5.f, 5.f) * params[YATT_PARAM].getValue();
		in.mod = clamp(inputs[MOD_INPUT].getVoltage(), -5.f, 5.f) * params[MODATT_PARAM].getValue();
		in.decayCv = clamp(inputs[DECAY_INPUT].getVoltage(), -5.f, 5.f) * params[DECAYATT_PARAM].getValue();
		uint64_t before = engine.counter;
		engine.process(in);
		if (engine.counter != before) trigFlash = 0.06f;
		if (engine.set.polyOut) {
			int n = std::max(1, engine.maxVoices());
			outputs[OUT_OUTPUT].setChannels(n);
			for (int c = 0; c < n; c++) outputs[OUT_OUTPUT].setVoltage(engine.polyOut[c], c);
		} else {
			outputs[OUT_OUTPUT].setChannels(1);
			outputs[OUT_OUTPUT].setVoltage(engine.out);
		}
	}

	void updateLights(float dt) {
		lights[XSET_LIGHT].setBrightness(ui == UI_PICK_X ? 1.f : 0.f);
		lights[YSET_LIGHT].setBrightness(ui == UI_PICK_Y ? 1.f : 0.f);
		lights[MODSET_LIGHT].setBrightness(ui == UI_PICK_MOD || ui == UI_PICK_A || ui == UI_PICK_B || ui == UI_PICK_TARGET ? 1.f : 0.f);
		lights[QUANT_LIGHT].setBrightness(quant ? 1.f : 0.f);
		lights[LOAD_LIGHT].setBrightness(ui == UI_LOAD ? 1.f : 0.f);
		lights[SAVE_LIGHT].setBrightness(ui == UI_SAVE ? 1.f : 0.f);
		trigFlash = std::max(0.f, trigFlash - dt);
		lights[TRIGGER_LIGHT].setBrightness(trigFlash > 0.f ? 1.f : 0.f);
		for (int i = 0; i < 6; i++) lights[TYPE0_LIGHT + i].setBrightness(type == i ? 1.f : 0.f);
		int nv = engine.maxVoices();
		float a = 0.f, b = 0.f;
		for (int i = 0; i < std::max(2, nv); i++) {
			float l = clamp(engine.voiceLevel[i] / 5.f, 0.f, 1.f);
			if (i % 2 == 0) a = std::max(a, l); else b = std::max(b, l);
		}
		lights[VOICE1_LIGHT].setSmoothBrightness(a, dt);
		lights[VOICE2_LIGHT].setSmoothBrightness(b, dt);
	}

	// ---------------------------------------------------------- display text (§5.11)
	std::string line1() {
		const tx::PlinkPanel& pn = engine.panel;
		auto slotName = [&](int s) { return string::f("%03d %s", s + 1, engine.bank[s].name); };
		switch (ui) {
			case UI_PICK_X: return "X > " + tx::destName(pn.dest[0]);
			case UI_PICK_Y: return "Y > " + tx::destName(pn.dest[1]);
			case UI_PICK_MOD: case UI_PICK_A: case UI_PICK_B: case UI_PICK_TARGET: return "MOD > " + tx::destName(pn.dest[2]);
			case UI_LOAD: return "LOAD " + slotName(browse);
			case UI_SAVE: return "SAVE TO " + slotName(browse);
		}
		if (touchTime > 0.f && touched >= 0) return tx::paramInfo(touched).shortName;
		if (touchTime > 0.f && touched <= -100) return std::string(touched == -100 ? "X" : touched == -101 ? "Y" : "MOD") + " depth";
		return slotName(currentSlot) + (modified() ? "*" : "");
	}
	std::string line2() {
		const tx::PlinkPanel& pn = engine.panel;
		auto depthOf = [&](int i) { return std::string(tx::depthName(pn.depth[i])); };
		auto slot = [&](int s) { return string::f("%03d %s", s + 1, engine.bank[s].name); };
		switch (ui) {
			case UI_PICK_X: return "depth " + depthOf(0) + "  SELECT to change";
			case UI_PICK_Y: return "depth " + depthOf(1) + "  SELECT to change";
			case UI_PICK_MOD:
				if (pn.dest[2] == tx::DEST_PRESET_STEP) return string::f("Pst A:B %03d:%03d  DEPTH=set", pn.presetA + 1, pn.presetB + 1);
				if (pn.dest[2] == tx::DEST_MORPH) return "to " + slot(pn.morphTarget) + "  DEPTH=set";
				if (pn.dest[2] >= tx::DEST_CHOKE_BOTH) return "gate at MOD";
				return "depth " + depthOf(2);
			case UI_PICK_A: return "A: " + slot(pn.presetA);
			case UI_PICK_B: return "B: " + slot(pn.presetB);
			case UI_PICK_TARGET: return "Morph to " + slot(pn.morphTarget);
			case UI_LOAD: return "LOAD/OK = load, other = cancel";
			case UI_SAVE: return "SAVE/OK = save as " + std::string(engine.bank[currentSlot].name);
		}
		if (touchTime > 0.f && touched >= 0) {
			float v = params[MIX_PARAM + touched].getValue();
			std::string ph = tx::physicalString(touched, v);
			return string::f("%d", (int) std::lround(v)) + (ph.empty() ? "" : "  " + ph);
		}
		if (touchTime > 0.f && touched <= -100) return depthOf(-100 - touched);
		if (pn.dest[2] == tx::DEST_PRESET_STEP) {
			int s = engine.lastStepSlot;
			return string::f("Pst A:B %03d:%03d", pn.presetA + 1, pn.presetB + 1) + (s >= 0 ? string::f("  >%03d", s + 1) : "");
		}
		return std::string(tx::objectName(type)) + string::f("  oct %+d", octave) + (quant ? "  Q" : "") +
		       "  X:" + tx::destName(pn.dest[0]);
	}

	// ---------------------------------------------------------- persistence
	json_t* presetToJson(const tx::Preset& p) {
		json_t* o = json_object();
		json_object_set_new(o, "name", json_string(p.name));
		json_t* a = json_array();
		for (int i = 0; i < tx::NP; i++) json_array_append_new(a, json_real(p.v[i]));
		json_object_set_new(o, "v", a);
		if (p.kit) {
			json_object_set_new(o, "kitA", json_integer(p.kitA));
			json_object_set_new(o, "kitB", json_integer(p.kitB));
		}
		return o;
	}
	void presetFromJson(json_t* o, tx::Preset& p) {
		p.init();
		if (json_t* n = json_object_get(o, "name")) p.setName(json_string_value(n));
		if (json_t* a = json_object_get(o, "v"))
			for (int i = 0; i < tx::NP; i++) if (json_t* j = json_array_get(a, i)) p.v[i] = (float) json_number_value(j);
		json_t* ka = json_object_get(o, "kitA");
		json_t* kb = json_object_get(o, "kitB");
		if (ka && kb) { p.kit = true; p.kitA = (int) json_integer_value(ka); p.kitB = (int) json_integer_value(kb); }
	}
	json_t* bankToJson() {
		json_t* b = json_array();
		for (int i = 0; i < tx::BANK_SIZE; i++) json_array_append_new(b, presetToJson(engine.bank[i]));
		return b;
	}
	void bankFromJson(json_t* b) {
		for (int i = 0; i < tx::BANK_SIZE; i++) if (json_t* o = json_array_get(b, i)) presetFromJson(o, engine.bank[i]);
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		const tx::PlinkPanel& pn = engine.panel;
		json_object_set_new(r, "type", json_integer(type));
		json_object_set_new(r, "octave", json_integer(octave));
		json_object_set_new(r, "quant", json_boolean(quant));
		json_object_set_new(r, "slot", json_integer(currentSlot));
		json_t* d = json_array();
		for (int i = 0; i < 3; i++) {
			json_t* o = json_object();
			json_object_set_new(o, "dest", json_integer(pn.dest[i]));
			json_object_set_new(o, "depth", json_integer(pn.depth[i]));
			json_array_append_new(d, o);
		}
		json_object_set_new(r, "inputs", d);
		json_object_set_new(r, "presetA", json_integer(pn.presetA));
		json_object_set_new(r, "presetB", json_integer(pn.presetB));
		json_object_set_new(r, "morphTarget", json_integer(pn.morphTarget));
		json_object_set_new(r, "voices", json_integer(engine.set.voices));
		json_object_set_new(r, "velMode", json_integer(engine.set.velMode));
		json_object_set_new(r, "gateDelay", json_real(engine.set.gateDelay));
		json_object_set_new(r, "partials", json_integer(engine.set.partials));
		json_object_set_new(r, "polyOut", json_boolean(engine.set.polyOut));
		json_object_set_new(r, "bank", bankToJson());
		return r;
	}
	void dataFromJson(json_t* r) override {
		tx::PlinkPanel& pn = engine.panel;
		if (json_t* b = json_object_get(r, "bank")) bankFromJson(b);
		if (json_t* j = json_object_get(r, "type")) type = clamp((int) json_integer_value(j), 0, tx::OBJ_COUNT - 1);
		if (json_t* j = json_object_get(r, "octave")) octave = clamp((int) json_integer_value(j), -3, 3);
		if (json_t* j = json_object_get(r, "quant")) quant = json_boolean_value(j);
		if (json_t* j = json_object_get(r, "slot")) currentSlot = clamp((int) json_integer_value(j), 0, tx::BANK_SIZE - 1);
		if (json_t* d = json_object_get(r, "inputs")) {
			for (int i = 0; i < 3; i++) {
				json_t* o = json_array_get(d, i);
				if (!o) continue;
				int maxD = i == 2 ? tx::DEST_COUNT : tx::DEST_XY_COUNT;
				if (json_t* j = json_object_get(o, "dest")) pn.dest[i] = clamp((int) json_integer_value(j), 0, maxD - 1);
				if (json_t* j = json_object_get(o, "depth")) pn.depth[i] = clamp((int) json_integer_value(j), 0, tx::DEPTH_COUNT - 1);
			}
		}
		if (json_t* j = json_object_get(r, "presetA")) pn.presetA = clamp((int) json_integer_value(j), 0, tx::BANK_SIZE - 1);
		if (json_t* j = json_object_get(r, "presetB")) pn.presetB = clamp((int) json_integer_value(j), 0, tx::BANK_SIZE - 1);
		if (json_t* j = json_object_get(r, "morphTarget")) pn.morphTarget = clamp((int) json_integer_value(j), 0, tx::BANK_SIZE - 1);
		if (json_t* j = json_object_get(r, "voices")) engine.set.voices = clamp((int) json_integer_value(j), 1, 8);
		if (json_t* j = json_object_get(r, "velMode")) engine.set.velMode = clamp((int) json_integer_value(j), 0, 2);
		if (json_t* j = json_object_get(r, "gateDelay")) engine.set.gateDelay = clamp((float) json_number_value(j), 0.f, 0.01f);
		if (json_t* j = json_object_get(r, "partials")) engine.set.partials = json_integer_value(j) <= 32 ? 32 : 64;
		if (json_t* j = json_object_get(r, "polyOut")) engine.set.polyOut = json_boolean_value(j);
		ui = UI_NONE;
	}
};

// ---------------------------------------------------------------- display widget
struct PlinkDisplay : widget::Widget {
	PLINK* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		std::string a = "PLINK", b = "modal percussion voice";
		if (module) { a = module->line1(); b = module->line2(); }
		NVGcolor fg = nvgRGB(0xff, 0x9a, 0x5a), dim = nvgRGBA(0xff, 0xc8, 0xa0, 0xc0);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFontSize(args.vg, box.size.y * 0.36f);
		nvgFillColor(args.vg, fg);
		nvgText(args.vg, 7.f, box.size.y * 0.32f, a.c_str(), NULL);
		nvgFontSize(args.vg, box.size.y * 0.27f);
		nvgFillColor(args.vg, dim);
		nvgText(args.vg, 7.f, box.size.y * 0.72f, b.c_str(), NULL);
		Widget::drawLayer(args, layer);
	}
};

struct PLINKWidget : ModuleWidget {
	PLINKWidget(PLINK* m) {
		txScaffold(this, m, "PLINK");
		placePLINK(this, m);
		auto* d = new PlinkDisplay;
		d->module = m;
		d->box.pos = mm2px(Vec(PLINK_DISPLAY_X, PLINK_DISPLAY_Y));
		d->box.size = mm2px(Vec(PLINK_DISPLAY_W, PLINK_DISPLAY_H));
		addChild(d);
	}

	void appendContextMenu(Menu* menu) override {
		PLINK* m = dynamic_cast<PLINK*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Voices", {"1 (new hit chokes the last)", "2 (hardware)", "4", "8"},
			[=]() { int v = m->engine.set.voices; return v == 1 ? 0 : v == 2 ? 1 : v == 4 ? 2 : 3; },
			[=](int i) { static const int n[4] = {1, 2, 4, 8}; m->engine.set.voices = n[i]; m->engine.reset(); }));
		menu->addChild(createBoolPtrMenuItem("Poly out (one channel per voice)", "", &m->engine.set.polyOut));
		menu->addChild(createIndexSubmenuItem("Velocity mode", {"Accent", "Dynamics", "Volume"},
			[=]() { return m->engine.set.velMode; }, [=](int i) { m->engine.set.velMode = i; }));
		menu->addChild(createIndexSubmenuItem("Gate delay", {"0 ms", "1 ms", "2 ms (hardware)", "5 ms"},
			[=]() { float d = m->engine.set.gateDelay; return d < 0.0005f ? 0 : d < 0.0015f ? 1 : d < 0.003f ? 2 : 3; },
			[=](int i) { static const float d[4] = {0.f, 0.001f, 0.002f, 0.005f}; m->engine.set.gateDelay = d[i]; }));
		menu->addChild(createIndexSubmenuItem("Partials", {"32 (lighter on CPU)", "64"},
			[=]() { return m->engine.set.partials <= 32 ? 0 : 1; }, [=](int i) { m->engine.set.partials = i ? 64 : 32; }));
		menu->addChild(createSubmenuItem("Object type", tx::objectName(m->type), [=](Menu* sub) {
			for (int i = 0; i < tx::OBJ_COUNT; i++)
				sub->addChild(createCheckMenuItem(tx::objectName(i), "", [=]() { return m->type == i; }, [=]() { m->type = i; }));
		}));
		for (int s = 0; s < 3; s++) {
			static const char* nm[3] = {"X destination", "Y destination", "MOD destination"};
			menu->addChild(createSubmenuItem(nm[s], tx::destName(m->engine.panel.dest[s]), [=](Menu* sub) {
				int n = s == 2 ? tx::DEST_COUNT : tx::DEST_XY_COUNT;
				for (int d = 0; d < n; d++)
					sub->addChild(createCheckMenuItem(tx::destName(d), "", [=]() { return m->engine.panel.dest[s] == d; }, [=]() { m->engine.panel.dest[s] = d; }));
				sub->addChild(new MenuSeparator);
				for (int d = 0; d < tx::DEPTH_COUNT; d++)
					sub->addChild(createCheckMenuItem(std::string("Depth ") + tx::depthName(d), "", [=]() { return m->engine.panel.depth[s] == d; }, [=]() { m->engine.panel.depth[s] = d; }));
			}));
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Preset %03d %s", m->currentSlot + 1, m->engine.bank[m->currentSlot].name)));
		menu->addChild(createSubmenuItem("Load preset", "", [=](Menu* sub) {
			for (int i = 0; i < tx::BANK_SIZE; i++) {
				const tx::Preset& p = m->engine.bank[i];
				if (i >= 32 && std::string(p.name) == "Init") continue;
				sub->addChild(createCheckMenuItem(string::f("%03d %s", i + 1, p.name), "", [=]() { return m->currentSlot == i; }, [=]() { m->ui = PLINK::UI_NONE; m->loadSlot(i); }));
			}
		}));
		auto* nameField = new ui::TextField;
		nameField->box.size.x = 160.f;
		nameField->placeholder = "Preset name (12 letters)";
		nameField->setText(m->engine.bank[m->currentSlot].name);
		struct RenameItem : MenuItem {
			PLINK* m; ui::TextField* f;
			void onAction(const ActionEvent& e) override { m->engine.bank[m->currentSlot].setName(f->getText()); }
		};
		menu->addChild(createMenuLabel("Rename (type, then click Rename):"));
		menu->addChild(nameField);
		auto* ri = createMenuItem<RenameItem>("Rename preset");
		ri->m = m; ri->f = nameField;
		menu->addChild(ri);
		menu->addChild(createMenuItem("Save sound to the current slot", "", [=]() { m->saveSlot(m->currentSlot); }));
		menu->addChild(createMenuItem("Init preset (default sound)", "", [=]() {
			tx::Preset p; p.init(); m->writeKnobs(p.v);
		}));
		menu->addChild(createMenuItem("Reset bank to factory", "", [=]() { tx::loadFactoryBank(m->engine.bank); }));
		menu->addChild(createMenuItem("Export bank…", "", [=]() {
			char* path = osdialog_file(OSDIALOG_SAVE, NULL, "plink-bank.json", NULL);
			if (!path) return;
			json_t* b = m->bankToJson();
			json_dump_file(b, path, JSON_INDENT(1));
			json_decref(b);
			std::free(path);
		}));
		menu->addChild(createMenuItem("Import bank…", "", [=]() {
			char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
			if (!path) return;
			json_error_t err;
			json_t* b = json_load_file(path, 0, &err);
			if (b && json_is_array(b)) m->bankFromJson(b);
			if (b) json_decref(b);
			std::free(path);
		}));
	}
};

Model* modelPLINK = createModel<PLINK, PLINKWidget>("PLINK");
