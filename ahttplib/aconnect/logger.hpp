/*
This file is part of [aconnect] library. 

Author: Artem Kustikov (kustikoff[at]tut.by)
version: 0.1

This code is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this code.

Permission is granted to anyone to use this code for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this code must not be misrepresented; you must
not claim that you wrote the original code. If you use this
code in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original code.

3. This notice may not be removed or altered from any source
distribution.
*/

#ifndef ACONNECT_LOGGER_H
#define ACONNECT_LOGGER_H

#include <fstream>
#include <boost/timer.hpp>
#include <boost/thread.hpp>

namespace aconnect
{
	namespace Log
	{
		enum LogLevel {
			Debug = 3,
			Info = 2,
			Warning = 1,
			Error = 0
		};

		string_constant DebugMsg = "Debug";
		string_constant InfoMsg = "Info";
		string_constant WarningMsg = "Warning";
		string_constant ErrorMsg = "Error";

		string_constant TimeStampMark = "{timestamp}";
		const size_t MaxFileSize = 4 * 1048576; // 4 Mb
	};

	
	//////////////////////////////////////////////////////////////////////////
	//
	//				abstract class Logger
	//
	//////////////////////////////////////////////////////////////////////////
	class Logger
	{

	protected:
		boost::mutex	mutex_;
		Log::LogLevel	level_;
		
		virtual void writeMessage (string_constref msg) = 0;
		virtual bool valid();
				
	public:


		Logger () : level_ (Log::Warning) { };
		Logger (Log::LogLevel level) : level_ (level) { };
        virtual ~Logger ()  { };
		
		virtual void processMessage (Log::LogLevel level, string_constptr msg);

		inline void debug (string_constptr format, ...)	
		{ 
			if (!isDebugEnabled())
				return;
			FORMAT_VA_MESSAGE (format, formattedMessage); 
			processMessage (Log::Debug, formattedMessage.c_str());	
		}

		inline void info  (string_constptr format, ...)	
		{ 
			if (!isInfoEnabled())
				return;
			FORMAT_VA_MESSAGE (format, formattedMessage); processMessage (Log::Info, 
			formattedMessage.c_str());	
		}

		inline void warn  (string_constptr format, ...)	
		{ 
			if (!isWarningEnabled())
				return;
			FORMAT_VA_MESSAGE (format, formattedMessage); 
			processMessage (Log::Warning, formattedMessage.c_str());	
		}

		inline void error (string_constptr format, ...)	
		{ 
			FORMAT_VA_MESSAGE (format, formattedMessage); 
			processMessage (Log::Error, formattedMessage.c_str());	
		}
		
		inline void error (const std::exception &ex)		{	processMessage (Log::Error, ex.what());	}
		
		inline bool isDebugEnabled()						{	return level_>=Log::Debug;				}
		inline bool isInfoEnabled()							{	return level_>=Log::Info;				}
		inline bool isWarningEnabled()						{	return level_>=Log::Warning;			}
	};

	class FakeLogger : public Logger
	{
	public:
		virtual bool valid() {	return false; };
	protected:
		virtual void writeMessage (string_constref msg) {}
	};

	class ConsoleLogger : public Logger
	{
	protected:
		virtual void writeMessage (string_constref msg);
	public:
		ConsoleLogger (Log::LogLevel level) : Logger (level) {
			
		}
	};

	class FileLogger : public Logger
	{
	protected:
		virtual void writeMessage (string_constref msg);
	public:
		
		FileLogger () : outputSize_ (0), maxFileSize_(0) {}
		virtual ~FileLogger ();
		
		// filePathTemplate can contain "{timestamp}" mark to replace it with timestamp,
		// if it is omitted then timestamp will be added to the end of file
		void init (Log::LogLevel level, string_constptr filePathTemplate,
			size_t maxFileSize = Log::MaxFileSize) throw (std::runtime_error);

		void destroy ();
		virtual bool valid();

	protected:
		void closeWriter ();
		void createLogFile () throw (std::runtime_error);
		string generateTimeStamp();
	
	protected:
		string filePathTemplate_;
		std::ofstream output_;
		size_t outputSize_;
		size_t maxFileSize_;
	};

	class ProgressTimer 
	{
	public:
		ProgressTimer (Logger& log, string_constptr funcName, Log::LogLevel level = Log::Debug):
		  log_ (log), funcName_(funcName), level_(level),  timer_() {}
		~ProgressTimer ();

	protected:
		Logger& log_;
		string funcName_;
		Log::LogLevel level_;
		boost::timer timer_;

	};
}


#endif // ACONNECT_LOGGER_H
