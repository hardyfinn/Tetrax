// Knobs, jack, toggle, buttons and lights. SVGs come from scripts/make_panels.py.
#pragma once
#include "../plugin.hpp"

inline std::shared_ptr<window::Svg> txSvg(const std::string& name) {
	return window::Svg::load(asset::plugin(pluginInstance, "res/components/" + name + ".svg"));
}

struct TxKnobBase : app::SvgKnob {
	TxKnobBase() { minAngle = -0.75f * M_PI; maxAngle = 0.75f * M_PI; }
};
struct TxKnob : TxKnobBase { TxKnob() { setSvg(txSvg("TxKnob")); shadow->opacity = 0.15f; } };
struct TxKnobSmall : TxKnobBase { TxKnobSmall() { setSvg(txSvg("TxKnobSmall")); shadow->opacity = 0.15f; } };
struct TxTrim : TxKnobBase { TxTrim() { setSvg(txSvg("TxTrim")); shadow->opacity = 0.1f; } };

struct TxJack : app::SvgPort { TxJack() { setSvg(txSvg("TxJack")); shadow->opacity = 0.1f; } };

struct TxToggle2 : app::SvgSwitch {
	TxToggle2() { addFrame(txSvg("Toggle2_0")); addFrame(txSvg("Toggle2_1")); shadow->opacity = 0.f; }
};

struct TxButton : app::SvgSwitch {
	TxButton() { momentary = true; addFrame(txSvg("TxButton_0")); addFrame(txSvg("TxButton_1")); shadow->opacity = 0.f; }
};

template <typename TBase>
struct TxButtonLight : TBase {
	TxButtonLight() { this->box.size = mm2px(math::Vec(2.4f, 2.4f)); }
};

struct TxOrangeLight : GrayModuleLightWidget {
	TxOrangeLight() { addBaseColor(nvgRGB(0xff, 0x7a, 0x2e)); }
};

// Panel and screws (Rack's black screws, spec §3).
template <class W>
inline void txScaffold(W* w, Module* m, const std::string& slug) {
	w->setModule(m);
	w->setPanel(createPanel(asset::plugin(pluginInstance, "res/" + slug + ".svg")));
	float width = w->box.size.x;
	w->addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewBlack>(Vec(width - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	if (width > 4 * RACK_GRID_WIDTH) {
		w->addChild(createWidget<ScrewBlack>(Vec(width - 2 * RACK_GRID_WIDTH, 0)));
		w->addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
}

// Gate input with the spec's 0.5 V / 1.5 V Schmitt thresholds (§2).
struct GateIn {
	bool state = false;
	bool process(float v) {
		state = state ? (v > 0.5f) : (v >= 1.5f);
		return state;
	}
};
