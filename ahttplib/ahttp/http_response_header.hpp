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

#ifndef AHTTP_RESPONSE_HEADER_H
#define AHTTP_RESPONSE_HEADER_H

#pragma once
#include <boost/utility.hpp>

#include "aconnect/types.hpp"
#include "aconnect/complex_types.hpp"

namespace ahttp
{
	class HttpResponseHeader : private boost::noncopyable
	{
	public:
		static const int UnknownStatus = -1;

		// properties
		aconnect::str2str_map Headers;
		int Status;

	public:
		// methods
		HttpResponseHeader () : Status(UnknownStatus) {	}

		void clear () 
		{
			Status = UnknownStatus;
			Headers.clear ();
		}

		aconnect::string getContent ();
		void setContentLength (size_t length);
		void setContentType (aconnect::string_constref contentType, aconnect::string_constref charset = "");

		static aconnect::string getResponseStatusString (int status);

		// inlines
		inline bool hasHeader (aconnect::string_constref headerName) const {
			return (Headers.find (headerName) != Headers.end());
		}

	};
}

#endif // AHTTP_RESPONSE_HEADER_H


