/*
 * Logger.cpp
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

#include "ibrcommon/config.h"
#include "ibrcommon/Logger.h"
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/MutexLock.h>

#include <algorithm>
#include <sys/time.h>
#include <iomanip>
#include <vector>
#include <unistd.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef __WIN32__
#include <winsock2.h>
#endif

namespace ibrcommon
{
	Logger::Logger(LogLevel level, const std::string &tag, int debug_verbosity)
	 : _level(level), _tag(tag), _debug_verbosity(debug_verbosity)
	{
		::gettimeofday(&_logtime, NULL);
	}

	Logger::Logger(const Logger &obj)
	 : _level(obj._level), _tag(obj._tag), _debug_verbosity(obj._debug_verbosity), _logtime(obj._logtime), _data(obj._data)
	{
	}

	Logger::~Logger()
	{
	}

	void Logger::setMessage(const std::string &data)
	{
		_data = data;
	}

	const std::string& Logger::str() const
	{
		return _data;
	}

	Logger Logger::emergency(const std::string &tag)
	{
		return Logger(LOGGER_EMERG, tag);
	}

	Logger Logger::alert(const std::string &tag)
	{
		return Logger(LOGGER_ALERT, tag);
	}

	Logger Logger::critical(const std::string &tag)
	{
		return Logger(LOGGER_CRIT, tag);
	}

	Logger Logger::error(const std::string &tag)
	{
		return Logger(LOGGER_ERR, tag);
	}

	Logger Logger::warning(const std::string &tag)
	{
		return Logger(LOGGER_WARNING, tag);
	}

	Logger Logger::notice(const std::string &tag)
	{
		return Logger(LOGGER_NOTICE, tag);
	}

	Logger Logger::info(const std::string &tag)
	{
		return Logger(LOGGER_INFO, tag);
	}

	Logger Logger::debug(const std::string &tag, int verbosity)
	{
		return Logger(LOGGER_DEBUG, tag, verbosity);
	}

	void Logger::setVerbosity(const int verbosity)
	{
		LogWriter::getInstance().setVerbosity(verbosity);
	}

	void LogWriter::setVerbosity(const int verbosity)
	{
		_verbosity = verbosity;
	}

	unsigned char Logger::getLogMask()
	{
		return LogWriter::getInstance().getLogMask();
	}

	int Logger::getVerbosity()
	{
		return LogWriter::getInstance().getVerbosity();
	}

	unsigned char LogWriter::getLogMask() const
	{
		return _global_logmask;
	}

	int LogWriter::getVerbosity() const
	{
		return _verbosity;
	}

	void Logger::addStream(std::ostream &stream, const unsigned char logmask, const unsigned char options)
	{
		LogWriter::getInstance().addStream(stream, logmask, options);
	}

	void Logger::removeStream(std::ostream &stream)
	{
		LogWriter::getInstance().removeStream(stream);
	}

	void Logger::setLogfile(const ibrcommon::File &logfile, const unsigned char logmask, const unsigned char options)
	{
		LogWriter::getInstance().setLogfile(logfile, logmask, options);
	}

	void LogWriter::addStream(std::ostream &stream, const unsigned char logmask, const unsigned char options)
	{
		_global_logmask |= logmask;
		_logger.push_back( LogWriter::LoggerOutput(stream, logmask, options) );
	}

	void LogWriter::removeStream(std::ostream &stream)
	{
		for (std::list<LoggerOutput>::iterator iter = _logger.begin(); iter != _logger.end(); ++iter)
		{
			const LoggerOutput &output = (*iter);

			if ((void*)&stream == (void*)(&(output._stream)))
			{
				iter = _logger.erase(iter);
			}
		}
	}

	void Logger::enableSyslog(const char *name, int option, int facility, const unsigned char logmask)
	{
		LogWriter::getInstance().enableSyslog(name, option, facility, logmask);
	}

	void Logger::print()
	{
		LogWriter::getInstance().log(*this);
	}

	void LogWriter::enableSyslog(const char *name, int option, int facility, const unsigned char logmask)
	{
#if ( defined HAVE_SYSLOG_H || defined ANDROID )
#ifdef HAVE_SYSLOG_H
		// init syslog
		::openlog(name, option, facility);
#endif
		_syslog = true;
		_syslog_mask = logmask;
		_global_logmask |= logmask;
#endif
	}

	void LogWriter::flush(const Logger &logger)
	{
		if (_verbosity >= logger.getDebugVerbosity())
		{
			for (std::list<LoggerOutput>::iterator iter = _logger.begin(); iter != _logger.end(); ++iter)
			{
				LoggerOutput &output = (*iter);
				output.log(logger);
			}

			// output to logfile
			{
				ibrcommon::MutexLock l(_logfile_mutex);
				if (_logfile_output != NULL)
				{
					_logfile_output->log(logger);
				}
			}

#if ( defined HAVE_SYSLOG_H || defined ANDROID )
			// additionally log to the syslog/android logcat
			if (_syslog)
			{
				if (logger.getLevel() & _syslog_mask)
				{
#ifdef ANDROID
					std::string log_tag = LogWriter::getInstance().getDefaultTag();
					if (logger.getTag().length() > 0) {
						log_tag = logger.getTag();
					}

					// add additional prefix for android tags to seperate them from other logcat messages
					log_tag = _android_tag_prefix + log_tag;

					switch (logger.getLevel())
					{
					case Logger::LOGGER_EMERG:
						__android_log_print(ANDROID_LOG_FATAL, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					case Logger::LOGGER_ALERT:
						__android_log_print(ANDROID_LOG_FATAL, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					case Logger::LOGGER_CRIT:
						__android_log_print(ANDROID_LOG_FATAL, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					case Logger::LOGGER_ERR:
						__android_log_print(ANDROID_LOG_ERROR, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					case Logger::LOGGER_WARNING:
						__android_log_print(ANDROID_LOG_WARN, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					case Logger::LOGGER_NOTICE:
						__android_log_print(ANDROID_LOG_INFO, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					case Logger::LOGGER_INFO:
						__android_log_print(ANDROID_LOG_INFO, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					case Logger::LOGGER_DEBUG:
						__android_log_print(ANDROID_LOG_DEBUG, log_tag.c_str(), "%s", logger.str().c_str());
						break;

					default:
						__android_log_print(ANDROID_LOG_INFO, log_tag.c_str(), "%s", logger.str().c_str());
						break;
					}
#else
					switch (logger.getLevel())
					{
					case Logger::LOGGER_EMERG:
						::syslog( LOG_EMERG, "%s", logger.str().c_str() );
						break;

					case Logger::LOGGER_ALERT:
						::syslog( LOG_ALERT, "%s", logger.str().c_str() );
						break;

					case Logger::LOGGER_CRIT:
						::syslog( LOG_CRIT, "%s", logger.str().c_str() );
						break;

					case Logger::LOGGER_ERR:
						::syslog( LOG_ERR, "%s", logger.str().c_str() );
						break;

					case Logger::LOGGER_WARNING:
						::syslog( LOG_WARNING, "%s", logger.str().c_str() );
						break;

					case Logger::LOGGER_NOTICE:
						::syslog( LOG_NOTICE, "%s", logger.str().c_str() );
						break;

					case Logger::LOGGER_INFO:
						::syslog( LOG_INFO, "%s", logger.str().c_str() );
						break;

					case Logger::LOGGER_DEBUG:
						::syslog( LOG_DEBUG, "%s", logger.str().c_str() );
						break;

					default:
						::syslog( LOG_NOTICE, "%s", logger.str().c_str() );
						break;
					}
#endif
				}
			}
#endif
		}
	}

	void LogWriter::LoggerOutput::log(const Logger &log)
	{
		if (_level & log.getLevel())
		{
			std::list<std::string> prefixes;

			// check for prefixes
			if (_options & Logger::LOG_DATETIME)
			{
				// get timestamp
				time_t secs = log.getLogTime().tv_sec;
				std::string timestamp(asctime( localtime(&secs) ));
				timestamp.erase(std::remove(timestamp.begin(), timestamp.end(), '\n'), timestamp.end());
				prefixes.push_back(timestamp);
			}

			// check for prefixes
			if (_options & Logger::LOG_TIMESTAMP)
			{
				std::stringstream ss;
				ss.fill('0');
				ss << log.getLogTime().tv_sec << "." << std::setw(6) << log.getLogTime().tv_usec;
				prefixes.push_back(ss.str());
			}

			if (_options & Logger::LOG_LEVEL)
			{
				// print log level
				switch (log.getLevel())
				{
					case Logger::LOGGER_EMERG:
						prefixes.push_back("EMERGENCY");
						break;

					case Logger::LOGGER_ALERT:
						prefixes.push_back("ALERT");
						break;

					case Logger::LOGGER_CRIT:
						prefixes.push_back("CRTITICAL");
						break;

					case Logger::LOGGER_ERR:
						prefixes.push_back("ERROR");
						break;

					case Logger::LOGGER_WARNING:
						prefixes.push_back("WARNING");
						break;

					case Logger::LOGGER_NOTICE:
						prefixes.push_back("NOTICE");
						break;

					case Logger::LOGGER_INFO:
						prefixes.push_back("INFO");
						break;

					case Logger::LOGGER_DEBUG:
					{
						std::stringstream ss;
						ss << "DEBUG." << log.getDebugVerbosity();
						prefixes.push_back(ss.str());
						break;
					}

					default:
						break;
				}
			}

			if (_options & Logger::LOG_HOSTNAME)
			{
				std::vector<char> hostname_array(64);
				if ( gethostname(&hostname_array[0], 64) == 0 )
				{
					std::string hostname(&hostname_array[0]);
					prefixes.push_back(hostname);
				}
			}

			if (_options & Logger::LOG_TAG)
			{
				if (log.getTag().length() > 0) {
					prefixes.push_back(log.getTag());
				} else {
					prefixes.push_back(LogWriter::getInstance().getDefaultTag());
				}
			}

			// print prefixes
			for (std::list<std::string>::const_iterator iter = prefixes.begin(); iter != prefixes.end(); ++iter)
			{
				if (iter == prefixes.begin())
				{
					_stream << (*iter);
				}
				else
				{
					_stream << " " << (*iter);
				}
			}

			if (!prefixes.empty())
			{
				_stream << ": ";
			}

			_stream << log.str() << std::endl;
		}
	}

	LogWriter::LoggerOutput::LoggerOutput(std::ostream &stream, const unsigned char logmask, const unsigned char options)
	 : _stream(stream), _level(logmask), _options(options)
	{
	}

	LogWriter::LoggerOutput::~LoggerOutput()
	{
	}

	void Logger::enableAsync()
	{
		LogWriter::getInstance().enableAsync();
	}

	void Logger::enableBuffer(size_t size)
	{
		LogWriter::getInstance().enableBuffer(size);
	}

	void Logger::writeBuffer(std::ostream &stream)
	{
		unsigned char logmask = ibrcommon::Logger::LOGGER_ALL;
		unsigned char options = ibrcommon::Logger::LOG_DATETIME | ibrcommon::Logger::LOG_LEVEL;

		LogWriter::getInstance().writeBuffer(stream, logmask, options);
	}

	void LogWriter::enableAsync()
	{
		try {
			_use_queue = true;
			start();
		} catch (const ibrcommon::ThreadException &ex) {
			IBRCOMMON_LOGGER_TAG("LogWriter", error) << "enableAsync failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
		}
	}
	
	void Logger::setDefaultTag(const std::string &tag)
	{
		LogWriter::getInstance().setDefaultTag(tag);
	}

	void Logger::stop()
	{
		// stop the LogWriter::run() thread (this is needed with uclibc)
		LogWriter::getInstance().stop();
	}

	void Logger::reload()
	{
		//reload the logger
		LogWriter::getInstance().reload();
	}

	Logger::LogLevel Logger::getLevel() const
	{
		return _level;
	}

	const std::string& Logger::getTag() const
	{
		return _tag;
	}

	int Logger::getDebugVerbosity() const
	{
		return _debug_verbosity;
	}

	struct timeval Logger::getLogTime() const
	{
		return _logtime;
	}

	LogWriter& LogWriter::getInstance()
	{
		static LogWriter instance;
		return instance;
	}

	LogWriter::LogWriter()
	 : _global_logmask(0), _verbosity(0), _syslog(0), _syslog_mask(0), _queue(50), _use_queue(false), _buffer_size(0), _buffer(NULL),
	   _logfile_output(NULL), _logfile_logmask(0), _logfile_options(0), _default_tag("Core"), _android_tag_prefix("IBR-DTN/")
	// limit queue to 50 entries
	{
	}

	LogWriter::~LogWriter()
	{
		// do cleanup only if the thread was running before
		if (_use_queue) stop();

		// join the LogWriter::run() thread
		join();

		// remove the ring-buffer
		ibrcommon::MutexLock l(_buffer_mutex);
		if (_buffer != NULL) delete _buffer;
	}

	void LogWriter::enableBuffer(size_t size)
	{
		ibrcommon::MutexLock l(_buffer_mutex);
		_buffer_size = size;
		_buffer = new std::list<Logger>();
	}

	void LogWriter::setLogfile(const ibrcommon::File &logfile, const unsigned char logmask, const unsigned char options)
	{
		ibrcommon::MutexLock l(_logfile_mutex);
		if (_logfile_output != NULL)
		{
			// delete the old logger
			delete _logfile_output;
			_logfile_output = NULL;

			// close the output file
			_logfile_stream.close();
		}

		// stop here if the logmask is zero
		// this can be used to disable file logging
		if (logmask == 0) return;

		_logfile = logfile;
		_logfile_logmask = logmask;
		_logfile_options = options;
		_global_logmask |= logmask;

		// open the new logfile
		_logfile_stream.open(_logfile.getPath().c_str(), std::ios::out | std::ios::app);

		// create a new logger output
		if (_logfile_stream.good())
		{
			_logfile_output = new LoggerOutput(_logfile_stream, _logfile_logmask, _logfile_options);
		}
		else
		{
			_logfile_stream.close();
		}
	}

	void LogWriter::reload()
	{
		ibrcommon::MutexLock l(_logfile_mutex);
		if (_logfile_output != NULL)
		{
			// delete the old logger
			delete _logfile_output;
			_logfile_output = NULL;

			// close the output file
			_logfile_stream.close();

			// open the new logfile
			_logfile_stream.open(_logfile.getPath().c_str(), std::ios::out | std::ios::app);

			// create a new logger output
			if (_logfile_stream.good())
			{
				_logfile_output = new LoggerOutput(_logfile_stream, _logfile_logmask, _logfile_options);
			}
			else
			{
				_logfile_stream.close();
			}
		}
	}

	void LogWriter::writeBuffer(std::ostream &stream, const unsigned char logmask, const unsigned char options)
	{
		ibrcommon::MutexLock l(_buffer_mutex);
		if (_buffer == NULL) return;

		LoggerOutput output(stream, logmask, options);

		for (std::list<Logger>::const_iterator iter = _buffer->begin(); iter != _buffer->end(); ++iter)
		{
			const Logger &l = (*iter);
			output.log(l);
		}
	}

	const std::string& LogWriter::getDefaultTag() const
	{
		return _default_tag;
	}

	void LogWriter::setDefaultTag(const std::string &value)
	{
		_default_tag = value;
	}

	void LogWriter::log(Logger &logger)
	{
		if (_use_queue)
		{
			_queue.push(logger);
		}
		else
		{
			flush(logger);
		}
	}

	void LogWriter::run() noexcept
	{
		try {
			while (true)
			{
				Logger log = _queue.poll();
				flush(log);

				// add to ring-buffer
				ibrcommon::MutexLock l(_buffer_mutex);
				if (_buffer != NULL)
				{
					_buffer->push_back(log);
					while (_buffer->size() > _buffer_size)
					{
						_buffer->pop_front();
					}
				}
			}
		} catch (const std::exception&) {
			ibrcommon::Queue<Logger>::Locked q = _queue.exclusive();

			try {
				// In this block we will write all remaining element in the queue
				// to the logging streams. While we do this, the queue should be locked
				// and finally we abort the queue to unblock all waiting threads.
				while (!q.empty())
				{
					Logger &log = q.front();
					try { flush(log); } catch (const std::exception&) {};

					q.pop();
				}
			} catch (const std::exception&) {

			}
		}
	}

	void LogWriter::__cancellation() noexcept
	{
		// cancel the main thread in here
		_queue.abort();
	}
}
