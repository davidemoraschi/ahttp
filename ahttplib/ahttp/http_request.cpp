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

#include <assert.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "aconnect/util.hpp"
#include "aconnect/network.hpp"

#include "ahttp/http_support.hpp"
#include "ahttp/http_request.hpp"

namespace algo = boost::algorithm;

namespace ahttp
{
	void HttpRequestHeader::clear()
	{
		Headers.clear ();

		VersionHigh = VersionLow = 0;
		ContentLength = 0;

		Method.clear ();
		Path.clear ();
	}

	void HttpRequestHeader::load (aconnect::string_constref headerBody) 
		throw (aconnect::request_processing_error)
	{
		assert ( headerBody.length() );

		using namespace aconnect;
		
		str_vector lines, reqLineParts;
		algo::split (lines, headerBody, algo::is_any_of("\r\n"), algo::token_compress_on);
		algo::split (reqLineParts, lines[0], algo::is_space(), algo::token_compress_on);

		// Parse Request line
		Method = reqLineParts[0];
		Path = reqLineParts[1];

		size_t pos;
		const size_t offset = strlen ("HTTP/");

		if ((pos = reqLineParts[2].find ('.', offset)) != string::npos) {
			VersionHigh = boost::lexical_cast<int> (reqLineParts[2].substr (offset, pos - offset));
			VersionLow = boost::lexical_cast<int> (reqLineParts[2].substr (pos + 1));

		} else {
			VersionHigh = boost::lexical_cast<int> (reqLineParts[2].substr(offset));
			VersionLow = 0;
		}

		if (lines.size() == 1)
			return;

		for (str_vector::iterator it = lines.begin()+1 ; it != lines.end(); ++it) {
			pos = it->find (':');
			if (pos == string::npos) 
				throw aconnect::request_processing_error ("Incorrect request header: %s", it->c_str());
				
			loadHeader (it->substr (0, pos), algo::trim_copy(it->substr (pos + 1)));
		}

	}

	void HttpRequestHeader::loadHeader (aconnect::string_constref name, aconnect::string_constref value) {
		using namespace aconnect;
		
		if ( util::equals (name, detail::HeaderContentLength) )
			ContentLength = boost::lexical_cast<size_t> (value);
		else
			Headers [name] = value;
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//		HttpRequestStream
	//
	//////////////////////////////////////////////////////////////////////////

	void HttpRequestStream::init (aconnect::string& requestBodyBegin, 
								 int contentLength, aconnect::socket_type sock) 
	{
		ContentLength = contentLength;
		socket_ = sock;
		loadedContentLength_ = 0;

		if (contentLength > 0) 
			requestBodyBegin_.swap (requestBodyBegin);
	}

	int HttpRequestStream::read (aconnect::string_ptr buff, int buffSize) throw (aconnect::socket_error)
	{
		if (0 == ContentLength)
			return 0;

		// read from buffer
		if (!requestBodyBegin_.empty()) {
			int copied = (int) requestBodyBegin_.copy(buff, aconnect::util::min2 ( buffSize, 
				(int) requestBodyBegin_.length()));

			requestBodyBegin_.erase(0, copied);
			loadedContentLength_ += copied;

			return copied;
		}

		if (loadedContentLength_ == ContentLength)
			return 0;

		// read from socket
		if (buffSize > (ContentLength - loadedContentLength_))
			buffSize = ContentLength - loadedContentLength_;

		int bytesRead = recv (socket_, buff, buffSize, 0);
		if (bytesRead == SOCKET_ERROR)
			throw aconnect::socket_error (socket_, "HTTP request: reading data from socket failed");

		loadedContentLength_ += bytesRead;

		return bytesRead;
	}
}
