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

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>
#include <boost/regex.hpp>

#include <assert.h>

#include "aconnect/util.hpp"
#include "aconnect/time_util.hpp"
#include "aconnect/complex_types.hpp"

#include "ahttp/http_messages.hpp"
#include "ahttplib.hpp"


namespace fs = boost::filesystem;
namespace algo = boost::algorithm;

namespace ahttp 
{
	HttpServerSettings* HttpServer::globalSettings_ = NULL;
	boost::detail::atomic_count HttpServer::RequestsCount (0);
	

#include "http_header_read_check.inl"

	//////////////////////////////////////////////////////////////////////////
	//		UploadFileInfo class
	//////////////////////////////////////////////////////////////////////////
	void UploadFileInfo::loadHeader (aconnect::string_constref header)
	{
		using namespace aconnect;

		reset();

		str_vector lines;
		string name, value;
		algo::split (lines, header, algo::is_any_of("\r\n"), algo::token_compress_on);

		size_t pos;
		str2str_map pairs;

		for (str_vector::iterator it = lines.begin(); it != lines.end(); ++it) 
		{
			if (it->empty())
				continue;

			if ((pos = it->find (':')) == string::npos) 
				throw request_processing_error ("Incorrect part header: %s", it->c_str());

			name = it->substr (0, pos);
			value = algo::trim_copy(it->substr (pos + 1));

			if (util::equals (name, detail::HeaderContentDisposition)) 
			{
				if ( !algo::starts_with(value, detail::ContentDispositionFormData) )
					throw request_processing_error ("Incorrect Content-Disposition type: %s", it->c_str());

				util::parseKeyValuePairs (value, pairs);

				if (pairs.find("name") == pairs.end() )
					throw request_processing_error ("'name' attribute is absent in Content-Disposition header: %s", it->c_str());

				this->name = pairs["name"];

				str2str_map::iterator fnIter = pairs.find ("filename");
				if ( fnIter != pairs.end()) {
					this->isFileData = true;
					this->fileName = fnIter->second;
				}


			} else if (util::equals (name, detail::HeaderContentType)) {
				this->contentType = value;
			}
		}
	}

	void UploadFileInfo::reset ()
	{
		isFileData = false;
		fileSize = 0;
		name.clear();
		fileName.clear();
		contentType.clear();
		uploadPath.clear();
	}

//////////////////////////////////////////////////////////////////////////
//		HttpContext class
//////////////////////////////////////////////////////////////////////////

	HttpContext::HttpContext (const aconnect::ClientInfo* clientInfo, 
			HttpServerSettings* globalSettings,
			aconnect::Logger *log) :
		Client (clientInfo),
		RequestHeader (),
		RequestStream (),
		Response (globalSettings ? globalSettings->responseBufferSize() : ahttp::defaults::ResponseBufferSize, 
			globalSettings ? globalSettings->maxChunkSize() : ahttp::defaults::MaxChunkSize),
		Method (HttpMethod::Unknown),
		GlobalSettings (globalSettings),
		Log (log)
	{
		assert (clientInfo);
		assert (globalSettings);
		assert (log);
	}

	HttpContext::~HttpContext()
	{
		std::map <aconnect::string, UploadFileInfo>::const_iterator iter;
		for (iter = UploadedFiles.begin(); iter != UploadedFiles.end(); ++iter)
		{
			try	{
				fs::remove(iter->second.uploadPath);
			} catch (std::exception &ex)  {
				Log->error ("Upload deletion failed - exception [%s]: %s, file: %s", 
					typeid(ex).name(), ex.what(), iter->second.uploadPath.c_str());

			} catch (...)  {
				Log->error ("Upload deletion failed - unknown exception caught, file: %s", 
					iter->second.uploadPath.c_str() );
			}
		}
		
	}

	bool HttpContext::init (bool isKeepAliveConnect,
							long keepAliveTimeoutSec) {
		
		HttpHeaderReadCheck check (&RequestHeader, Client->server, 
			isKeepAliveConnect, keepAliveTimeoutSec);

		aconnect::string requestBodyBegin = aconnect::util::readFromSocket (Client->socket, check, false);
		if (check.connectionWasClosed() || requestBodyBegin.empty())
			return false;

		boost::algorithm::erase_head ( requestBodyBegin, (int) check.headerSize());
		RequestStream.init (requestBodyBegin, (int) RequestHeader.ContentLength, Client->socket);

		Response.init (Client);
		Response.setServerName (GlobalSettings->serverVersion());
		
		return true;
	}

