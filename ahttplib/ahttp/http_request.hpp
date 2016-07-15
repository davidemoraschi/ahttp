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

#ifndef AHTTP_REQUEST_H
#define AHTTP_REQUEST_H
#pragma once

#include <boost/utility.hpp>

#include "aconnect/types.hpp"
#include "aconnect/complex_types.hpp"

namespace ahttp
{
	class HttpRequestHeader : private boost::noncopyable
	{

	public:
		aconnect::str2str_map Headers;

		int VersionHigh, VersionLow;
		size_t ContentLength;				// Content-Length for POST

		aconnect::string Method;
		aconnect::string Path;		// path to source - with query string...

	public:
		HttpRequestHeader () : VersionHigh(0), VersionLow(0), ContentLength (0) {

		}

		void load (aconnect::string_constref headerBody) throw (aconnect::request_processing_error);
		void clear ();
		
		inline bool hasHeader (aconnect::string_constref headerName) const {
			return (Headers.find (headerName) != Headers.end());
		}
		inline aconnect::string getHeader (aconnect::string_constref headerName) const {
			aconnect::str2str_map::const_iterator iter = Headers.find (headerName);
			if (iter == Headers.end())
				return "";
			return iter->second;
		}

		inline aconnect::string operator[] (aconnect::string_constref headerName) const {
			return getHeader (headerName);
		}

	
	protected:
		void loadHeader (aconnect::string_constref name, aconnect::string_constref value);
	};


	class HttpRequestStream : private boost::noncopyable
	{
	public:
		HttpRequestStream () : 
			ContentLength(0), 
			socket_ (INVALID_SOCKET),
			loadedContentLength_ (0)	
			{};
		
		void init (aconnect::string& requestBodyBegin, int contentLength, aconnect::socket_type socket);
		int read (aconnect::string_ptr buff, int buffSize) throw (aconnect::socket_error);
		
		inline void clear() {
			ContentLength = 0;
			requestBodyBegin_.clear();
		}

		inline bool hasBufferedContent()		{	return !requestBodyBegin_.empty(); }
		inline bool isRead()					{	return loadedContentLength_ == ContentLength; }
		inline aconnect::socket_type socket()	{	return socket_; }

	public:
		int ContentLength;

	protected:
		aconnect::string requestBodyBegin_;
		aconnect::socket_type socket_;
		int loadedContentLength_;

	};
}
#endif // AHTTP_REQUEST_H

