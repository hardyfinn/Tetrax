#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelTETRA);
	p->addModel(modelTETRAX);
	p->addModel(modelPLINK);
}
