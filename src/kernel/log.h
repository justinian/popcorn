#pragma once

#include "kutil/logger.h"

namespace log = kutil::log;

namespace logs {

#define LOG(name, lvl) extern log::area_t name;
#include "log_areas.inc"
#undef LOG

void init();
void logger_task();

} // namespace logs

