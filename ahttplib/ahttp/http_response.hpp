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

#ifndef AHTTP_RESPONSE_H
#define AHTTP_RESPONSE_H
#pragma once
#include <boost/utility.hpp>

#include "aconnect/types.hpp"
#include "aconnect/complex_types.hpp"

#include "http_support.hpp"

namespace ahttp
{
	class HttpResponseHeader;
	class HttpResponseStream;
	class HttpResponse;

	class HttpResponseStream : private boost::noncopyable
	{
	public:
		HttpResponseStream  (size_t buffSize, size_t chunkSize) :
			maxBuffSize_ (buffSize),
			maxChunkSize_ (chunkSize),
			socket_(INVALID_SOCKET),
			chunked_ (false),
			sendContent_ (true)
		  {};

		  inline void clear ()  {
			  buffer_.clear();
			  chunked_ = false;
		  }

		  inline void destroy ()  {
			  clear();
			  socket_ = INVALID_SOCKET;
		  }

		  inline void init (aconnect::socket_type sock) {	
			  socket_ = sock;
		  };
		  inline bool willBeFlushed (size_t contentSize) {
			  return ( (buffer_.size() + contentSize) >= maxBuffSize_ );
		  }
		  inline size_t getBufferSize() {
			  return maxBuffSize_;
		  }
		  inline size_t getBufferContentSize() {
			  return buffer_.size();
		  }
		  inline aconnect::socket_type socket()	{	
			  return socket_; 
		  }

		  friend class HttpResponse;

	private:
		inline void setChunkedMode () {
			chunked_ = true;
		}
		inline bool isChunked () {
			return chunked_;
		}

		inline void setSendContent (bool sendContent) {
			sendContent_ = sendContent;
		}

		void write (aconnect::string_constref content);
		void write (aconnect::string_constptr buff, size_t dataSize);
		void flush () throw (aconnect::socket_error);
		void end () throw (aconnect::socket_error);
		void writeDirectly (aconnect::string_constref content) throw (aconnect::socket_error);
		

	protected:
		size_t maxBuffSize_;
		size_t maxChunkSize_;

		aconnect::string buffer_;
		aconnect::socket_type socket_;
		bool chunked_;
		bool sendContent_;
	};

	class HttpResponse : private boost::noncopyable
	{

	public:
		HttpResponse (size_t buffSize, size_t chunkSize) :
			Header(),
			Stream (buffSize, chunkSize),
			clientInfo_ (NULL),
			headersSent_ (false), 
			finished_ (false),
			httpMethod_ (ahttp::HttpMethod::Unknown)

		{
			
		};

		inline void clear()  
		{
			Header.clear();
			Stream.destroy();
			clientInfo_ = NULL;
			finished_ = headersSent_ = false;
			serverName_.clear();
		}

		inline void init (const aconnect::ClientInfo* clientInfo) 
		{
			assert (clientInfo);
			clientInfo_ = clientInfo;
			Stream.init (clientInfo->socket);
		}

		void write (aconnect::string_constref content);
		void write (aconnect::string_constptr buff, size_t dataSize);
		void flush () throw (aconnect::socket_error);
		void writeCompleteResponse (aconnect::string_constref response) throw (std::runtime_error);
		void writeCompleteHtmlResponse (aconnect::string_constref response) throw (std::runtime_error);

		void end () throw (aconnect::socket_error);

		inline bool isFinished ()			{ return finished_;		};
		inline bool isHeadersSent ()		{ return headersSent_;	};
		inline bool canSendContent()		{ return httpMethod_ != HttpMethod::Head;	};
		inline void setServerName (aconnect::string_constref serverName) {
			serverName_ = serverName;
		}
		inline void setHttpMethod (ahttp::HttpMethod::HttpMethodType httpMethod) {
			httpMethod_ = httpMethod;
			Stream.setSendContent (canSendContent());
		}

		
		static aconnect::string getErrorResponse (int status, 
			aconnect::string_constptr messageFormat = NULL, ...);

	protected:
		void fillCommonResponseHeaders ();
		void sentHeaders () throw (std::runtime_error);
		void applyContentEncoding ();

	// properties
	public:
		HttpResponseHeader						Header;
		HttpResponseStream						Stream;

	protected:	
		const aconnect::ClientInfo*	clientInfo_;
		bool headersSent_;
		bool finished_;	
		aconnect::string serverName_;
		ahttp::HttpMethod::HttpMethodType httpMethod_;
	};

}

#endif // AHTTP_RESPONSE_H

