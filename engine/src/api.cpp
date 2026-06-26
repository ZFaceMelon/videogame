#include "engine/api.h"

#include "engine/engine.h"

extern "C" int engine_run(void) { return engine::run(); }

extern "C" const char *engine_version(void) { return "0.1.0"; }
