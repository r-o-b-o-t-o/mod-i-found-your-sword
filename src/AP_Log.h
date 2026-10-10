#ifndef _MOD_ARCHIPELAWOW_LOG_H_
#define _MOD_ARCHIPELAWOW_LOG_H_

#include "Log.h"

// Tags the module's lines in the server log. The format string must be a literal, so the prefix
// can be concatenated to it.
#define AP_LOG_TRACE(...) LOG_TRACE("module.archipelawow", "[AP] " __VA_ARGS__)
#define AP_LOG_DEBUG(...) LOG_DEBUG("module.archipelawow", "[AP] " __VA_ARGS__)
#define AP_LOG_INFO(...)  LOG_INFO("module.archipelawow", "[AP] " __VA_ARGS__)
#define AP_LOG_WARN(...)  LOG_WARN("module.archipelawow", "[AP] " __VA_ARGS__)
#define AP_LOG_ERROR(...) LOG_ERROR("module.archipelawow", "[AP] " __VA_ARGS__)
#define AP_LOG_FATAL(...) LOG_FATAL("module.archipelawow", "[AP] " __VA_ARGS__)

#endif
