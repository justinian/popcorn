#pragma once
/// \file logger.h
/// Kernel logging facility.

#include <stdarg.h>
#include <stdint.h>

#include "kutil/bip_buffer.h"
#include "kutil/spinlock.h"

namespace kutil {
namespace log {

using area_t = uint8_t;
enum class level : uint8_t {
	none, debug, info, warn, error, fatal, max
};

class logger
{
public:
	/// Callback type for immediate-mode logging
	typedef void (*immediate_cb)(area_t, level, const char *);

	/// Callback type for log flushing
	typedef void (*flush_cb)();

	/// Default constructor. Creates a logger without a backing store.
	/// \arg output  Immediate-mode logging output function
	logger(immediate_cb output = nullptr);

	/// Constructor. Logs are written to the given buffer.
	/// \arg buffer  Buffer to which logs are written
	/// \arg size    Size of `buffer`, in bytes
	/// \arg output  Immediate-mode logging output function
	logger(uint8_t *buffer, size_t size, immediate_cb output = nullptr);

	/// Register a log area for future use.
	/// \arg area      The key for the new area
	/// \arg name      The area name
	/// \arg verbosity What level of logs to print for this area
	void register_area(area_t area, const char *name, level verbosity);

	/// Register an immediate-mode log callback
	inline void set_immediate(immediate_cb cb) { m_immediate = cb; }

	/// Register a flush callback
	inline void set_flush(flush_cb cb) { m_flush = cb; }

	/// Get the default logger.
	inline logger & get() { return *s_log; }

	/// Get the registered name for a given area
	inline const char * area_name(area_t area) const { return m_names[area]; }

	/// Get the name of a level
	inline const char * level_name(level l) const { return s_level_names[static_cast<unsigned>(l)]; }

	/// Write to the log
	/// \arg severity  The severity of the message
	/// \arg area      The log area to write to
	/// \arg fmt       A printf-like format string
	inline void log(level severity, area_t area, const char *fmt, ...)
	{
		level limit = get_level(area);
		if (limit == level::none || severity < limit)
			return;

		va_list args;
		va_start(args, fmt);
		output(severity, area, fmt, args);
		va_end(args);
	}

	struct entry
	{
		uint8_t bytes;
		area_t area;
		level severity;
		uint8_t sequence;
		char message[0];
	};

	/// Get the next log entry from the buffer
	/// \arg buffer  The buffer to copy the log message into
	/// \arg size    Size of the passed-in buffer, in bytes
	/// \returns     The size of the log entry (if larger than the
	///              buffer, then no data was copied)
	size_t get_entry(void *buffer, size_t size);

	/// Get whether there is currently data in the log buffer
	inline bool has_log() const { return m_buffer.size(); }

private:
	friend void debug(area_t area, const char *fmt, ...);
	friend void info (area_t area, const char *fmt, ...);
	friend void warn (area_t area, const char *fmt, ...);
	friend void error(area_t area, const char *fmt, ...);
	friend void fatal(area_t area, const char *fmt, ...);

	void output(level severity, area_t area, const char *fmt, va_list args);

	void set_level(area_t area, level l);
	level get_level(area_t area);

	static const unsigned num_areas = 1 << (sizeof(area_t) * 8);
	uint8_t m_levels[num_areas / 2];
	const char *m_names[num_areas];
	immediate_cb m_immediate;
	flush_cb m_flush;

	uint8_t m_sequence;

	kutil::bip_buffer m_buffer;
	kutil::spinlock m_lock;

	static logger *s_log;
	static const char *s_level_names[static_cast<unsigned>(level::max)];
};

void debug(area_t area, const char *fmt, ...);
void info (area_t area, const char *fmt, ...);
void warn (area_t area, const char *fmt, ...);
void error(area_t area, const char *fmt, ...);
void fatal(area_t area, const char *fmt, ...);

} // namespace log

namespace logs {
#define LOG(name, lvl) extern const log::area_t name;
#include "j6/tables/log_areas.inc"
#undef LOG
} // namespace logs

} // namespace kutil
