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

#ifndef AHTTP_SERVER_SETTINGS_H
#define AHTTP_SERVER_SETTINGS_H
#pragma once

#include <stdexcept>
#include <boost/regex.hpp>


#include "aconnect/types.hpp"
#include "aconnect/complex_types.hpp"
#include "aconnect/logger.hpp"
#include "aconnect/server_settings.hpp"

namespace ahttp
{
	class HttpServerSettings;

	typedef bool (*init_handler_function) (const aconnect::str2str_map& params, 
		HttpServerSettings* globalSettings);

	typedef std::map <aconnect::string, struct DirectorySettings> directories_map;
	typedef std::vector<std::pair<bool, aconnect::string> > default_documents_vector;
	
	// key - extension, value - pointer to executor (process_request_function)
	typedef std::multimap <aconnect::string, void*> directory_handlers_map; 

	// key - registered handler name, value - handler settings
	typedef std::map <aconnect::string, struct HandlerInfo> global_handlers_map;

	typedef std::vector<std::pair<boost::regex, aconnect::string> > mappings_vector;

	namespace defaults
	{
		const bool EnableKeepAlive		= true;	
		const int KeepAliveTimeout		= 5;	// sec
		const int ServerSocketTimeout	= 900;	// sec
		const int CommandSocketTimeout	= 30;	// sec
		const size_t ResponseBufferSize	= 2 * 1024 * 1024;	// bytes
		const size_t MaxChunkSize				= 65535;	// bytes
		aconnect::string_constant ServerVersion = "ahttpserver";
		aconnect::string_constant DirectoryConfigFile = "directory.config";
	}
	
	struct settings_load_error : public std::runtime_error {
		
		settings_load_error (aconnect::string_constptr format, ...) : std::runtime_error ("") 
		{
			if (NULL == format) {
				message_ = "Settings loading failed";
			} else {
				FORMAT_VA_MESSAGE(format, message);
				message_.swap (message);
			}
		}

		virtual ~settings_load_error() throw () { }
				
		virtual const char * what() const throw () {	
			return (message_.c_str());
		}
	private:
		aconnect::string message_;
	};

	namespace SettingsTags
	{
		aconnect::string_constant RootElement = "settings";
		aconnect::string_constant ServerElement = "server";
		aconnect::string_constant LogElement = "log";
		aconnect::string_constant PathElement = "path";
		aconnect::string_constant RelativePathElement = "relative-path";
		aconnect::string_constant VirtualPathElement = "virtual-path";

		aconnect::string_constant DirectoryElement = "directory";
		aconnect::string_constant DefaultDocumentsElement = "default-documents";
		aconnect::string_constant MimeTypesElement = "mime-types";
		aconnect::string_constant DocumentElement = "document";
		aconnect::string_constant HandlersElement = "handlers";
		aconnect::string_constant MappingsElement = "mappings";

		aconnect::string_constant HandlerItemElement = "handler";

		aconnect::string_constant AddElement = "add";
		aconnect::string_constant RemoveElement = "remove";
		aconnect::string_constant TypeElement = "type";
		aconnect::string_constant ParameterElement = "parameter";
		aconnect::string_constant RegisterElement = "register";
		aconnect::string_constant RegexElement = "regex";
		aconnect::string_constant UrlElement = "url";
		

		aconnect::string_constant HeaderTemplateElement = "header-template";
		aconnect::string_constant DirectoryTemplateElement = "directory-template";
		aconnect::string_constant ParentDirectoryTemplateElement = "parent-directory-template";
		aconnect::string_constant VirtualDirectoryTemplateElement = "virtual-directory-template";
		aconnect::string_constant FileTemplateElement = "file-template";
		aconnect::string_constant FooterTemplateElement = "footer-template";

		aconnect::string_constant WorkersCountAttr = "workers-count";
		aconnect::string_constant PoolingEnabledAttr = "pooling-enabled";
		aconnect::string_constant WorkerLifeTimeAttr = "worker-life-time";
		aconnect::string_constant PortAttr = "port";
		aconnect::string_constant CommandPortAttr = "command-port";
		aconnect::string_constant RootAttr = "root";
		aconnect::string_constant LogLevelAttr = "log-level";
		aconnect::string_constant MaxFileSizeAttr = "max-file-size";
		
