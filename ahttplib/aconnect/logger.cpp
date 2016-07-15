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

#include "boost_format_safe.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <iostream>

#include "util.hpp"
#include "time_util.hpp"
#include "logger.hpp"

namespace aconnect
{
	bool Logger::valid()	{	
		return true; 
	}


	void Logger::processMessage (Log::LogLevel level, string_constptr msg)
	{
		if(!valid())
			return;

		if (level > level_)
			return;

		using boost::format;

		struct tm tmTime = util::getDateTime();
		
		format f("[%02.d-%02.d-%02.d %02.d:%02.d:%02.d] %6.d %s: %s");
		f % tmTime.tm_mday;
		f % (tmTime.tm_mon + 1);
		f % (tmTime.tm_year + 1900);

		f % (tmTime.tm_hour);
		f % (tmTime.tm_min);
		f % (tmTime.tm_sec);

		f % (util::getCurrentThreadId());

		if (level == Log::Debug)
			f % Log::DebugMsg;
		else if (level == Log::Info)
			f % Log::InfoMsg;
		else if (level == Log::Warning)
			f % Log::WarningMsg;
		else 
			f % Log::ErrorMsg;

		if (msg )
			f % msg;
		else
			f % "";
			
		writeMessage ( f.str() );
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//		ConsoleLogger
	//
	void ConsoleLogger::writeMessage (string_constref msg)
	{
		boost::mutex::scoped_lock lock (mutex_);
		std::cout << msg << std::endl;
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//		FileLogger
	//

	bool FileLogger::valid()
	{	
		return output_.is_open() && !output_.fail(); 
	}

	void FileLogger::init (Log::LogLevel level, string_constptr filePathTemplate, size_t maxFileSize)  throw (std::runtime_error) 
	{
		level_ = level;
		maxFileSize_ = maxFileSize;

		if (util::isNullOrEmpty (filePathTemplate))
			throw std::runtime_error ("Log file name template is null or empty");

		filePathTemplate_ = filePathTemplate;
		createLogFile ();
	}

	FileLogger::~FileLogger() {
		destroy();
	}

	void FileLogger::closeWriter () {
		if (valid()) 
			output_.flush();
		output_.rdbuf()->close();
	}

	void FileLogger::destroy() {
		closeWriter ();
		output_.close();
	}

	string FileLogger::generateTimeStamp() {
		using boost::format;

		struct tm tmTime = util::getDateTime();

		format f("%02.d_%02.d_%02.d_%02.d_%02.d_%02.d");
		f % tmTime.tm_mday % (tmTime.tm_mon + 1) % (tmTime.tm_year + 1900);
		f % (tmTime.tm_hour) % (tmTime.tm_min) % (tmTime.tm_sec);

		return str(f); 
	}


	void FileLogger::createLogFile()  throw (std::runtime_error)
	{
		namespace fs = boost::filesystem;
		using boost::format;

		string ts = generateTimeStamp();
		string fileNameInit;
		if (filePathTemplate_.find(Log::TimeStampMark) != string::npos ) 
			fileNameInit = boost::algorithm::replace_all_copy(filePathTemplate_, Log::TimeStampMark, ts);
		else
			fileNameInit = filePathTemplate_ + ts;
		
		fs::path fileName (fileNameInit);
		const string ext = fs::extension (fileName);
		format extFormat (".%06.d" + ext);

		int ndx = 0;
		while ( fs::exists (fileName) ) {
			extFormat % ndx;
			fileName = fs::change_extension (fs::path (fileNameInit), 
				extFormat.str() ) ;

			extFormat.clear();
			++ndx;
		}

		closeWriter ();
		output_.open ( fileName.file_string().c_str(), std::ios::out | std::ios::binary );

		if (output_.fail())
			throw std::runtime_error ( str ( format("Cannot create \"%s\" log file") % fileName.file_string().c_str()) );
	}

	void FileLogger::writeMessage (string_constref msg)
	{
		boost::mutex::scoped_lock lock (mutex_);
		if(!valid())
			return;
		
		outputSize_ += msg.size();
		output_ << msg << std::endl;

		if (output_.fail())
			throw std::runtime_error ("Error writing log file");

		if (outputSize_ >= maxFileSize_) {
			createLogFile ();
			outputSize_ = 0;
		}

	}


	ProgressTimer::~ProgressTimer () {
		try 
		{
			using boost::format;
			
			format f("%s: elapsed time - %f sec");
			f % funcName_ % timer_.elapsed();
			
			log_.processMessage (level_, f.str().c_str());

		} catch (...) {
			// eat exception
		}
	}

}
