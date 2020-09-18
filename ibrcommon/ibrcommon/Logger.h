/*
 * Logger.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/data/File.h>
#include <fstream>
#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <list>

/**
 * @file Logger.h
 *
 * This file provides common logging facilities. For easy usage it provides some defines and is usable
 * like a standard output stream. It is fully configurable, can log to all standard output streams and
 * to the system syslog.
 *
 * Provided tags:
 *  emergency, alert, critical, error, warning, notice, info
 *
 * Example log message with info tag:
 * \code
 * IBRCOMMON_LOGGER(info) << "Some output..." << IBRCOMMON_ENDL;
 * \endcode
 *
 * Example debug with verbose level 42:
 * \code
 * IBRCOMMON_LOGGER_DEBUG(42) << "some debugging output" << IBRCOMMON_ENDL;
 * \endcode
 *
 * To check the current verbose level in the code use the IBRCOMMON_LOGGER_LEVEL define like this:
 * \code
 * if (IBRCOMMON_LOGGER_LEVEL > 42)
 * {
 * 		do something...
 * }
 * \endcode
 *
 */

#define IBRCOMMON_LOGGER_LEVEL \
	ibrcommon::Logger::getVerbosity()

#define IBRCOMMON_LOGGER(level) \
	if (ibrcommon::LogLevel::level & ibrcommon::Logger::getLogMask()) { \
		ibrcommon::Logger __macro_ibrcommon_logger = ibrcommon::Logger::level(""); \
		std::stringstream __macro_ibrcommon_stream; __macro_ibrcommon_stream

#define IBRCOMMON_LOGGER_TAG(tag, level) \
	if (ibrcommon::LogLevel::level & ibrcommon::Logger::getLogMask()) { \
		ibrcommon::Logger __macro_ibrcommon_logger = ibrcommon::Logger::level(tag); \
		std::stringstream __macro_ibrcommon_stream; __macro_ibrcommon_stream

#define IBRCOMMON_LOGGER_DEBUG(verbosity) \
	if (ibrcommon::Logger::getVerbosity() >= verbosity) \
	{ \
		ibrcommon::Logger __macro_ibrcommon_logger = ibrcommon::Logger::debug("", verbosity); \
		std::stringstream __macro_ibrcommon_stream; __macro_ibrcommon_stream

#define IBRCOMMON_LOGGER_DEBUG_TAG(tag, verbosity) \
	if (ibrcommon::Logger::getVerbosity() >= verbosity) \
	{ \
		ibrcommon::Logger __macro_ibrcommon_logger = ibrcommon::Logger::debug(tag, verbosity); \
		std::stringstream __macro_ibrcommon_stream; __macro_ibrcommon_stream

#define IBRCOMMON_LOGGER_ENDL \
		std::flush; \
		__macro_ibrcommon_logger.setMessage(__macro_ibrcommon_stream.str()); \
		__macro_ibrcommon_logger.print(); \
	}

#define IBRCOMMON_LOGGER_ex(level) \
	IBRCOMMON_LOGGER(level) << __PRETTY_FUNCTION__ << ": "

#define IBRCOMMON_LOGGER_DEBUG_ex(verbosity) \
	IBRCOMMON_LOGGER_DEBUG(verbosity) << __FILE__ << ":" << __LINE__ << " in " << __PRETTY_FUNCTION__ << ": "

namespace ibrcommon
{
	namespace LogLevel {
		enum Level
		{
			emergency =	1 << 0,	/* system is unusable */
			alert =		1 << 1,	/* action must be taken immediately */
			critical =	1 << 2,	/* critical conditions */
			error =		1 << 3,	/* error conditions */
			warning = 	1 << 4,	/* warning conditions */
			notice = 	1 << 5,	/* normal but significant condition */
			info = 		1 << 6,	/* informational */
			debug =		1 << 7	/* debug-level messages */
		};
	}

	/**
	 * @class Logger
	 *
	 * The Logger class is the heart of the logging framework.
	 */
	class Logger
	{
	public:
		enum LogOptions
		{
			LOG_NONE =		0,
			LOG_DATETIME =	1 << 0,	/* print date/time on log messages */
			LOG_HOSTNAME = 	1 << 1, /* print hostname on log messages */
			LOG_LEVEL =		1 << 2,	/* print the log level on log messages */
			LOG_TIMESTAMP =	1 << 3,	/* print timestamp on log messages */
			LOG_TAG = 		1 << 4  /* print tag on log messages */
		};

		enum LogLevel
		{
			LOGGER_EMERG =		1 << 0,	/* system is unusable */
			LOGGER_ALERT =		1 << 1,	/* action must be taken immediately */
			LOGGER_CRIT =		1 << 2,	/* critical conditions */
			LOGGER_ERR =		1 << 3,	/* error conditions */
			LOGGER_WARNING = 	1 << 4,	/* warning conditions */
			LOGGER_NOTICE = 	1 << 5,	/* normal but significant condition */
			LOGGER_INFO = 		1 << 6,	/* informational */
			LOGGER_DEBUG = 		1 << 7,	/* debug-level messages */
			LOGGER_ALL =		0xff
		};

		Logger(const Logger&);
		virtual ~Logger();

		/**
		 * Set the logging message
		 */
		void setMessage(const std::string &data);

		/**
		 * Returns the logging message as string
		 */
		const std::string& str() const;

		static Logger emergency(const std::string &tag);
		static Logger alert(const std::string &tag);
		static Logger critical(const std::string &tag);
		static Logger error(const std::string &tag);
		static Logger warning(const std::string &tag);
		static Logger notice(const std::string &tag);
		static Logger info(const std::string &tag);
		static Logger debug(const std::string &tag, int verbosity);

		/**
		 * Set the global verbosity of the logger.
		 * @param verbosity A verbosity level as number. Higher value leads to more output.
		 */
		static void setVerbosity(const int verbosity);

		/**
		 * Returns the global logging mask
		 */
		static unsigned char getLogMask();

		/**
		 * Get the global verbosity of the logger.
		 * @return The verbosity level as number. Higher value leads to more output.
		 */
		static int getVerbosity();

		/**
		 * Add a standard output stream to the logging framework.
		 * @param stream Standard output stream
		 * @param logmask This mask specify what will be written to this stream. You can combine options with the or function.
		 * @param options This mask specify what will be added to each log message. You can combine options with the or function.
		 */
		static void addStream(std::ostream &stream, const unsigned char logmask = LOGGER_INFO, const unsigned char options = LOG_NONE);

		/**
		 * Add a standard output stream to the logging framework.
		 * @param stream Standard output stream
		 */
		static void removeStream(std::ostream &stream);

		/**
		 * Set the logfile for the logging framework.
		 * @param logfile The file to log into.
		 * @param logmask This mask specify what will be written to this stream. You can combine options with the or function.
		 * @param options This mask specify what will be added to each log message. You can combine options with the or function.
		 */
		static void setLogfile(const ibrcommon::File &logfile, const unsigned char logmask = LOGGER_INFO, const unsigned char options = LOG_NONE);

		/**
		 * Enable logging message to the system syslog.
		 * @param name The naming prefix for all log messages.
		 * @param option Syslog specific options. @see syslog.h
		 * @param facility Syslog facility. @see syslog.h
		 * @param logmask This mask specify what will be written to the syslog. You can combine options with the or function.
		 */
		static void enableSyslog(const char *name, int option, int facility, const unsigned char logmask = LOGGER_INFO);

		/**
		 * enable the asynchronous logging
		 * This starts a separate thread and a thread-safe queue to
		 * queue all logging messages first and call the log routine by
		 * the thread. This option is necessary, if the stream to log into
		 * are not thread-safe by itself.
		 */
		static void enableAsync();

		/**
		 * Enables the internal ring-buffer.
		 * @param size The size of the buffer.
		 */
		static void enableBuffer(size_t size);

		/**
		 * Write the buffer to a stream
		 * @param
		 */
		static void writeBuffer(std::ostream&);

		/**
		 * stops the asynchronous logging thread
		 * you need to call this before your program is going down, if you have
		 * called enableAsync() before. 
		 */
		static void stop();

		/**
		 * Set the default tag for untagged logging
		 */
		static void setDefaultTag(const std::string &tag);

		/**
		 * Print the log message to the log output (or queue it in the writer)
		 */
		void print();
		
		/**
		 * Reload the logger. This re-opens the logfile for output.
		 */
		static void reload();

		/**
		 * Return the log level of this Logger
		 */
		LogLevel getLevel() const;

		/**
		 * Return the TAG of this Logger
		 */
		const std::string& getTag() const;

		/*
		 * Return the debug verbosity of this Logger
		 */
		int getDebugVerbosity() const;

		/**
		 * Return the Log entry of this Logger
		 */
		struct timeval getLogTime() const;

	private:
		Logger(LogLevel level, const std::string &tag, int debug_verbosity = 0);

		LogLevel _level;
		const std::string _tag;
		int _debug_verbosity;
		struct timeval _logtime;

		std::string _data;
	};

	class LogWriter : public ibrcommon::JoinableThread
	{
	public:
		static LogWriter& getInstance();

		virtual ~LogWriter();

		void log(Logger &logger);

		/**
		 * Set the global verbosity of the logger.
		 * @param verbosity A verbosity level as number. Higher value leads to more output.
		 */
		void setVerbosity(const int verbosity);

		/**
		 * Get the global verbosity of the logger.
		 * @return The verbosity level as number. Higher value leads to more output.
		 */
		int getVerbosity() const;


		unsigned char getLogMask() const;

		/**
		 * Add a standard output stream to the logging framework.
		 * @param stream Standard output stream
		 * @param logmask This mask specify what will be written to this stream. You can combine options with the or function.
		 * @param options This mask specify what will be added to each log message. You can combine options with the or function.
		 */
		void addStream(std::ostream &stream, const unsigned char logmask = Logger::LOGGER_INFO, const unsigned char options = Logger::LOG_NONE);

		/**
		 * Remove a standard output stream to the logging framework.
		 * @param stream Standard output stream
		 */
		void removeStream(std::ostream &stream);

		/**
		 * Set the logfile for the logging framework.
		 * @param logfile The file to log into.
		 * @param logmask This mask specify what will be written to this stream. You can combine options with the or function.
		 * @param options This mask specify what will be added to each log message. You can combine options with the or function.
		 */
		void setLogfile(const ibrcommon::File &logfile, const unsigned char logmask = Logger::LOGGER_INFO, const unsigned char options = Logger::LOG_NONE);

		/**
		 * Enable logging message to the system syslog.
		 * @param name The naming prefix for all log messages.
		 * @param option Syslog specific options. @see syslog.h
		 * @param facility Syslog facility. @see syslog.h
		 * @param logmask This mask specify what will be written to the syslog. You can combine options with the or function.
		 */
		void enableSyslog(const char *name, int option, int facility, const unsigned char logmask = Logger::LOGGER_INFO);

		/**
		 * enable the asynchronous logging
		 * This starts a seperate thread and a thread-safe queue to
		 * queue all logging messages first and call the log routine by
		 * the thread. This option is nessacary, if the stream to log into
		 * are not thread-safe by itself.
		 */
		void enableAsync();

		/**
		 * Enables the internal ring-buffer.
		 * @param size The size of the buffer.
		 */
		void enableBuffer(size_t size);

		/**
		 * Reload the logger. This re-opens the logfile for output.
		 */
		void reload();

		/**
		 * Write the buffer to a stream
		 * @param
		 */
		void writeBuffer(std::ostream&, const unsigned char logmask, const unsigned char options);

		/**
		 * Return the default tag
		 */
		const std::string& getDefaultTag() const;

		/**
		 * set the default tag to log
		 */
		void setDefaultTag(const std::string &value);

	protected:
		void flush();
		void run() noexcept;
		void __cancellation() noexcept;

	private:
		class LoggerOutput
		{
		public:
			LoggerOutput(std::ostream &stream, const unsigned char logmask, const unsigned char options);
			virtual ~LoggerOutput();

			void log(const Logger &log);

			std::ostream &_stream;
			unsigned char _level;
			unsigned char _options;
		};

		/**
		 * private constructor
		 */
		LogWriter();

		/**
		 * Flush the log message to the output/syslog.
		 */
		void flush(const Logger &logger);

		unsigned char _global_logmask;
		int _verbosity;
		bool _syslog;
		unsigned char _syslog_mask;

		ibrcommon::Queue<Logger> _queue;
		bool _use_queue;
		std::list<LoggerOutput> _logger;

		ibrcommon::Mutex _buffer_mutex;
		size_t _buffer_size;
		std::list<Logger> *_buffer;

		ibrcommon::Mutex _logfile_mutex;
		ibrcommon::File _logfile;
		std::ofstream _logfile_stream;
		LoggerOutput *_logfile_output;
		unsigned char _logfile_logmask;
		unsigned char _logfile_options;

		std::string _default_tag;
		std::string _android_tag_prefix;
	};
}

#endif /* LOGGER_H_ */