	void HttpContext::reset () 
	{
		RequestHeader.clear();
		RequestStream.clear();
		Response.clear();
		
		GetParameters.clear();
		PostParameters.clear();
		Cookies.clear();
		
		UploadedFiles.clear();
		
		Method = HttpMethod::Unknown;
	}
	
	void HttpContext::setHtmlResponse() {
		if ( Response.Header.Status == HttpResponseHeader::UnknownStatus )
			Response.Header.Status = 200;
		if ( !Response.Header.hasHeader (detail::HeaderContentType) )
			Response.Header.setContentType (detail::ContentTypeTextHtml);
	}
	
	bool HttpContext::isClientConnected() {
		if (RequestStream.hasBufferedContent())
			return true;

		if (!RequestStream.isRead())
			return aconnect::util::checkSocketState (RequestStream.socket(),
				GlobalSettings->serverSettings().socketReadTimeout);
		
		return aconnect::util::checkSocketState (Response.Stream.socket(),
			GlobalSettings->serverSettings().socketWriteTimeout,
			true);
	}

	void HttpContext::parseQueryStringParams ()
	{
		using namespace aconnect;
		string::size_type pos = RequestHeader.Path.find("?");
		if (string::npos == pos || pos == RequestHeader.Path.length()-1)
			return;

		str_vector pairs;
		string getParams = RequestHeader.Path.substr(pos + 1);

		algo::split (pairs, getParams, boost::algorithm::is_any_of("&"), algo::token_compress_on);

		for (str_vector::iterator it = pairs.begin() ; it != pairs.end(); ++it) {
			pos = it->find ('=');
			if (pos == string::npos) {
				GetParameters[util::decodeUrl(*it)] = "";
			} else {
				GetParameters[util::decodeUrl(it->substr (0, pos))] = util::decodeUrl(it->substr (pos + 1));
			}
		}
	}

	void HttpContext::parseCookies () {
		// HTTP header: "Cookie: PART_NUMBER=RIDING_ROCKET_0023; PART_NUMBER=ROCKET_LAUNCHER_0001"
		using namespace aconnect;
		if ( !RequestHeader.hasHeader (detail::HeaderCookie) )
			return;

		string cookiesString = RequestHeader.getHeader(detail::HeaderCookie);
		
		str_vector pairs;
		algo::split (pairs, cookiesString, boost::algorithm::is_any_of(";"), algo::token_compress_on);

		for (str_vector::iterator it = pairs.begin() ; it != pairs.end(); ++it) {
			string::size_type pos = it->find ('=');
			if (pos == string::npos) {
				Cookies [util::decodeUrl(*it)] = "";
			} else {
				Cookies [util::decodeUrl(it->substr (0, pos))] = util::decodeUrl(it->substr (pos + 1));
			}
		}
	}

