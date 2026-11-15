// TETRA four-channel function generator and TETRA X expander (spec §4, §6).
#include "../plugin.hpp"
#include "../dsp/Tetra.hpp"
#include "../generated/Layout.hpp"

// Message from TETRA to the TETRA X on its right (§4.13).
struct TetraMessage {
	bool valid = false;
	bool eor[4] = {}, eof[4] = {};
	int mode[4] = {};
	float out[4] = {};
};

#define CH_PARAMS(n) RISE##n##_PARAM, FALL##n##_PARAM, SHAPE##n##_PARAM, LEVEL##n##_PARAM, MODE##n##_PARAM, ALT##n##_PARAM, \
	LINK##n##_PARAM, TENV##n##_PARAM, FIRE##n##_PARAM, RISEATT##n##_PARAM, FALLATT##n##_PARAM, SHAPEATT##n##_PARAM, LEVELATT##n##_PARAM
#define CH_INPUTS(n) RISECV##n##_INPUT, FALLCV##n##_INPUT, SHAPECV##n##_INPUT, LEVELCV##n##_INPUT, TRIG##n##_INPUT
#define CH_LIGHTS(n) ENUMS(CH##n##_LIGHT, 3), ENUMS(MODE##n##_LIGHT, 3), ALT##n##_LIGHT, ENUMS(LINK##n##_LIGHT, 3), TENV##n##_LIGHT, FIRE##n##_LIGHT

static const char* MODE_NAMES[tx::MODE_COUNT] = {"AD", "AHR", "Cycle", "Burst", "LFO"};
static const char* LINK_NAMES[tx::LINK_COUNT] = {"Off", "TRIG (gate of the channel to the left)", "End of rise", "End of fall"};

// Tooltip names that follow the channel's mode (§4.4).
struct ModeKnobQuantity : ParamQuantity {
	int ch = 0, which = 0;   // which: 0 rise, 1 fall, 2 shape
	std::string getLabel() override;
	std::string getDisplayValueString() override;
};

struct TETRA : Module {
	enum ParamId { CH_PARAMS(1), CH_PARAMS(2), CH_PARAMS(3), CH_PARAMS(4), PARAMS_LEN };
	enum InputId { CH_INPUTS(1), CH_INPUTS(2), CH_INPUTS(3), CH_INPUTS(4), POLYTRIG_INPUT, INPUTS_LEN };
	enum OutputId { OUT1_OUTPUT, OUT2_OUTPUT, OUT3_OUTPUT, OUT4_OUTPUT, POLY_OUTPUT, OUTPUTS_LEN };
	enum LightId { CH_LIGHTS(1), CH_LIGHTS(2), CH_LIGHTS(3), CH_LIGHTS(4), LIGHTS_LEN };
	static constexpr int PS = RISE2_PARAM - RISE1_PARAM;
	static constexpr int IS = RISECV2_INPUT - RISECV1_INPUT;
	static constexpr int LS = CH2_LIGHT - CH1_LIGHT;

	tx::TetraEngine engine;
	int mode[4] = {};
	bool alt[4][tx::MODE_COUNT] = {};
	int link[4] = {};
	bool tenV[4] = {true, true, true, true};
	bool burstRetrigger = false, steppedLevel = false, cvdPalette = false;
	dsp::BooleanTrigger modeBtn[4], altBtn[4], linkBtn[4], tenvBtn[4];
	GateIn trigIn[4], polyIn[4];
	float fireFlash[4] = {};
	dsp::ClockDivider uiDiv;
	TetraMessage msg[2];