		aconnect::string_constant BrowsingEnabledAttr = "browsing-enabled";
		aconnect::string_constant NameAttr = "name";
		aconnect::string_constant ParentAttr = "parent";
		aconnect::string_constant CharsetAttr = "charset";

		aconnect::string_constant KeepAliveEnabledAttr = "keep-alive-enabled";
		aconnect::string_constant KeepAliveTimeoutAttr = "keep-alive-timeout";
		aconnect::string_constant ServerSocketTimeoutAttr = "server-socket-timeout";
		aconnect::string_constant CommandSocketTimeoutAttr = "command-socket-timeout";
		aconnect::string_constant ResponseBufferSizeAttr = "response-buffer-size";
		
		aconnect::string_constant VersionAttr = "version";
		aconnect::string_constant MaxChunkSizeAttr = "max-chunk-size";
		aconnect::string_constant DirectoryConfigFileAttr = "directory-config-file";
		
		aconnect::string_constant ExtAttr = "ext";
		aconnect::string_constant FileAttr = "file";
		aconnect::string_constant DefaultExtAttr = "default-ext";
		
		aconnect::string_constant AppPathMark = "{app-path}";
		aconnect::string_constant NameMark = "{name}";
		aconnect::string_constant UrlMark = "{url}";
		aconnect::string_constant SizeMark = "{size}";
		aconnect::string_constant TimeMark = "{time}";
		aconnect::string_constant PageUrlMark = "{page-url}";
		aconnect::string_constant ParentUrlMark = "{parent-url}";

		aconnect::string_constant FilesCountMark = "{files-count}";
		aconnect::string_constant DirectoriesCountMark = "{directories-count}";
		aconnect::string_constant ErrorsCountMark = "{errors-count}";

		aconnect::string_constant TabulatorMark = "{tab}";

		aconnect::string_constant BooleanTrue = "true";
		aconnect::string_constant BooleanFalse = "false";


		aconnect::string_constant ProcessRequestFunName = "processHandlerRequest";
		aconnect::string_constant InitFunName = "initHandler";

		aconnect::string_constant AllExtensionsMark = "*";
	}

	struct HandlerInfo
	{
		aconnect::string		pathToLoad;
		aconnect::string		defaultExtension;
		void*					processRequestFunc;
		void*					initFunc;
		aconnect::str2str_map	params;

		HandlerInfo() :  processRequestFunc (NULL), initFunc (NULL) {}
	};

	struct DirectorySettings
	{
		DirectorySettings ();

		aconnect::string name;
		aconnect::string parentName;
		aconnect::string relativePath;	// virtual path from parent
		aconnect::string virtualPath;		// full virtual path
		aconnect::string realPath;		// real physical path
		int browsingEnabled;				// -1: unknown; 0: false; 1: true
		bool isLinkedDirectory;
		aconnect::string charset;

		default_documents_vector defaultDocuments; // bool - add/remove (false/true)
		directory_handlers_map handlers;

		mappings_vector	mappings;
		
		aconnect::string headerTemplate,
			directoryTemplate,
			parentDirectoryTemplate,
			virtualDirectoryTemplate,
			fileTemplate,
			footerTemplate;
	};


	class HttpServerSettings
	{
	public:
		

		HttpServerSettings();
		~HttpServerSettings();

		void load (aconnect::string_constptr docPath) throw (settings_load_error);
		
		// properties
		inline aconnect::port_type port() const {
			assert (port_ != -1);
			return port_;
		}
		
		inline const aconnect::string& root() const {
			return rootDirName_;
		}
		inline void setRoot(aconnect::string_constptr root) {
			assert (root);
			rootDirName_ = root;
		}

		inline const aconnect::ServerSettings serverSettings() const {
			return settings_;
		}

