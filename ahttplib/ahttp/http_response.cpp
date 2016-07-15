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

#include "aconnect/boost_format_safe.hpp"

#include <assert.h>
#include <boost/lexical_cast.hpp>

#include "aconnect/aconnect.hpp"
#include "aconnect/util.hpp"
#include "aconnect/time_util.hpp"

#include "ahttp/http_support.hpp"
#include "ahttp/http_messages.hpp"
#include "ahttp/http_server_settings.hpp"
#include "ahttp/http_response_header.hpp"
#include "ahttp/http_response.hpp"

namespace ahttp
{
	//////////////////////////////////////////////////////////////////////////
	//
	//		HttpResponse
	//

	void HttpResponse::fillCommonResponseHeaders () 
	{
		if( !serverName_.empty() )
			Header.Headers [detail::HeaderServer] = serverName_;
		Header.Headers [detail::HeaderDate] = detail::formatDate_RFC1123 (aconnect::util::getDateTimeUtc ());
	}

	void HttpResponse::sentHeaders () throw (std::runtime_error)
	{
		assert ( !headersSent_ && "Headers already sent" );
		assert ( httpMethod_ != ahttp::HttpMethod::Unknown && "HTTP method is not loaded");
		if (headersSent_)
			throw std::runtime_error ("HTTP headers already sent");

		applyContentEncoding();
		fillCommonResponseHeaders();
		
		aconnect::util::writeToSocket (clientInfo_->socket, 
			Header.getContent());

		headersSent_ = true;
	}


	void HttpResponse::applyContentEncoding () 
	{
		if ( !Header.hasHeader (detail::HeaderContentLength) ) 
		{
			Stream.setChunkedMode ();
			Header.Headers[detail::HeaderTransferEncoding] = detail::TransferEncodingChunked;
		}
	}

	void HttpResponse::writeCompleteHtmlResponse (aconnect::string_constref response) throw (std::runtime_error) 
	{
		Header.setContentType (detail::ContentTypeTextHtml);
		writeCompleteResponse (response);
	}

	void HttpResponse::writeCompleteResponse (aconnect::string_constref response) throw (std::runtime_error)
	{
		assert ( !finished_ && "Response already sent" );
		assert ( !headersSent_ && "Headers already sent" );

		if (headersSent_)
			throw std::runtime_error ("HTTP headers already sent");
		if (finished_)
			throw std::runtime_error ("Response already sent");

		
		Header.setContentLength ( response.size ());
		sentHeaders();
	
		Stream.clear();
		Stream.writeDirectly (response);
		
		finished_ = true;
	}


	void HttpResponse::write (aconnect::string_constptr buff, size_t dataSize) 
	{
		if (finished_)
			throw std::runtime_error ("Response already sent");

		if (!headersSent_ && Stream.willBeFlushed ( dataSize ))
			sentHeaders();

		Stream.write (buff, dataSize);
	}

	void HttpResponse::write (aconnect::string_constref content) 
	{
		write (content.c_str(), content.size() );
	}

	void HttpResponse::flush () throw (aconnect::socket_error) 
	{
		if (!headersSent_)
			sentHeaders();
		
		Stream.flush();
	}

	void HttpResponse::end () throw (aconnect::socket_error) 
	{
		// setup correct content length
		if (!headersSent_) 
			Header.setContentLength ( Stream.getBufferContentSize() );
		
		flush();
		Stream.end();

		// INVESTIGATE: Keep-alive fix for Firefox
		Stream.writeDirectly ("");

		finished_ = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// statics
	aconnect::string HttpResponse::getErrorResponse (int status, 
		aconnect::string_constptr messageFormat, ...)
	{
		using namespace boost;
		aconnect::str_stream ret;
		aconnect::string statusDesc = detail::httpStatusDesc (status);
		aconnect::string description = messages::ErrorUndefined;

		if (messageFormat) {
			FORMAT_VA_MESSAGE (messageFormat, message);
			description.swap (message);
		}
		
		ret << str(format(messages::MessageFormat) % statusDesc % statusDesc % description);
		return ret.str();
	}

	//
	//
	//////////////////////////////////////////////////////////////////////////


	//////////////////////////////////////////////////////////////////////////
	//
	//		HttpResponseStream
	//
	void HttpResponseStream::write (aconnect::string_constref content) 
	{	
		buffer_.append (content);
		if (buffer_.size() >= maxBuffSize_)
			flush ();
	};

	void HttpResponseStream::write (aconnect::string_constptr buff, size_t dataSize)
	{	
		buffer_.append (buff, dataSize);
		if (buffer_.size() >= maxBuffSize_)
			flush ();
	};

	void HttpResponseStream::writeDirectly (aconnect::string_constref content) throw (aconnect::socket_error)
	{
		assert (!chunked_ && "writeDirectly must not be called in 'chunked' mode");
		if (sendContent_)
			aconnect::util::writeToSocket (socket_, content);
	}

	void HttpResponseStream::flush () throw (aconnect::socket_error)
	{	
		using boost::format;
		using namespace aconnect;

		if (buffer_.empty())
			return;
		
		if (!sendContent_)
			return;

		if (chunked_) {
			const size_t bufferLen = buffer_.size();
			size_t curPos = 0, chunkSize = bufferLen;
			format chunkFormat (detail::ChunkHeaderFormat);

			if (chunkSize > maxChunkSize_)
				chunkSize = maxChunkSize_;

			do 
			{
				// write chunk size
				chunkFormat % chunkSize;
				util::writeToSocket (socket_, chunkFormat.str());
				chunkFormat.clear();

				// write data
				util::writeToSocket (socket_, buffer_.c_str() + curPos, 
					(int) chunkSize);
				
				// write chunk end mark
				util::writeToSocket (socket_, detail::ChunkEndMark);

				curPos += chunkSize;
				chunkSize = util::min2 (maxChunkSize_, bufferLen- curPos);

			} while (curPos < bufferLen);
			
		} else {
			util::writeToSocket (socket_, buffer_);
		}

		buffer_.clear();
	};

	void HttpResponseStream::end () throw (aconnect::socket_error)
	{	
		if (chunked_ && sendContent_) {
			// write last chunk
			aconnect::util::writeToSocket (socket_, aconnect::string (detail::LastChunkFormat) );
		}
	};
}
//
//
//////////////////////////////////////////////////////////////////////////