	TETRA() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int c = 0; c < 4; c++) {
			std::string n = std::to_string(c + 1);
			auto mk = [&](int id, int which, const char* name) {
				auto* q = configParam<ModeKnobQuantity>(id, 0.f, 1.f, 0.5f, std::string(name) + " " + n);
				q->ch = c; q->which = which;
			};
			mk(RISE1_PARAM + c * PS, 0, "Rise");
			mk(FALL1_PARAM + c * PS, 1, "Fall");
			mk(SHAPE1_PARAM + c * PS, 2, "Shape");
			configParam(LEVEL1_PARAM + c * PS, 0.f, 1.f, 1.f, "Level " + n, "%", 0.f, 100.f);
			configButton(MODE1_PARAM + c * PS, "Mode " + n + " (AD, AHR, cycle, burst, LFO)");
			configButton(ALT1_PARAM + c * PS, "Alternate " + n + " (mirrored curves, ALT burst, LFV random)");
			configButton(LINK1_PARAM + c * PS, "Link " + n + " (off, TRIG, end of rise, end of fall of the channel to the left)");
			configButton(TENV1_PARAM + c * PS, "10 V maximum " + n + " (off = 5 V)");
			configButton(FIRE1_PARAM + c * PS, "Fire " + n + " (manual trigger)");
			for (int k = 0; k < 4; k++) {
				static const char* nm[4] = {"Rise", "Fall", "Shape", "Level"};
				configParam(RISEATT1_PARAM + k + c * PS, -1.f, 1.f, 1.f, std::string(nm[k]) + " CV attenuverter " + n, "%", 0.f, 100.f);
				configInput(RISECV1_INPUT + k + c * IS, std::string(nm[k]) + " CV " + n + " (5 V = full knob travel)");
			}
			configInput(TRIG1_INPUT + c * IS, "Trigger / gate / clock " + n);
			configOutput(OUT1_OUTPUT + c, "Channel " + n);
			configLight(CH1_LIGHT + c * LS, "Channel " + n + " level");
		}
		configInput(POLYTRIG_INPUT, "Poly trigger (channel c is OR'd into TRIG c)");
		configOutput(POLY_OUTPUT, "Poly (channels 1 to 4)");
		rightExpander.producerMessage = &msg[0];
		rightExpander.consumerMessage = &msg[1];
		uiDiv.setDivision(32);
		engine.setRate(48000.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int c = 0; c < 4; c++) { mode[c] = 0; link[c] = 0; tenV[c] = true; for (auto& a : alt[c]) a = false; }
		burstRetrigger = steppedLevel = false;
		engine.reset();
	}
	void resetChannel(int c) {
		mode[c] = 0; link[c] = 0; tenV[c] = true;
		for (auto& a : alt[c]) a = false;
		engine.ch[c].reset();
	}

	float knob(int id) { return tx::deadZone(params[id].getValue()); }

	void process(const ProcessArgs& args) override {
		bool ui = uiDiv.process();
		tx::TetraIn in;
		int polyCh = inputs[POLYTRIG_INPUT].getChannels();
		for (int c = 0; c < 4; c++) {
			if (ui) {
				if (modeBtn[c].process(params[MODE1_PARAM + c * PS].getValue() > 0.f)) mode[c] = (mode[c] + 1) % tx::MODE_COUNT;
				if (altBtn[c].process(params[ALT1_PARAM + c * PS].getValue() > 0.f)) alt[c][mode[c]] = !alt[c][mode[c]];
				if (linkBtn[c].process(params[LINK1_PARAM + c * PS].getValue() > 0.f)) link[c] = (link[c] + 1) % tx::LINK_COUNT;
				if (tenvBtn[c].process(params[TENV1_PARAM + c * PS].getValue() > 0.f)) tenV[c] = !tenV[c];
			}
			tx::ChannelSettings& s = engine.set[c];
			s.mode = mode[c]; s.alt = alt[c][mode[c]]; s.link = link[c]; s.tenV = tenV[c];
			auto cv = [&](int k) {
				float v = inputs[RISECV1_INPUT + k + c * IS].getVoltage();
				v = clamp(v, -tx::CV_CLIP, tx::CV_CLIP);
				return v / 5.f * params[RISEATT1_PARAM + k + c * PS].getValue();
			};
			s.rise = knob(RISE1_PARAM + c * PS) + cv(0);
			s.fall = knob(FALL1_PARAM + c * PS) + cv(1);
			s.shape = clamp(params[SHAPE1_PARAM + c * PS].getValue() + cv(2), 0.f, 1.f);
			float lvl = params[LEVEL1_PARAM + c * PS].getValue();
			if (steppedLevel) lvl = std::round(lvl * 5.f) / 5.f;
			s.level = clamp(lvl + cv(3), 0.f, 1.f);
			bool g = trigIn[c].process(inputs[TRIG1_INPUT + c * IS].getVoltage());
			if (c < polyCh) g = polyIn[c].process(inputs[POLYTRIG_INPUT].getVoltage(c)) || g;
			g = g || params[FIRE1_PARAM + c * PS].getValue() > 0.f;
			in.gate[c] = g;
			in.patched[c] = inputs[TRIG1_INPUT + c * IS].isConnected() || c < polyCh;
		}
		engine.setBurstRetrigger(burstRetrigger);
		engine.process(in);
		outputs[POLY_OUTPUT].setChannels(4);
		for (int c = 0; c < 4; c++) {
			outputs[OUT1_OUTPUT + c].setVoltage(engine.out[c]);
			outputs[POLY_OUTPUT].setVoltage(engine.out[c], c);
			if (engine.fired[c]) fireFlash[c] = 0.06f;
		}
		// expander
		if (rightExpander.module && rightExpander.module->model == modelTETRAX) {
			TetraMessage* m = (TetraMessage*) rightExpander.producerMessage;
			m->valid = true;
			for (int c = 0; c < 4; c++) {
				m->eor[c] = engine.ch[c].eor; m->eof[c] = engine.ch[c].eof;
				m->mode[c] = mode[c]; m->out[c] = engine.out[c];
			}
			rightExpander.requestMessageFlip();
		}
		if (ui) updateLights(args.sampleTime * 32.f);
	}

	void updateLights(float dt) {
		for (int c = 0; c < 4; c++) {
			int L = c * LS;
			float max = mode[c] == tx::MODE_LFO ? 5.f : (tenV[c] ? 10.f : 5.f);
			float v = clamp(engine.out[c] / max, -1.f, 1.f);
			float pos = std::max(v, 0.f), neg = std::max(-v, 0.f);
			if (cvdPalette) {   // positive blue, negative orange
				lights[CH1_LIGHT + L + 0].setBrightness(neg);
				lights[CH1_LIGHT + L + 1].setBrightness(neg * 0.5f);
				lights[CH1_LIGHT + L + 2].setBrightness(pos);
			} else {
				lights[CH1_LIGHT + L + 0].setBrightness(neg);
				lights[CH1_LIGHT + L + 1].setBrightness(pos);
				lights[CH1_LIGHT + L + 2].setBrightness(0.f);
			}
			static const float MC[tx::MODE_COUNT][3] = {{0.55f, 0.55f, 0.55f}, {1, 0, 0}, {0, 1, 0}, {1, 0.75f, 0}, {1, 0, 1}};
			static const float LC[tx::LINK_COUNT][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 0.75f, 0}};
			for (int k = 0; k < 3; k++) {
				lights[MODE1_LIGHT + L + k].setBrightness(MC[mode[c]][k]);
				lights[LINK1_LIGHT + L + k].setBrightness(LC[link[c]][k]);
			}
			lights[ALT1_LIGHT + L].setBrightness(alt[c][mode[c]] ? 1.f : 0.f);
			lights[TENV1_LIGHT + L].setBrightness(tenV[c] ? 1.f : 0.f);
			fireFlash[c] = std::max(0.f, fireFlash[c] - dt);
			lights[FIRE1_LIGHT + L].setBrightness(fireFlash[c] > 0.f ? 1.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_t* chs = json_array();
		for (int c = 0; c < 4; c++) {
			json_t* o = json_object();
			json_object_set_new(o, "mode", json_integer(mode[c]));
			json_object_set_new(o, "link", json_integer(link[c]));
			json_object_set_new(o, "tenV", json_boolean(tenV[c]));
			json_t* a = json_array();
			for (int m = 0; m < tx::MODE_COUNT; m++) json_array_append_new(a, json_boolean(alt[c][m]));
			json_object_set_new(o, "alt", a);
			json_array_append_new(chs, o);
		}
		json_object_set_new(r, "channels", chs);
		json_object_set_new(r, "burstRetrigger", json_boolean(burstRetrigger));
		json_object_set_new(r, "steppedLevel", json_boolean(steppedLevel));
		json_object_set_new(r, "cvdPalette", json_boolean(cvdPalette));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* chs = json_object_get(r, "channels");
		for (int c = 0; c < 4 && chs; c++) {
			json_t* o = json_array_get(chs, c);
			if (!o) continue;
			if (json_t* j = json_object_get(o, "mode")) mode[c] = clamp((int) json_integer_value(j), 0, tx::MODE_COUNT - 1);
			if (json_t* j = json_object_get(o, "link")) link[c] = clamp((int) json_integer_value(j), 0, tx::LINK_COUNT - 1);
			if (json_t* j = json_object_get(o, "tenV")) tenV[c] = json_boolean_value(j);
			if (json_t* a = json_object_get(o, "alt"))
				for (int m = 0; m < tx::MODE_COUNT; m++) if (json_t* j = json_array_get(a, m)) alt[c][m] = json_boolean_value(j);
		}
		if (json_t* j = json_object_get(r, "burstRetrigger")) burstRetrigger = json_boolean_value(j);
		if (json_t* j = json_object_get(r, "steppedLevel")) steppedLevel = json_boolean_value(j);
		if (json_t* j = json_object_get(r, "cvdPalette")) cvdPalette = json_boolean_value(j);
	}
};

