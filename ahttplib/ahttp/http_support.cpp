/*
This file is part of [ahttp] library. 

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

// #if defined(WIN32)
// #define  BOOST_FILESYSTEM_DYN_LINK
// #endif

#include <algorithm>
#include <boost/filesystem.hpp>

#include "aconnect/time_util.hpp"

#include "ahttp/http_support.hpp"

namespace fs = boost::filesystem;

namespace ahttp { 
	namespace detail
{


	// sample: Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	aconnect::string formatDate_RFC1123 (const tm& dateTime) 
	{
		aconnect::char_type buff[32] = {0};

		int cnt = snprintf (buff, 32, "%s, %.2d %s %.4d %.2d:%.2d:%.2d GMT", 
			WeekDays_RFC1123[dateTime.tm_wday],
			dateTime.tm_mday,
			Months_RFC1123[dateTime.tm_mon],

			dateTime.tm_year + 1900,
			dateTime.tm_hour,
			dateTime.tm_min,
			dateTime.tm_sec
			);

		return aconnect::string (buff, cnt);
	}

	bool sortWdByTypeAndName (const WebDirectoryItem& item1, const WebDirectoryItem& item2)
	{
		if (item1.type != item2.type)
			return item1.type < item2.type;
		return item1.name < item2.name;
	}

	void readDirectoryContent (string_constref dirPath,
							   string_constref dirVirtualPath,
							   std::vector<WebDirectoryItem> &items, 
							   aconnect::Logger& logger,
							   size_t &errCount,
							   WebDirectorySortType sortType)
	{

		aconnect::ProgressTimer progress (logger, __FUNCTION__);

		fs::directory_iterator endTter;
		for ( fs::directory_iterator dirIter (dirPath);
			dirIter != endTter;
			++dirIter )
		{
			try
			{
				WebDirectoryItem item;

				item.name = dirIter->path().leaf();
				item.lastWriteTime = fs::last_write_time (dirIter->path());

				if ( fs::is_directory( dirIter->status() ) ) {
					item.type = WdDirectory;
					item.url = dirVirtualPath + item.name + Slash;

				} else  {
					item.type = WdFile;
					item.url = dirVirtualPath + item.name;
					item.size = fs::file_size (dirIter->path());
				}

				items.push_back (item);

			} catch ( const std::exception& ex ) {
				logger.error ("Exception caught at directory \"%s\" content loading (%s): %s", 
					dirVirtualPath.c_str(),
					typeid(ex).name(), ex.what());
				++errCount;
			}

			if (sortType == WdSortByTypeAndName)
				std::sort(items.begin(), items.end(), sortWdByTypeAndName);
		}

	}
}}

