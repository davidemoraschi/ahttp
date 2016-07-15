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

#include <boost/lexical_cast.hpp>

#include "ahttp/http_support.hpp"
#include "ahttp/http_response_header.hpp"

namespace ahttp
{

	aconnect::string HttpResponseHeader::getContent ()
	{
		using namespace aconnect;
		aconnect::str_stream content;
		content << HttpResponseHeader::getResponseStatusString (Status);

		for (str2str_map::iterator it = Headers.begin(); it != Headers.end(); it++)
		{
			content << it->first << detail::HeaderValueDelimiter <<
				it->second << detail::HeadersDelimiter;
		}

		content << detail::HeadersDelimiter;
		return content.str();
	}

	void HttpResponseHeader::setContentLength (size_t length) 
	{
		Headers[detail::HeaderContentLength] = boost::lexical_cast<aconnect::string> (length);
	}
	void HttpResponseHeader::setContentType (aconnect::string_constref contentType, aconnect::string_constref charset) {
		if (charset.empty()) 
			Headers[detail::HeaderContentType] = contentType;
		else 
			Headers[detail::HeaderContentType] = contentType + "; charset=" + charset;
	}

	aconnect::string HttpResponseHeader::getResponseStatusString (int status)
	{
		aconnect::str_stream ret;

		ret << detail::HttpVersion << " " << status
			<< " " << detail::httpStatusDesc(status) << detail::HeadersDelimiter;

		return ret.str();
	}

}