std::string ModeKnobQuantity::getLabel() {
	TETRA* t = dynamic_cast<TETRA*>(module);
	std::string n = std::to_string(ch + 1);
	if (!t) return ParamQuantity::getLabel();
	int m = t->mode[ch];
	bool a = t->alt[ch][m];
	static const char* names[3][3] = {{"Rise time", "Fall time", "Curve"}, {"Pulse rate", "Burst length", "Pulse shape and tilt"},
	                                  {"Rate", "Morph", "Wave shape"}};
	int row = m == tx::MODE_BURST ? 1 : (m == tx::MODE_LFO ? 2 : 0);
	std::string s = names[row][which];
	if (m == tx::MODE_LFO && a) { static const char* lfv[3] = {"Rate", "Variance", "Slew"}; s = lfv[which]; }
	return s + " " + n + " (" + MODE_NAMES[m] + (a ? " alt" : "") + ")";
}

std::string ModeKnobQuantity::getDisplayValueString() {
	TETRA* t = dynamic_cast<TETRA*>(module);
	float v = getValue();
	if (!t || which == 2) return string::f("%.0f%%", v * 100.f);
	int m = t->mode[ch];
	float p = tx::deadZone(v);
	auto timeStr = [](float s) { return s < 1.f ? string::f("%.3g ms", s * 1000.f) : string::f("%.3g s", s); };
	if (m == tx::MODE_LFO && which == 0) {
		if (t->inputs[TETRA::TRIG1_INPUT + ch * TETRA::IS].isConnected() || t->link[ch] != tx::LINK_OFF) {
			float r = tx::clockRatio(tx::ratioIndex(p));
			return r >= 1.f ? string::f("x%g of clock", r) : string::f("/%g of clock", std::round(1.f / r));
		}
		return string::f("%.4g Hz", tx::rateLaw(p));
	}
	if (m == tx::MODE_LFO) return string::f("%.0f%%", v * 100.f);
	if (m == tx::MODE_BURST && which == 0) return string::f("%.4g Hz", tx::rateLaw(p));
	if (m == tx::MODE_BURST && which == 1) return timeStr(std::max(tx::BURST_MIN_LEN, tx::timeLaw(p)));
	return timeStr(tx::timeLaw(p));
}