		inline const aconnect::string appLocaton() const			{		return appLocaton_;				}
		inline void setAppLocaton (aconnect::string location)		{		appLocaton_ = location;			}

		inline aconnect::Logger* logger()							{		return logger_;					}
		inline void setLogger (aconnect::Logger* logger)			{		logger_ = logger;				}

		inline const aconnect::string serverVersion() const		{		return serverVersion_;			}
		inline void setServerVersion (aconnect::string version)	{		serverVersion_ = version;		}

		inline const aconnect::Log::LogLevel logLevel() const		{		return logLevel_;				}
		inline const aconnect::string logFileTemplate() const		{		return logFileTemplate_;		}
		inline const size_t	maxLogFileSize() const					{		return maxLogFileSize_;			}
		inline const aconnect::port_type commandPort() const		{		return commandPort_;			}
		
		inline const bool isKeepAliveEnabled() const				{		return enableKeepAlive_;		}
		inline const int keepAliveTimeout() const					{		return keepAliveTimeout_;		}
		inline const int commandSocketTimeout() const				{		return commandSocketTimeout_;	}
		inline const size_t responseBufferSize() const				{		return responseBufferSize_;		}
		inline const size_t maxChunkSize() const					{		return maxChunkSize_;			}
		inline const directories_map& Directories() const			{		return directories_;			}

		void updateAppLocationInPath (aconnect::string &pathStr) const;
		
		aconnect::string getMimeType (aconnect::string_constref ext) const;

		void initHandlers ();

		static bool loadIntAttribute (class TiXmlElement* elem, 
			aconnect::string_constptr attr, int &value);
		
		static bool loadBoolAttribute (class TiXmlElement* elem, 
			aconnect::string_constptr attr, bool &value);
				

	protected:
		void loadServerSettings (class TiXmlElement* serverElem) throw (settings_load_error);
		void loadLoggerSettings (class TiXmlElement* logElement) throw (settings_load_error);
		
		DirectorySettings loadDirectory (class TiXmlElement* dirElement) throw (settings_load_error);

		void tryLoadLocalSettings (aconnect::string_constref filePath, DirectorySettings& dirInfo) throw (settings_load_error);
		/**
		*	Now loads only handlers, mappings, default-documents
		*
		*/
		void loadLocalDirectorySettings (class TiXmlElement* dirElement, DirectorySettings& dirInfo) throw (settings_load_error);

		void fillDirectoriesMap (std::vector <DirectorySettings>& directoriesList, std::vector <DirectorySettings>::iterator parent);
		void loadMimeTypes (class TiXmlElement* mimeTypesElement) throw (settings_load_error);
		
		void loadHandlers (class TiXmlElement* handlersElement) throw (settings_load_error);
		void registerHandler (aconnect::string_constref handlerName, HandlerInfo& info) 
			throw (settings_load_error);

		void loadDirectoryIndexDocuments (class TiXmlElement* documentsElement, DirectorySettings& dirInfo) throw (settings_load_error);
		void loadDirectoryHandlers (class TiXmlElement* handlersElement, DirectorySettings& dirInfo) throw (settings_load_error);
		void loadDirectoryMappings (class TiXmlElement* mappingsElement, DirectorySettings& dirInfo) throw (settings_load_error);

	protected:
		aconnect::ServerSettings settings_;
		aconnect::port_type port_;
		aconnect::port_type commandPort_;
		aconnect::string rootDirName_;

		aconnect::string appLocaton_;
		// logger
		aconnect::Log::LogLevel logLevel_;
		aconnect::string logFileTemplate_;
		size_t maxLogFileSize_;

		bool enableKeepAlive_;
		int keepAliveTimeout_;
		int commandSocketTimeout_;
		size_t responseBufferSize_;
		size_t maxChunkSize_;

		directories_map directories_;
		aconnect::str2str_map mimeTypes_;

		aconnect::Logger*	logger_;
		aconnect::string	serverVersion_;

		global_handlers_map registeredHandlers_;
		bool				firstLoad_;

		aconnect::string	directoryConfigFile_;
	};
}
#endif // AHTTP_SERVER_SETTINGS_H
