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

#ifndef AHTTP_SERVER_H
#define AHTTP_SERVER_H
#pragma once
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <boost/detail/atomic_count.hpp>


#include "aconnect/types.hpp"
#include "aconnect/util.hpp"

#include "ahttp/http_support.hpp"
#include "ahttp/http_server_settings.hpp"
#include "ahttp/http_request.hpp"
#include "ahttp/http_response_header.hpp"
#include "ahttp/http_response.hpp"

namespace ahttp
{
	class HttpServer;

	struct UploadFileInfo
	{
		aconnect::string name;
		aconnect::string fileName;
		aconnect::string contentType;
		bool isFileData;
		
		size_t fileSize;
		aconnect::string uploadPath;

		UploadFileInfo () { 
			reset();
		} 

		void loadHeader (aconnect::string_constref header);
		void reset();
		
	};

	class HttpContext : private boost::noncopyable
	{
	public:	
		HttpContext (const aconnect::ClientInfo* clientInfo, 
			HttpServerSettings* globalSettings,
			aconnect::Logger *log);
		~HttpContext();

		bool init (bool isKeepAliveConnect, 
			long keepAliveTimeoutSec);

		bool isClientConnected();
		void reset ();
		void setHtmlResponse();

		void parseQueryStringParams ();
		void parsePostParams ();
		void parseCookies ();
		void loadMultipartFormData (aconnect::string_constref boundary);
		

		// properties
	public:
		const aconnect::ClientInfo*				Client;

		HttpRequestHeader						RequestHeader;
		HttpRequestStream						RequestStream;
		HttpResponse							Response;

		HttpMethod::HttpMethodType				Method;
		aconnect::string						VirtualPath;
		aconnect::string						MappedVirtualPath;
		boost::filesystem::path					FileSystemPath;
		
		HttpServerSettings*						GlobalSettings;
		aconnect::Logger*						Log;	

		boost::filesystem::path					UploadsDirPath;
		
		aconnect::str2str_map					GetParameters;
		aconnect::str2str_map					PostParameters;
		aconnect::str2str_map					Cookies;

		std::map <aconnect::string, UploadFileInfo>			UploadedFiles;
	};


	class HttpServer
	{
	private:
		static HttpServerSettings* globalSettings_;
		
	public:
		static HttpServerSettings* GlobalSettings() throw (std::runtime_error) {
			if  (globalSettings_ == NULL)
				throw std::runtime_error ("Global HTTP server settings is not loaded");
			if  (globalSettings_->logger() == NULL)
				throw std::runtime_error ("Global HTTP server logger is not initiliazed");

			return globalSettings_;
		}

		static aconnect::Logger* Log() throw (std::runtime_error) {
			return GlobalSettings()->logger();
		}

		static void setGlobalSettings (HttpServerSettings* settings) {
			globalSettings_ = settings;
		}

		static boost::detail::atomic_count RequestsCount;

		/**
		* Process HTTP request (and following keep-alive requests on opened socket)
		* @param[in]	client		Filled aconnect::ClientInfo object (with opened socket)
		*/
		static void processConnection (const aconnect::ClientInfo& client);

		/**
		* Process worker creation fail (503 HTTP status will be sent)
		* @param[in]	clientSock		opened client socket
		*/
		static void processWorkerCreationError (const aconnect::socket_type clientSock);

		static void redirectRequest (HttpContext& context,
			aconnect::string_constref virtualPath, int status = 302 /*Found*/); 

		static void processError404 (HttpContext& context);

		static void processError403 (HttpContext& context,
			aconnect::string_constptr message);

		static void processError405 (HttpContext& context,
			aconnect::string_constref allowedMethods);

		static void processError406 (HttpContext& context, 
			aconnect::string_constref message);
		
		// send server error response (500+)
		static void processServerError (HttpContext& context, 
			int status = 500, aconnect::string_constptr message = NULL);

		
	private:
		
		/**
		* Register HTTP request in server (increment count, write some logs)
		* @param[in/out]	context		Filled HttpContext instance
		*/
		static bool processRequest (HttpContext& context);

		static bool isMethodImplemented (HttpContext& context);
		
		static bool findTarget (HttpContext& context);

		/**
		* Run handlers registered for current directory against current target,
		* returns true if request was completed.
		* @param[in/out]	context		Filled HttpContext instance
		*/
		static bool runHandlers (HttpContext& context, const struct DirectorySettings& dirSettings);

		static void processDirectFileRequest (HttpContext& context);

		static void processDirectoryRequest (HttpContext& context, 
			const struct DirectorySettings& dirSettings);

		static aconnect::string formatParentDirRecord (const DirectorySettings& dirSettings, 
			aconnect::string_constref parentPath);
		static aconnect::string formatHeaderRecord (const DirectorySettings& dirSettings, 
			aconnect::string_constref virtualPath);
		static aconnect::string formatItemRecord (aconnect::string_constref itemTemplate,
			aconnect::string_constref itemUrl,
			aconnect::string_constref itemName,
			const size_t itemSize,
			std::time_t lastWriteTime);
		static aconnect::string formatFooterRecord (const DirectorySettings& dirSettings, 
			aconnect::string_constref virtualPath,
			const size_t fileCount, const size_t dirCount, const size_t errCount);

		inline static aconnect::string formatDateTime (const struct tm& dateTime)
		{
			const int buffSize = 20;
			aconnect::char_type buff[buffSize] = {0};

			int cnt = snprintf (buff, buffSize, "%.2d.%.2d.%.4d %.2d:%.2d:%.2d", 
				dateTime.tm_mday,
				dateTime.tm_mon + 1,
				dateTime.tm_year + 1900,
				dateTime.tm_hour,
				dateTime.tm_min,
				dateTime.tm_sec
				);

			return aconnect::string (buff, aconnect::util::min2(cnt, buffSize) );
		}
	};
}
#endif // AHTTP_SERVER_H