struct TETRAWidget : ModuleWidget {
	TETRAWidget(TETRA* m) {
		txScaffold(this, m, "TETRA");
		placeTETRA(this, m);
	}
	void appendContextMenu(Menu* menu) override {
		TETRA* m = dynamic_cast<TETRA*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Burst retrigger (a trigger restarts a running burst)", "", &m->burstRetrigger));
		menu->addChild(createBoolPtrMenuItem("Stepped level (20% steps, as on the hardware)", "", &m->steppedLevel));
		menu->addChild(createBoolPtrMenuItem("Colour-blind LED palette (positive blue, negative orange)", "", &m->cvdPalette));
		for (int c = 0; c < 4; c++) {
			menu->addChild(createSubmenuItem(string::f("Channel %d", c + 1), MODE_NAMES[m->mode[c]], [=](Menu* sub) {
				for (int k = 0; k < tx::MODE_COUNT; k++)
					sub->addChild(createCheckMenuItem(MODE_NAMES[k], "", [=]() { return m->mode[c] == k; }, [=]() { m->mode[c] = k; }));
				sub->addChild(createBoolMenuItem("Alternate", "", [=]() { return m->alt[c][m->mode[c]]; }, [=](bool v) { m->alt[c][m->mode[c]] = v; }));
				sub->addChild(new MenuSeparator);
				for (int k = 0; k < tx::LINK_COUNT; k++)
					sub->addChild(createCheckMenuItem(std::string("Link: ") + LINK_NAMES[k], "", [=]() { return m->link[c] == k; }, [=]() { m->link[c] = k; }));
				sub->addChild(new MenuSeparator);
				sub->addChild(createMenuItem("Reset channel", "", [=]() { m->resetChannel(c); }));
			}));
		}
		menu->addChild(createMenuItem("Reset all channels", "", [=]() { for (int c = 0; c < 4; c++) m->resetChannel(c); }));
	}
};

