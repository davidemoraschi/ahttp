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

#ifndef AHTTP_HTTP_SUPPORT_H
#define AHTTP_HTTP_SUPPORT_H

#include <vector>
#include <boost/cstdint.hpp>

#include "aconnect/types.hpp"
#include "aconnect/logger.hpp"

namespace ahttp 
{ 
	namespace HttpMethod
	{
		enum HttpMethodType
		{
			Unknown = 0,
			Get = 1,
			Post = 2,
			Head = 3,
		};
	};

	enum WebDirectoryItemType // defines sorting order
	{
		WdUnknown,
		WdVirtualDirectory,
		WdDirectory,
		WdFile,
	};

	enum WebDirectorySortType
	{
		WdSortByName,
		WdSortByTypeAndName,
	};


	struct WebDirectoryItem
	{
		WebDirectoryItemType	type;
		aconnect::string		name;
		aconnect::string		url;
		boost::uintmax_t		size;
		std::time_t				lastWriteTime;

		WebDirectoryItem () : type (WdUnknown), size(-1), lastWriteTime(-1) { }
	};


	namespace detail 
	{
		using namespace aconnect;

		const char_type WeekDays_RFC1123[7][4] = {
			"Sun", "Mon", "Tue", "Wed", "Thu" , "Fri" , "Sat"
		};
		const char_type Months_RFC1123[12][4] = {
			"Jan" , "Feb" , "Mar" , "Apr", 
			"May" , "Jun" , "Jul" , "Aug", 
			"Sep" , "Oct" , "Nov" , "Dec"
		};

		// HTTP Methods
		string_constant MethodGet = "GET";
		string_constant MethodPost = "POST";
		string_constant MethodHead = "HEAD";

		// HTTP Headers
		string_constant HeaderAccept = "Accept";
		string_constant HeaderAcceptCharset = "Accept-Charset";
		string_constant HeaderAcceptEncoding = "Accept-Encoding";
		string_constant HeaderAcceptLanguage = "Accept-Language";
		string_constant HeaderAcceptRanges = "Accept-Ranges";
		string_constant HeaderAge = "Age";
		string_constant HeaderAllow = "Allow";
		string_constant HeaderAuthorization = "Authorization";
		string_constant HeaderCacheControl = "Cache-Control";
		string_constant HeaderConnection = "Connection";
		string_constant HeaderContentEncoding = "Content-Encoding";
		string_constant HeaderContentDisposition = "Content-Disposition";
		string_constant HeaderContentLanguage = "Content-Language";
		string_constant HeaderContentLength = "Content-Length";
		string_constant HeaderContentLocation = "Content-Location";
		string_constant ContentMD5 = "Content-MD5";
		string_constant HeaderContentRange = "Content-Range";
		string_constant HeaderContentType = "Content-Type";
		string_constant HeaderCookie = "Cookie";
		string_constant HeaderDate = "Date";
		string_constant HeaderETag = "ETag";
		string_constant HeaderExpect = "Expect";
		string_constant HeaderExpires = "Expires";
		string_constant HeaderFrom = "From";
		string_constant HeaderHost = "Host";
		string_constant HeaderIfMatch = "If-Match";
		string_constant HeaderIfModifiedSince = "If-Modified-Since";
		string_constant HeaderIfNoneMatch = "If-None-Match";
		string_constant HeaderIfRange = "If-Range";
		string_constant HeaderIfUnmodifiedSince = "If-Unmodified-Since";
		string_constant HeaderLastModified = "Last-Modified";
		string_constant HeaderLocation = "Location";
		string_constant HeaderMaxForwards = "Max-Forwards";
		string_constant HeaderPragma = "Pragma";
		string_constant HeaderProxyAuthenticate = "Proxy-Authenticate";
		string_constant HeaderProxyAuthorization = "Proxy-Authorization";
		string_constant HeaderProxyConnection = "Proxy-Connection";
		string_constant HeaderRange = "Range";
		string_constant HeaderReferer = "Referer";
		string_constant HeaderRetryAfter = "Retry-After";
		string_constant HeaderServer = "Server";
		string_constant HeaderTE = "TE";
		string_constant HeaderTrailer = "Trailer";
		string_constant HeaderTransferEncoding = "Transfer-Encoding";
		string_constant HeaderUpgrade = "Upgrade";
		string_constant HeaderUserAgent = "User-Agent";
		string_constant HeaderVary = "Vary";
		string_constant HeaderVia = "Via";
		string_constant HeaderWarning = "Warning";
		string_constant HeaderWWWAuthenticate = "WWW-Authenticate";

