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

#ifndef ACONNECT_TIME_H
#define ACONNECT_TIME_H

#if defined(WIN32)
#	include <ctime>
#elif defined(__GNUC__)
#	include <sys/time.h>
#endif

#include <boost/thread.hpp>
#include "types.hpp"

namespace aconnect 
{
	namespace util 
	{

		inline tm getDateTime (time_t timeToConv = time ( NULL )) throw (std::runtime_error)
		{
			struct tm tmTime;
		#ifdef WIN32
			errno_t res = localtime_s (&tmTime, &timeToConv);
			if (res != 0) {
				char_type buff[1024];
				_snprintf_s (buff, 1024, _TRUNCATE, "Invalid argument to localtime_s: %d, code: %d.", timeToConv, res);
				throw std::runtime_error ( buff );
			}
		#else
			tmTime = *localtime (&timeToConv);
		#endif    
			return tmTime;
		}


		inline tm getDateTimeUtc (time_t timeToConv = time ( NULL )) throw (std::runtime_error)
		{
			struct tm tmTime;
#ifdef WIN32
			errno_t res = gmtime_s (&tmTime, &timeToConv);
			if (res != 0) {
				char_type buff[1024];
				_snprintf_s (buff, 1024, _TRUNCATE, "Invalid argument to gmtime_s: %d, code: %d.", timeToConv, res);
				throw std::runtime_error ( buff );
			}
#else
			tmTime = *gmtime (&timeToConv);
#endif    
			return tmTime;
		}
		
		inline boost::xtime createTimePeriod (boost::xtime::xtime_sec_t sec, 
											  boost::xtime::xtime_nsec_t nsec = 0) 
		{
		
			boost::xtime xt;
			boost::xtime_get(&xt, boost::TIME_UTC);
			xt.sec += sec; 
			xt.nsec += nsec; 
		
			return xt;
		}

	}
}

#endif // ACONNECT_TIME_H