Model* modelTETRA = createModel<TETRA, TETRAWidget>("TETRA");

// ---------------------------------------------------------------- TETRA X
struct TETRAX : Module {
	enum ParamId { PARAMS_LEN };
	enum InputId { INPUTS_LEN };
	enum OutputId { EOR1_OUTPUT, EOF1_OUTPUT, EOR2_OUTPUT, EOF2_OUTPUT, EOR3_OUTPUT, EOF3_OUTPUT, EOR4_OUTPUT, EOF4_OUTPUT,
	                POLYEOR_OUTPUT, POLYEOF_OUTPUT, OUTPUTS_LEN };
	enum LightId { LINKED_LIGHT, LIGHTS_LEN };
	TETRAX() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int c = 0; c < 4; c++) {
			configOutput(EOR1_OUTPUT + 2 * c, string::f("End of rise %d", c + 1));
			configOutput(EOF1_OUTPUT + 2 * c, string::f("End of fall %d", c + 1));
		}
		configOutput(POLYEOR_OUTPUT, "Poly end of rise (channels 1 to 4)");
		configOutput(POLYEOF_OUTPUT, "Poly end of fall (channels 1 to 4)");
		configLight(LINKED_LIGHT, "Connected to a TETRA on the left");
	}
	void process(const ProcessArgs& args) override {
		bool linked = leftExpander.module && leftExpander.module->model == modelTETRA;
		TetraMessage* m = linked ? (TetraMessage*) leftExpander.consumerMessage : nullptr;
		bool ok = m && m->valid;
		outputs[POLYEOR_OUTPUT].setChannels(4);
		outputs[POLYEOF_OUTPUT].setChannels(4);
		for (int c = 0; c < 4; c++) {
			float eor = ok && m->eor[c] ? tx::TX_GATE_V : 0.f;
			float eof = ok && m->eof[c] ? tx::TX_GATE_V : 0.f;
			outputs[EOR1_OUTPUT + 2 * c].setVoltage(eor);
			outputs[EOF1_OUTPUT + 2 * c].setVoltage(eof);
			outputs[POLYEOR_OUTPUT].setVoltage(eor, c);
			outputs[POLYEOF_OUTPUT].setVoltage(eof, c);
		}
		lights[LINKED_LIGHT].setBrightness(ok ? 1.f : 0.f);
	}
};

struct TETRAXWidget : ModuleWidget {
	TETRAXWidget(TETRAX* m) {
		txScaffold(this, m, "TETRAX");
		placeTETRAX(this, m);
	}
};

Model* modelTETRAX = createModel<TETRAX, TETRAXWidget>("TETRAX");