		// HTTP headers values
		string_constant ConnectionKeepAlive = "Keep-Alive";
		string_constant ConnectionClose = "Close";
		
		string_constant ContentTypeTextHtml = "text/html";
		string_constant ContentTypeOctetStream = "application/octet-stream";
		string_constant ContentTypeMultipartFormData = "multipart/form-data";

		string_constant ContentDispositionFormData = "form-data";
		string_constant ContentDispositionAttachment = "attachment";

		string_constant TransferEncodingChunked = "chunked";

		string_constant CacheControlNoCache = "no-cache";
		string_constant CacheControlPrivate = "private";
		
		//////////////////////////////////////////////////////////////////////////
		//
		//		Common server definitions  
		string_constant Slash = "/";
		const char_type SlashCh = '/';

		string_constant ChunkHeaderFormat = "%x\r\n";
		string_constant ChunkEndMark = "\r\n";
		string_constant LastChunkFormat = "0\r\n\r\n";
			
		string_constant HttpVersion = "HTTP/1.1";
		string_constant HeadersDelimiter = "\r\n";
		string_constant HeadersEndMark = "\r\n\r\n";
		string_constant HeaderValueDelimiter = ": ";
		
		string_constant DefaultContentCharset = "ISO-8859-1"; // q=1 by default: HTTP RFC
		string_constant ContentCharsetUtf8 = "UTF-8";
		string_constant AnyContentCharsetMark = "*";
		string_constant MultipartBoundaryMark = "boundary=";
		string_constant MultipartBoundaryPrefix = "--";

		//
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		//
		//		Support functions

		void readDirectoryContent (string_constref dirPath,
			string_constref dirVirtualPath,
			std::vector<WebDirectoryItem> &items,
			class aconnect::Logger& logger,
			size_t &errCount,
			WebDirectorySortType sortType = WdSortByName);

		// sample: Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
		string formatDate_RFC1123 (const struct tm& dateTime);

		inline string httpStatusDesc (int status) 
		{
			string desc;

			switch(status) {

			case 100 : desc = ("Continue"); break;
			case 101 : desc = ("Switching Protocols"); break;

			case 200 : desc = ("OK"); break;
			case 201 : desc = ("Created"); break;
			case 202 : desc = ("Accepted"); break;
			case 203 : desc = ("Non-Authoritative Information"); break;
			case 204 : desc = ("No Content"); break;
			case 205 : desc = ("Reset Content"); break;
			case 206 : desc = ("Partial Content"); break;

			case 300 : desc = ("Multiple Choices"); break;
			case 301 : desc = ("Moved Permanently"); break;
			case 302 : desc = ("Found"); break;
			case 303 : desc = ("See Other"); break;
			case 304 : desc = ("Not Modified"); break;
			case 305 : desc = ("Use Proxy"); break;
			case 306 : desc = ("(Unused)"); break;
			case 307 : desc = ("Temporary Redirect"); break;

			case 400 : desc = ("Bad Request"); break;
			case 401 : desc = ("Unauthorized"); break;
			case 402 : desc = ("Payment Required"); break;
			case 403 : desc = ("Forbidden"); break;
			case 404 : desc = ("Not Found"); break;
			case 405 : desc = ("Method Not Allowed"); break;
			case 406 : desc = ("Not Acceptable"); break;
			case 407 : desc = ("Proxy Authentication Required"); break;
			case 408 : desc = ("Request Timeout"); break;
			case 409 : desc = ("Conflict"); break;
			case 410 : desc = ("Gone"); break;
			case 411 : desc = ("Length Required"); break;
			case 412 : desc = ("Precondition Failed"); break;
			case 413 : desc = ("Request Entity Too Large"); break;
			case 414 : desc = ("Request-URI Too Long"); break;
			case 415 : desc = ("Unsupported Media Type"); break;
			case 416 : desc = ("Requested Range Not Satisfiable"); break;
			case 417 : desc = ("Expectation Failed"); break;

			case 500 : desc = ("Internal Server Error"); break;
			case 501 : desc = ("Not Implemented"); break;
			case 502 : desc = ("Bad Gateway"); break;
			case 503 : desc = ("Service Unavailable"); break;
			case 504 : desc = ("Gateway Timeout"); break;
			case 505 : desc = ("HTTP Version Not Supported"); break;

			default:
				desc = ("Undefined");
			}
			return desc;
		};

	}
} 

#endif // AHTTP_HTTP_SUPPORT_H