	void HttpContext::parsePostParams() 
	{
		using namespace aconnect;

		string contentType = RequestHeader.getHeader(detail::HeaderContentType);
		
		if (algo::istarts_with (contentType, detail::ContentTypeMultipartFormData) ) {
			
			string boundary = contentType.substr (contentType.find (detail::MultipartBoundaryMark) +
					ARRAY_SIZE(detail::MultipartBoundaryMark) - 1);
			
			return loadMultipartFormData (boundary);
		}
		
		if (RequestHeader.ContentLength == 0)
			return;

		const int buffSize = (int) util::min2 (Response.Stream.getBufferSize(), RequestHeader.ContentLength);
		boost::scoped_array<char_type> buff (new char_type [buffSize]);
		
		int readBytes = 0;
		str_stream record;
		string key, val;
		char_type ch;
		bool keyLoaded = false;
		
		do 
		{
			readBytes = RequestStream.read(buff.get(), buffSize);
			
			if (readBytes > 0) {
				record.write (buff.get(), readBytes);
				
				while (record.get(ch)) {
					if ( ch == '&' && !key.empty() ) {
						PostParameters [util::decodeUrl(key)] = util::decodeUrl (val);
						key.clear();
						val.clear();
						keyLoaded = false;
					
					} else if (ch == '=') {
						keyLoaded = true;
					
					} else if (keyLoaded) {
						val.append(1, ch);
					} else {
						key.append(1, ch);
					}
				}
			
			} else if (!key.empty()) {
				PostParameters [util::decodeUrl(key)] = util::decodeUrl (val);
			}
		
		} while (readBytes > 0);
		
		assert (record.eof() && "Request stream is not empty" );

	}


	
	void HttpContext::loadMultipartFormData (aconnect::string_constref boundary) 
	{
		using namespace aconnect;
		const int buffSize = (int) util::min2 (Response.Stream.getBufferSize(), RequestHeader.ContentLength);

		boost::scoped_array<char_type> buff (new char_type [buffSize]);

		int readBytes = 0;
		string record, fieldName;
		size_t boundaryPos, endPos;
		fs::path uploadPath;
		
		const string boundaryBegin = detail::MultipartBoundaryPrefix + boundary;
		const string boundaryBeginWithEndMark = string(detail::HeadersDelimiter) + detail::MultipartBoundaryPrefix + boundary;
		const string boundaryEnd = detail::MultipartBoundaryPrefix + boundary + detail::MultipartBoundaryPrefix;

		const size_t boundOffset = (boundaryBegin + detail::HeadersDelimiter).size();
		const size_t endMarkLen = strlen (detail::HeadersEndMark);
		const size_t headerEndMarkLen = strlen (detail::HeadersDelimiter);

		UploadFileInfo uploadInfo;
		std::ofstream currentFile;
		string fileNamePrefix;
				
		do 
		{
			readBytes = RequestStream.read (buff.get(), buffSize);
			record.append (buff.get(), readBytes);

			boundaryPos = record.find (boundaryBegin);
			
			while (record.length()) 
			{
				// start reading
				if (boundaryPos == 0 
					&& (endPos = record.find (detail::HeadersEndMark, 0)) != string::npos) 
				{
					if (currentFile.is_open()) {
						currentFile.flush();
						currentFile.rdbuf()->close();
					}

					uploadInfo.loadHeader (record.substr(boundOffset, endPos - boundOffset));
					fieldName = util::decodeUrl (uploadInfo.name);

					record.erase(0, endPos + endMarkLen);
					boundaryPos = util::findSequence (record, boundaryBeginWithEndMark);
					
					if ( !uploadInfo.isFileData ) {
						PostParameters [fieldName] += record.substr (0, boundaryPos);
			
					} else {
						
						if ( !uploadInfo.fileName.empty() ) 
						{
							fileNamePrefix.clear();
							// UNDONE: replace += '$' to "timestamp + rnd()"
							// fast upload name search
							do {
								uploadPath = UploadsDirPath / (fileNamePrefix + uploadInfo.fileName);
								fileNamePrefix += '$';
							} while (fs::exists (uploadPath));

							// open file
							do {
								uploadPath = UploadsDirPath / (fileNamePrefix + uploadInfo.fileName);
								currentFile.open ( uploadPath.file_string().c_str(), std::ios::out | std::ios::binary );
								fileNamePrefix += '$';

							} while (currentFile.bad());


							uploadInfo.uploadPath = uploadPath.file_string();
							
							if (record.size()) {
								currentFile << record.substr (0, boundaryPos);
							}
						}

						UploadedFiles[fieldName] = uploadInfo;
					}

					record.erase(0, boundaryPos);
					boundaryPos = 0;
				
				} else if (!fieldName.empty() && boundaryPos != 0 && boundaryPos != string::npos) {
					
					if ( !uploadInfo.isFileData ) {
						PostParameters [fieldName] += record.substr (0, boundaryPos - headerEndMarkLen);
					} else {
						currentFile << record.substr (0, boundaryPos - headerEndMarkLen);
					}
					
					record.erase(0, boundaryPos);
					boundaryPos = 0;

				} else { 
					
					if ( uploadInfo.isFileData 
						&& currentFile.is_open()
						&&  util::findSequence (record, boundaryBeginWithEndMark) == string::npos
						&&  util::findSequence (record, boundaryBegin) == string::npos) 
					{
						currentFile << record;
						record.clear();
									
					} else {
						break;
					}
				}
				
				if (record.find (boundaryEnd) == 0) {
					// eat request
					while (!RequestStream.isRead() && readBytes > 0)
						readBytes = RequestStream.read (buff.get(), buffSize);
					
					readBytes = 0;
					break;
				}
			}
			
		} while (readBytes > 0);

		if (currentFile.is_open()) {
			currentFile.flush();
			currentFile.rdbuf()->close();
		}

		// read files info
		std::map <aconnect::string, UploadFileInfo>::iterator iter;
		for (iter = UploadedFiles.begin(); iter != UploadedFiles.end(); ++iter)
		{
			if (iter->second.uploadPath.empty())
				continue;

			try	{
				iter->second.fileSize = (size_t) 
					fs::file_size (iter->second.uploadPath);
			
			} catch (std::exception &ex)  {
				Log->error ("Uploaded file properties loading failed - exception [%s]: %s, file: %s", 
					typeid(ex).name(), ex.what(), iter->second.uploadPath.c_str());
			} catch (...)  {
				Log->error ("Uploaded file properties loading failed - unknown exception caught, file: %s", 
					iter->second.uploadPath.c_str() );
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//
	// Process worker thread creation fail
	void HttpServer::processWorkerCreationError (const aconnect::socket_type clientSock) 
	{
		using namespace aconnect;
		
		string content = HttpResponse::getErrorResponse(503,
			messages::Error503);

		str_stream response;

		// create header
		response << HttpResponseHeader::getResponseStatusString (503);
		response << detail::HeaderContentType << detail::HeaderValueDelimiter << detail::ContentTypeTextHtml << detail::HeadersDelimiter;
		response << detail::HeaderContentLength << detail::HeaderValueDelimiter << content.length() << detail::HeadersDelimiter;
		response << detail::HeaderServer << detail::HeaderValueDelimiter << GlobalSettings()->serverVersion() << detail::HeadersDelimiter;

		aconnect::util::writeToSocket (clientSock, response.str());
	}

	// check HTTP method availability - sent 501 on fail
	bool HttpServer::isMethodImplemented (HttpContext& context)
	{
		using namespace aconnect;
		string_constref method = context.RequestHeader.Method;

		if (method.empty()) {
			Log()->warn("Empty HTTP method retrieved in request");
			return false;
		}

		if (util::equals(method, detail::MethodGet)) {
			context.Method = HttpMethod::Get;
			return true;
		}
		if (util::equals(method, detail::MethodPost)) {
			context.Method = HttpMethod::Post;
			return true;
		}
		if (util::equals(method, detail::MethodHead)) {
			context.Method = HttpMethod::Head;
			return true;
		}

		// format "Not Implemented" response
		context.Response.Header.Status = 501;
		aconnect::string errorResponse = HttpResponse::getErrorResponse (context.Response.Header.Status,
			messages::Error501_MethodNotImplemented, method.c_str());

		context.Response.Header.setContentType (detail::ContentTypeTextHtml);
		context.Response.writeCompleteResponse (errorResponse);
		
		return false;
	}

	void HttpServer::redirectRequest (HttpContext& context,
								aconnect::string_constref virtualPath, 
								int status)
	{
		using namespace aconnect;

		context.Response.Header.Status = status;
		context.Response.Header.Headers [detail::HeaderLocation] = virtualPath;

		aconnect::string errorResponse = HttpResponse::getErrorResponse (context.Response.Header.Status,
			messages::ErrorDocumentMoved, virtualPath.c_str() );
		
		context.Response.writeCompleteHtmlResponse (errorResponse);
	}

	// format and send 403 error
	void HttpServer::processError403 (HttpContext& context,
								 aconnect::string_constptr message)
	{
		using namespace aconnect;
		
		context.Response.Header.Status = 403;
		aconnect::string errorResponse = HttpResponse::getErrorResponse (context.Response.Header.Status,
			message);
		context.Response.writeCompleteHtmlResponse (errorResponse);
	}

	// format and send 404 error
	void HttpServer::processError404 (HttpContext& context)
	{
		using namespace aconnect;

		// format "Not Found" response
		context.Response.Header.Status = 404;
		aconnect::string errorResponse = HttpResponse::getErrorResponse (context.Response.Header.Status,
			messages::Error404, context.VirtualPath.c_str());
		
		context.Response.writeCompleteHtmlResponse (errorResponse);
	}

	void HttpServer::processError405 (HttpContext& context,
									  aconnect::string_constref allowedMethods) 
	{

		using namespace aconnect;

		// format "Method Not Allowed" response
		context.Response.Header.Status = 405;
		aconnect::string errorResponse = HttpResponse::getErrorResponse (context.Response.Header.Status,
			messages::Error405, context.RequestHeader.Method.c_str(), allowedMethods.c_str());

		context.Response.Header.Headers[detail::HeaderAllow] = allowedMethods;
		context.Response.Header.Headers[detail::HeaderConnection] = detail::ConnectionClose;
		context.Response.writeCompleteHtmlResponse (errorResponse);
	}

	void HttpServer::processError406 (HttpContext& context, 
		aconnect::string_constref message) 
	{
		// format "Not Acceptable" response
		context.Response.Header.Status = 406;
		aconnect::string errorResponse = HttpResponse::getErrorResponse (context.Response.Header.Status,
			message.c_str());
		context.Response.writeCompleteHtmlResponse (errorResponse);
	}

	// send server error response (500+)
	void HttpServer::processServerError (HttpContext& context,
								 int status, aconnect::string_constptr message)
	{
		using namespace aconnect;

		if (!context.Response.isHeadersSent() &&
			!context.Response.isFinished()) 
		{
			aconnect::string errorResponse = HttpResponse::getErrorResponse (status,
				messages::Error500, (message ? message : "") );

			context.Response.Header.Status = status;
			context.Response.writeCompleteHtmlResponse (errorResponse);
		
		} else {

			string response = 
				boost::str(boost::format (messages::MessageFormatInline) % 
					detail::httpStatusDesc (status) % (message ? message : ""));
			
			context.Response.write (response);
			context.Response.end();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//		HTTP request processing procedure
	//////////////////////////////////////////////////////////////////////////

	void HttpServer::processConnection (const aconnect::ClientInfo& client)
	{
		using namespace aconnect;
		string connectionHeader, requestString;
		try
		{
			bool isKeepAliveConnect = false;

			do {
				HttpContext context (&client, 
					HttpServer::GlobalSettings(),
					HttpServer::GlobalSettings()->logger());

				bool loaded = context.init (isKeepAliveConnect, 
					GlobalSettings()->keepAliveTimeout());

				if (!loaded)
					break;
				requestString = context.RequestHeader.Path;

				if (processRequest (context))
					break;
				
				if (!GlobalSettings()->isKeepAliveEnabled())
					break;

				if (context.RequestHeader.hasHeader(detail::HeaderProxyConnection))
					connectionHeader = context.RequestHeader.getHeader (detail::HeaderProxyConnection);
				else
					connectionHeader = context.RequestHeader.getHeader (detail::HeaderConnection);

				isKeepAliveConnect = true;
				
				
			// process subsequent "Keep-Alive" requests
			} while ( util::equals (connectionHeader, detail::ConnectionKeepAlive) );
			

		} catch (std::exception &ex)  {
			Log()->error ("Exception caught (%s): %s, client IP: %s, path: %s", 
				typeid(ex).name(), ex.what(), 
				util::formatIpAddr (client.ip).c_str(),
				requestString.empty() ? "<not loaded>" : requestString.c_str());

		} catch (...)  {
			Log()->error ("Unknown exception caught, client IP: %s", 
				util::formatIpAddr (client.ip).c_str() );
		}

	}

	bool HttpServer::processRequest (HttpContext &context)
	{
		using namespace aconnect;
		ProgressTimer progress (*Log(), __FUNCTION__);

		++RequestsCount;
		
		if (!isMethodImplemented (context))
			return true;

		if ( Log()->isDebugEnabled() )
			Log()->debug ("Request: %s %s", context.RequestHeader.Method.c_str(), context.RequestHeader.Path.c_str());

		context.Response.setHttpMethod (context.Method);
		context.MappedVirtualPath = 
			context.VirtualPath = context.RequestHeader.Path.substr(0, context.RequestHeader.Path.find("?"));
		
		try {
			
			// find request target by URL and process it is a real file
			if ( findTarget (context))
				processDirectFileRequest (context);

		// try to send 500 error
		} catch (request_processing_error &ex) {
			Log()->error (ex);
			processServerError(context, 500, ex.what());
		}
		
		// check request state - it must be read at this point
		if ( !context.RequestStream.isRead()) {
			processServerError(context, 500, messages::Error500_RequestNotLoaded);
			return true;
		}


		// check response correctness
		if (!context.Response.isFinished ()) {
			if (context.Response.Header.Status == HttpResponseHeader::UnknownStatus)
				processError404 (context);
			else
				context.Response.end();

			if ( Log()->isDebugEnabled() )
				Log()->debug ("Request end: %s, status: %d", context.VirtualPath.c_str(), 
					context.Response.Header.Status);
		}
	
		return false;
	}

	bool HttpServer::findTarget (HttpContext& context) 
	{
		using namespace aconnect;
		const directories_map &directories = GlobalSettings()->Directories();
		directories_map::const_iterator rootRecord = directories.find (detail::Slash);
 
		if (rootRecord == directories.end()) 
		{
			Log()->error("Root web directory (\"/\") is not registered");

			processError404 (context);
			return false;
		}

		DirectorySettings parentDirSettings = rootRecord->second;

		// find registered directory
		if ( !util::equals (context.VirtualPath, detail::Slash) ) 
		{
			string::size_type slashPos = 0;
			directories_map::const_iterator dirIter;
			string parentDir;
			
			while ((slashPos = context.VirtualPath.find(detail::Slash, slashPos + 1)) != string::npos) {
				parentDir = context.VirtualPath.substr(0, slashPos + 1);
				
				if ( (dirIter = directories.find (parentDir)) != directories.end())
					parentDirSettings = dirIter->second;
				else
					break;
			} 
		}

		// apply mappings
		if (!parentDirSettings.mappings.empty ()) {
			
			string virtualPath = context.VirtualPath.substr (parentDirSettings.virtualPath.length());
            boost::smatch matches;
			string val;
                
			for (mappings_vector::iterator iter = parentDirSettings.mappings.begin();
					iter != parentDirSettings.mappings.end();
					++iter) 
			{
				if (boost::regex_match (virtualPath, matches, boost::regex(iter->first) )) {
					
					string target = iter->second;
					str_stream pattern;
					
					for (int ndx = 1; ndx< (int) matches.size(); ++ndx) {
						pattern.str("");
						pattern << "{" << (ndx-1) << "}";

						if (matches[ndx].matched) 
							val = matches.str(ndx);
                        else
                            val.clear();
						
                        // UNDONE: skip "{{" and "}}"
                        // boost::regex re (pattern.str()); 
						// virtualPath = boost::regex_replace (virtualPath, re, matches.str(ndx));

						algo::replace_all (target, pattern.str(), val);
					}

					// update path and query string
					context.RequestHeader.Path = parentDirSettings.virtualPath + target;
					context.MappedVirtualPath = context.RequestHeader.Path.substr(0, context.RequestHeader.Path.find("?"));
				}
			}
		}

		if (context.MappedVirtualPath == parentDirSettings.virtualPath) {
			context.FileSystemPath = fs::path (parentDirSettings.realPath, fs::native);
		} else {
			context.FileSystemPath = fs::complete (
					fs::path (util::decodeUrl (context.MappedVirtualPath.substr (
						parentDirSettings.virtualPath.length())), fs::portable_name), 
					fs::path (parentDirSettings.realPath, fs::native)
				);
		}
		
		
		if ( runHandlers(context, parentDirSettings) )
			return false; // processed by handler

		
		if (fs::is_directory(context.FileSystemPath) ) 
		{
			if (context.VirtualPath == context.MappedVirtualPath) {
				if (algo::ends_with (context.VirtualPath, detail::Slash))
					processDirectoryRequest (context, parentDirSettings);
				else
					redirectRequest (context, context.VirtualPath + detail::Slash); // redirect
			
			} else {
				processDirectoryRequest (context, parentDirSettings);
			}

			return false;
		}

		// find virtual dir, if found - redirect
		directories_map::const_iterator virtDirIter = directories.begin();
		const string virtDirPath = context.VirtualPath + detail::Slash;
		while (virtDirIter != directories.end()) {
			if (virtDirIter->second.isLinkedDirectory 
				&& virtDirPath == virtDirIter->second.virtualPath
				) 
			{
				redirectRequest (context, virtDirPath); // redirect
				return false;
			}
			virtDirIter++;
		} 

		if ( !fs::exists( context.FileSystemPath ) ) {
			// 404 error
			processError404 (context);
			return false;
		}

		return true;
	}

	bool HttpServer::runHandlers (HttpContext& context, const struct DirectorySettings& dirSettings)
	{
		using namespace aconnect;

		const string extension = fs::extension(context.FileSystemPath);
		directory_handlers_map::const_iterator it;
		
		Log()->debug ("Run handler for \"%s\", directory settings: \"%s\"", 
							context.FileSystemPath.string().c_str(),
							dirSettings.name.c_str());		

		for (it = dirSettings.handlers.begin(); it != dirSettings.handlers.end(); ++it)
		{
			if (util::equals (it->first, extension) || 
				util::equals (it->first, SettingsTags::AllExtensionsMark))
			{
				if (reinterpret_cast<process_request_function> (it->second) (context))
					return true;
			}
		}

		return false;
	}

	void HttpServer::processDirectoryRequest ( HttpContext& context, 
											const DirectorySettings& dirSettings)
	{
		using namespace aconnect;
		
		ProgressTimer progress (*Log(), __FUNCTION__);
		
		// find and open default document 
		for (default_documents_vector::const_iterator it = dirSettings.defaultDocuments.begin();
			it != dirSettings.defaultDocuments.end(); it++) 
		{
			fs::path docPath = context.FileSystemPath / fs::path(it->second);
			if (fs::exists (docPath)) 
			{
				context.FileSystemPath = docPath;
				context.VirtualPath += it->second;
				Log()->debug ( "Redirection to \"%s\"", context.VirtualPath.c_str() );				

				// Content-Location: <path to document>
				context.Response.Header.Headers[detail::HeaderContentLocation] = context.VirtualPath;
				
				if ( runHandlers(context, dirSettings) )
					return;
				
				processDirectFileRequest (context);
				return;
			}
		}
		
		if ( !dirSettings.browsingEnabled ) {
			// 403 error
			return processError403 (context, messages::Error403_BrowseContent);
		}

		// HTTP method must be GET or HEAD, if not - sent 405, with "Allow: GET, HEAD"
		if (context.Method != HttpMethod::Get 
			&& context.Method != HttpMethod::Head) 
			return processError405 (context, "GET, HEAD");
		
		if ( !fs::exists( context.FileSystemPath ) ) {
			// 404 error
			return processError404 (context);
		}

		string record;
		
		if ( fs::is_directory( context.FileSystemPath ) )
		{
			// check "Accept-Charset" header
			if (context.RequestHeader.hasHeader(detail::HeaderAcceptCharset)) 
			{
				string acceptedCharsets = context.RequestHeader[detail::HeaderAcceptCharset];
				
				if ( !algo::contains (acceptedCharsets, detail::AnyContentCharsetMark) &&
					!algo::icontains (acceptedCharsets, dirSettings.charset) &&
					!algo::iequals	(detail::DefaultContentCharset, dirSettings.charset)) {

						Log()->error ("Charset \"%s\" is not allowed in \"%s\"", 
							dirSettings.charset.c_str(),
							acceptedCharsets.c_str());
						return processError406 (context, messages::Error406_CharsetNotAllowed);
				}

			}

			// start response
			context.Response.Header.Status = 200;
			context.Response.Header.setContentType (detail::ContentTypeTextHtml, dirSettings.charset);

			// format header
			context.Response.write (formatHeaderRecord (dirSettings, context.VirtualPath));

			if ( !util::equals (context.VirtualPath, detail::Slash)) {
				string parentDir = context.VirtualPath.substr (0, context.VirtualPath.rfind (detail::SlashCh, context.VirtualPath.size() - 2) + 1);
				context.Response.write (formatParentDirRecord (dirSettings, parentDir ));
			}
			
			std::vector<WebDirectoryItem> directoryItems;

			// write virtual directories
			const directories_map &directories = GlobalSettings()->Directories();
			directories_map::const_iterator virtDirIter = directories.begin();

			while (virtDirIter != directories.end()) {
				if (virtDirIter->second.isLinkedDirectory &&
						virtDirIter->second.parentName == dirSettings.name &&
						context.VirtualPath == dirSettings.virtualPath) 
				{
					WebDirectoryItem item;
					item.url = virtDirIter->second.virtualPath;
					item.name = virtDirIter->second.relativePath;
					item.type = WdVirtualDirectory;
					item.lastWriteTime = fs::last_write_time (virtDirIter->second.realPath);

					directoryItems.push_back (item);
				}

				virtDirIter++;
			} 

			size_t fileCount = 0, dirCount = 0, errCount = 0;

			// get filesystem items
			detail::readDirectoryContent (context.FileSystemPath.string(), 
					context.VirtualPath,
					directoryItems,
					*Log(),
					errCount/*, WdSortByTypeAndName*/);
			
			// write content
			
			for ( std::vector<WebDirectoryItem>::const_iterator itemIter = directoryItems.begin();
				itemIter != directoryItems.end();
				++itemIter )
			{
				
				if (itemIter->type == WdVirtualDirectory) {
					record = formatItemRecord (dirSettings.virtualDirectoryTemplate,
						itemIter->url,
						itemIter->name, 
						(size_t) itemIter->size,
						itemIter->lastWriteTime);
				
				} else if (itemIter->type == WdDirectory) {
					record = formatItemRecord (dirSettings.directoryTemplate,
						itemIter->url,
						itemIter->name, 
						(size_t)itemIter->size,
						itemIter->lastWriteTime);
					
					++dirCount;

				} else {
					record = formatItemRecord (dirSettings.fileTemplate,
							itemIter->url,
							itemIter->name,
							(size_t)itemIter->size,
							itemIter->lastWriteTime);
					++fileCount;
				}

				context.Response.write (record);
			}

			// format footer
			context.Response.write (formatFooterRecord (dirSettings, context.VirtualPath,
				fileCount, dirCount, errCount));
			context.Response.end();

		} else {
			Log()->error ("%s: file path retrieved instead of directory - \"%s\"", 
				__FUNCTION__, 
				context.FileSystemPath.string().c_str());
			
			processServerError(context, 500, messages::ServerError_FileInsteadDirectory);
		}
	}

	void HttpServer::processDirectFileRequest (HttpContext& context) 
	{
		using namespace aconnect;
		ProgressTimer progress (*Log(), __FUNCTION__);

		// HTTP method must be GET or HEAD, if not - sent 405, with "Allow: GET, HEAD"
		if (context.Method != HttpMethod::Get 
			&& context.Method != HttpMethod::Head) 
			return processError405 (context, "GET, HEAD");

		std::ifstream file (context.FileSystemPath.string().c_str(), std::ios::binary);
		if ( file.fail() ) {
			// Access denied (404 checked previously)
			processError403(context, messages::Error403_AccessDenied);
			return;
		}
		
		size_t fileSize = (size_t) fs::file_size (context.FileSystemPath);
		std::time_t modifyTime = fs::last_write_time ( context.FileSystemPath);
		string etag = util::calculateFileCrc (context.FileSystemPath.string(), modifyTime);

		if (context.RequestHeader.hasHeader (detail::HeaderIfNoneMatch) ) {
			if (etag == context.RequestHeader.Headers[detail::HeaderIfNoneMatch])
			{
				context.Response.Header.Status = 304;
				context.Response.Header.setContentLength ( 0 );
				context.Response.Header.Headers[detail::HeaderETag] = etag;
				return;
			}
		}
		
		Log()->debug ("Send file: %s", context.FileSystemPath.string().c_str());

		// prepare response
		context.Response.Header.Status = 200;
		context.Response.Header.setContentLength ( fileSize );
		context.Response.Header.setContentType ( context.GlobalSettings->getMimeType (
			fs::extension (context.FileSystemPath) ) );
		
		// add ETag, Last-Modified
		context.Response.Header.Headers[detail::HeaderETag] = etag;
		context.Response.Header.Headers[detail::HeaderLastModified] = detail::formatDate_RFC1123 (util::getDateTimeUtc (modifyTime));

		// send file
		const std::streamsize buffSize = (std::streamsize) util::min2( fileSize, context.Response.Stream.getBufferSize());
		boost::scoped_array<char_type> buff (new char_type [buffSize]);
		do {
			file.read (buff.get(), buffSize);
			context.Response.write (buff.get(), file.gcount());

		} while (file.good());
	}



	aconnect::string HttpServer::formatHeaderRecord (const DirectorySettings& dirSettings, 
													   aconnect::string_constref virtualPath) 
	{
		aconnect::string record = algo::replace_all_copy (dirSettings.headerTemplate,
		   SettingsTags::PageUrlMark, virtualPath);
		return record;
	}

	aconnect::string HttpServer::formatParentDirRecord (const DirectorySettings& dirSettings, 
													   aconnect::string_constref parentPath) 
	{
		aconnect::string record = algo::replace_all_copy (dirSettings.parentDirectoryTemplate,
			SettingsTags::ParentUrlMark, parentPath);
		return record;
	}

	aconnect::string HttpServer::formatItemRecord (aconnect::string_constref itemTemplate,
													 aconnect::string_constref itemUrl,
													 aconnect::string_constref itemName,
													 const size_t itemSize,
													 std::time_t lastWriteTime) 
	{
		 aconnect::string record = algo::replace_all_copy (itemTemplate,
			 SettingsTags::UrlMark, itemUrl);
		 algo::replace_all (record, SettingsTags::NameMark, itemName);

		if (itemSize != (size_t)-1)
			algo::replace_all (record, SettingsTags::SizeMark,
				 boost::lexical_cast<aconnect::string> (itemSize) );

		if (lastWriteTime != (std::time_t) -1)
			algo::replace_all (record, SettingsTags::TimeMark,
				formatDateTime ( aconnect::util::getDateTimeUtc(lastWriteTime) ) );

		 return record;
	}

	aconnect::string HttpServer::formatFooterRecord (const DirectorySettings& dirSettings, 
													   aconnect::string_constref virtualPath,
													   const size_t fileCount, const size_t dirCount, const size_t errCount) 
	{
		aconnect::string record = algo::replace_all_copy (dirSettings.footerTemplate,
			SettingsTags::PageUrlMark, virtualPath);

		algo::replace_all (record, SettingsTags::FilesCountMark, 
			boost::lexical_cast<aconnect::string> (fileCount) );
		algo::replace_all (record, SettingsTags::DirectoriesCountMark, 
			boost::lexical_cast<aconnect::string> (dirCount) );
		algo::replace_all (record, SettingsTags::ErrorsCountMark, 
			boost::lexical_cast<aconnect::string> (errCount) );

		return record;
	}
}


