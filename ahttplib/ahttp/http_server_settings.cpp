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

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <assert.h>

#include "tinyxml/tinyxml.h"

#include "aconnect/util.hpp"
#include "ahttp/http_support.hpp"
#include "ahttp/http_server_settings.hpp"

#if defined(__GNUC__)
#	include <dlfcn.h>
#endif

namespace algo = boost::algorithm;
namespace fs = boost::filesystem;

namespace ahttp
{

	DirectorySettings::DirectorySettings () : 
		browsingEnabled (-1), 
		isLinkedDirectory(false),
		charset ()
	{

	}

	HttpServerSettings::HttpServerSettings() :
		port_(-1), 
		commandPort_ (-1),
		logLevel_ (aconnect::Log::Debug), 				   
		maxLogFileSize_ (aconnect::Log::MaxFileSize), 
		enableKeepAlive_ (defaults::EnableKeepAlive),
		keepAliveTimeout_ (defaults::KeepAliveTimeout),
		commandSocketTimeout_ (defaults::CommandSocketTimeout),
		responseBufferSize_ (defaults::ResponseBufferSize),
		maxChunkSize_ (defaults::MaxChunkSize),
		logger_ (NULL),
		serverVersion_ (defaults::ServerVersion),
		firstLoad_ (true),
		directoryConfigFile_ (defaults::DirectoryConfigFile)
	{
		settings_.socketReadTimeout = defaults::ServerSocketTimeout;
		settings_.socketWriteTimeout = defaults::ServerSocketTimeout;
	}

	HttpServerSettings::~HttpServerSettings()
	{
	}

	aconnect::string HttpServerSettings::getMimeType (aconnect::string_constref ext) const 
	{
		using namespace aconnect;
		
		str2str_map::const_iterator it = mimeTypes_.find(ext);
		if (it != mimeTypes_.end())
			return it->second;

		return detail::ContentTypeOctetStream;			
	}


	void HttpServerSettings::load (aconnect::string_constptr docPath) throw (settings_load_error)
	{
		if ( !docPath || docPath[0] == '\0' ) 
			throw settings_load_error ("Empty settings file path to load");

		TiXmlDocument doc( docPath );
		bool loadOkay = doc.LoadFile();

		if ( !loadOkay ) {
			boost::format msg ("Could not load settings file \"%s\". Error=\"%s\".");
			msg % docPath;
			msg % doc.ErrorDesc();

			throw settings_load_error (boost::str (msg).c_str());
		}
		
		TiXmlElement* root = doc.RootElement( );
		assert ( root );
		if ( !aconnect::util::equals(root->Value(), SettingsTags::RootElement, false)  ) 
			throw settings_load_error ("Invalid setting root element");
		
		if (firstLoad_)
		{
			TiXmlElement* serverElem = root->FirstChildElement (SettingsTags::ServerElement);
			assert (serverElem);
			if ( !serverElem ) 
				throw settings_load_error ("Cannot find <server> element");

			// server setup
			loadServerSettings (serverElem);

			TiXmlElement* logElement = serverElem->FirstChildElement (SettingsTags::LogElement);
			if ( !serverElem ) 
				throw settings_load_error ("Cannot find <log> element");

			// logger setup
			loadLoggerSettings (logElement);
		} 
		else 
		{
			// clear directories info
			directories_.clear();
		}
		
		TiXmlElement* directoryElem = root->FirstChildElement (SettingsTags::DirectoryElement);
		if ( !directoryElem ) 
			throw settings_load_error ("At least one <directory> element must be");

		std::vector <DirectorySettings> directoriesList;
		do {
			directoriesList.push_back ( loadDirectory (directoryElem));
			directoryElem = directoryElem->NextSiblingElement(SettingsTags::DirectoryElement);
		
		} while (directoryElem);

		std::vector <DirectorySettings>::iterator it = directoriesList.end();
		for (it = directoriesList.begin(); 
			it != directoriesList.end() && it->name != rootDirName_; ++it );

		if (it == directoriesList.end())
			throw settings_load_error ("There is no <directory> record with name \"%s\"", 
				rootDirName_.c_str());

		if (it->realPath.empty())
			throw settings_load_error ("Empty path in root <directory> record");
		
		// work with filesystem
		try
		{
			fs::path rootPath (it->realPath, fs::portable_directory_name);
			if ( !fs::exists (rootPath) )
				throw settings_load_error ("Incorrect path in root <directory> record - path does not exist");
			if ( !fs::is_directory (rootPath) )
				throw settings_load_error ("Incorrect path in root <directory> record - target is not a directory");

			it->realPath = rootPath.directory_string();
			it->virtualPath = detail::Slash;

			// try to load local directory configuration
			fs::path dirConfigFile = fs::path (it->realPath, fs::native) / fs::path(directoryConfigFile_, fs::portable_file_name);
			tryLoadLocalSettings (dirConfigFile.string(), *it);

			// register root
			directories_ [it->virtualPath] = *it;

			fillDirectoriesMap (directoriesList, it);

			firstLoad_ = false;

		} catch (fs::basic_filesystem_error<fs::path> &err) {
			throw settings_load_error (
				"Directories info loading failed - 'basic_filesystem_error' caught: %s, "
				"system error code: %d, path 1: %s, path 2: %s", 
				err.what(), err.system_error(),
				err.path1().string().c_str(),
				err.path2().string().c_str()); 

		} catch (fs::filesystem_error &err) {
			throw settings_load_error (
				"Directories info loading failed - 'filesystem_error' caught: %s, "
				"system error code: %d", 
				err.what(), err.system_error());

		} catch (std::exception &ex)  {
			throw settings_load_error ("Directories info loading failed - exception [%s]: %s", 
				typeid(ex).name(), ex.what());;

		} catch (...)  {
			throw settings_load_error ("Directories info loading failed - unknown exception caught");
		}
	}

	bool HttpServerSettings::loadIntAttribute (class TiXmlElement* elem, 
		aconnect::string_constptr attr, int &value) 
	{
		int n;
		if ( elem->QueryIntAttribute (attr, &n) != TIXML_SUCCESS )
			return false;
			
		value = n;
		return true;
	}

	bool HttpServerSettings::loadBoolAttribute (class TiXmlElement* elem, 
		aconnect::string_constptr attr, bool &value) 
	{
		aconnect::string s;
		if ( elem->QueryValueAttribute( attr, &s) != TIXML_SUCCESS) 
			return false;

		value = aconnect::util::equals (s, SettingsTags::BooleanTrue);
		return true;
	}


	void HttpServerSettings::fillDirectoriesMap (std::vector <DirectorySettings>& directoriesList, 
											 std::vector <DirectorySettings>::iterator parent)
	{
		using namespace aconnect;
		
		
		std::vector <DirectorySettings>::iterator childIter;
		for (childIter = directoriesList.begin(); childIter != directoriesList.end(); ++childIter ) 
		{
			if (childIter->parentName == parent->name) 
			{
				if (childIter->virtualPath.empty())
					throw settings_load_error ("Empty <virtual-path> for nested directory: %s", 
						childIter->name.c_str());
				string virtualPathInit = childIter->virtualPath;

				childIter->virtualPath = parent->virtualPath + childIter->virtualPath;
				if (childIter->virtualPath.substr(childIter->virtualPath.length() - 1) != detail::Slash)
					childIter->virtualPath += detail::Slash;

				// get real path
				fs::path childPath;
				if (!childIter->realPath.empty()) {
					childPath = fs::path (childIter->realPath, fs::portable_directory_name);
					childIter->isLinkedDirectory = true;
					childIter->relativePath = virtualPathInit;
				} else {
					childPath = fs::complete (fs::path (childIter->relativePath, fs::portable_directory_name),
										fs::path (parent->realPath, fs::native));
				}
				
				if ( !fs::exists (childPath) )
					throw settings_load_error ("Incorrect path in <directory> record - path does not exist,\
											   directory: %s", childIter->name.c_str());
				if ( !fs::is_directory (childPath) )
					throw settings_load_error ("Incorrect path in <directory> record - target is not a directory,\
											   directory: %s", childIter->name.c_str());

				childIter->realPath = childPath.directory_string();
				
				// try to load local directory configuration
				fs::path dirConfigFile = fs::path (childIter->realPath, fs::native) / fs::path(directoryConfigFile_, fs::portable_file_name);

				tryLoadLocalSettings (dirConfigFile.string(), *childIter);

				// copy properties
				if (childIter->browsingEnabled == -1)
					childIter->browsingEnabled = parent->browsingEnabled;

				if (childIter->charset.empty())
					childIter->charset = parent->charset;

				if (childIter->fileTemplate.empty())	childIter->fileTemplate = parent->fileTemplate;
				if (childIter->directoryTemplate.empty())	childIter->directoryTemplate = parent->directoryTemplate;
				if (childIter->parentDirectoryTemplate.empty())	childIter->parentDirectoryTemplate = parent->parentDirectoryTemplate;
				if (childIter->virtualDirectoryTemplate.empty())	childIter->virtualDirectoryTemplate = parent->virtualDirectoryTemplate;
				if (childIter->headerTemplate.empty())	childIter->headerTemplate = parent->headerTemplate;
				if (childIter->footerTemplate.empty())	childIter->footerTemplate = parent->footerTemplate;

				// fill default documents list
				default_documents_vector defDocsList = parent->defaultDocuments;

				default_documents_vector::iterator iter = childIter->defaultDocuments.begin(), removeIter;
				while (iter != childIter->defaultDocuments.end()) 
				{
					if (iter->first && std::find (defDocsList.begin(), defDocsList.end(), *iter) == defDocsList.end()) {
						defDocsList.push_back(*iter);
					
					} else if (!iter->first) {
						removeIter = std::find (defDocsList.begin(), defDocsList.end(), std::make_pair(true, iter->second));
						if (removeIter == defDocsList.end())
							throw settings_load_error ("Cannot remove default document record \"%s\", in directory: %s - "
								"it is not declared in parent directory record.", 
									iter->second.c_str(), 
									childIter->name.c_str());
						else
							defDocsList.erase(removeIter);
					}
					iter++;
				}
				childIter->defaultDocuments = defDocsList;
				
				// copy handlers registraton
				directory_handlers_map::iterator handlerIter;
				for (handlerIter = parent->handlers.begin(); 
						handlerIter != parent->handlers.end(); ++handlerIter) 
				{
					if (childIter->handlers.find(handlerIter->first) == childIter->handlers.end())
						childIter->handlers.insert (*handlerIter);
				}

				directories_[childIter->virtualPath] = *childIter;
				
				fillDirectoriesMap (directoriesList, childIter);
			}

		}

	}

	void HttpServerSettings::tryLoadLocalSettings (aconnect::string_constref filePath, DirectorySettings& dirInfo) 
		throw (settings_load_error)
	{

		if (fs::exists (filePath))
		{
			TiXmlDocument doc( filePath.c_str() );

			if ( !doc.LoadFile() ) {
				boost::format msg ("Could not load local directory config file \"%s\". Error=\"%s\".");
				msg % filePath;
				msg % doc.ErrorDesc();

				throw settings_load_error (boost::str (msg).c_str());
			}

			TiXmlElement* root = doc.RootElement( );

			if ( !aconnect::util::equals(root->Value(), SettingsTags::DirectoryElement, false)  ) 
				throw settings_load_error ("Invalid local directory config file root element, file: %s",
					filePath.c_str());

			loadLocalDirectorySettings (root, dirInfo);
		}
	}

	void HttpServerSettings::loadServerSettings (TiXmlElement* serverElem) throw (settings_load_error)
	{
		using namespace aconnect;
		int intValue = 0, getAttrRes;
		string_constptr strValue;
		string stringValue;

		// version
		strValue = serverElem->Attribute (SettingsTags::VersionAttr);
		if (!util::isNullOrEmpty(strValue))
			serverVersion_ = strValue;

		// port
		getAttrRes = serverElem->QueryIntAttribute (SettingsTags::PortAttr, &intValue );
		if ( getAttrRes != TIXML_SUCCESS ) 
			throw settings_load_error ("Port number loading failed");
		port_ = intValue;

		// command port
		getAttrRes = serverElem->QueryIntAttribute (SettingsTags::CommandPortAttr, &intValue );
		if ( getAttrRes != TIXML_SUCCESS ) 
			throw settings_load_error ("Command port number loading failed");
		commandPort_ = intValue;

		// workers count - OPTIONAL
		loadIntAttribute (serverElem, SettingsTags::WorkersCountAttr, settings_.workerLifeTime);

		// pooling - OPTIONAL
		loadBoolAttribute (serverElem, SettingsTags::PoolingEnabledAttr, settings_.enablePooling);

		// worker life time - OPTIONAL
		loadIntAttribute (serverElem, SettingsTags::WorkerLifeTimeAttr, settings_.workerLifeTime);
		
		// read timeouts
		getAttrRes = serverElem->QueryIntAttribute (SettingsTags::ServerSocketTimeoutAttr, &intValue );
		settings_.socketWriteTimeout = 
			settings_.socketReadTimeout = 
				(getAttrRes == TIXML_SUCCESS ? intValue : defaults::ServerSocketTimeout);

		// load keep-alive mode
		loadBoolAttribute (serverElem, SettingsTags::KeepAliveEnabledAttr, enableKeepAlive_);
		
		getAttrRes = serverElem->QueryIntAttribute (SettingsTags::KeepAliveTimeoutAttr, &intValue );
		keepAliveTimeout_ = (getAttrRes == TIXML_SUCCESS ? intValue : defaults::KeepAliveTimeout);
			
		getAttrRes = serverElem->QueryIntAttribute (SettingsTags::CommandSocketTimeoutAttr, &intValue );
		commandSocketTimeout_ = (getAttrRes == TIXML_SUCCESS ? intValue : defaults::CommandSocketTimeout);

		// directory configuration file
		strValue = serverElem->Attribute (SettingsTags::DirectoryConfigFileAttr);
		if (!util::isNullOrEmpty(strValue))
			directoryConfigFile_ = strValue;

		// read HTTP settings
		strValue = serverElem->Attribute (SettingsTags::ResponseBufferSizeAttr);
		if (!util::isNullOrEmpty(strValue))
			responseBufferSize_ = boost::lexical_cast<size_t> (strValue);

		strValue = serverElem->Attribute (SettingsTags::MaxChunkSizeAttr);
		if (!util::isNullOrEmpty(strValue))
			maxChunkSize_ = boost::lexical_cast<size_t> (strValue);

		// root directory
		strValue = serverElem->Attribute( SettingsTags::RootAttr );
		if ( util::isNullOrEmpty(strValue) ) 
			throw settings_load_error ("Invalid root directory name");
		setRoot (strValue);

		TiXmlElement* mimeTypesElement = serverElem->FirstChildElement (SettingsTags::MimeTypesElement);
		if (!mimeTypesElement)
			throw settings_load_error ("<%s> not found in server settings", SettingsTags::MimeTypesElement);

		loadMimeTypes (mimeTypesElement);

		// load global handlers
		TiXmlElement* handlersElem = serverElem->FirstChildElement (SettingsTags::HandlersElement);
		if (handlersElem)
			loadHandlers (handlersElem);
	}

	void HttpServerSettings::loadLoggerSettings (TiXmlElement* logElement) throw (settings_load_error)
	{
		using namespace aconnect;
		assert (logElement);
		string_constptr strValue;

		// load level
		strValue = logElement->Attribute( SettingsTags::LogLevelAttr );
		if (_stricmp (strValue, Log::ErrorMsg) == 0)
			logLevel_ = Log::Error;
		else if (_stricmp (strValue, Log::WarningMsg) == 0)
			logLevel_ = Log::Warning;
		else if (_stricmp (strValue, Log::InfoMsg) == 0)
			logLevel_ = Log::Info;
		else 
			logLevel_ = Log::Debug;
		
		int intValue = 0, getAttrRes;
		
		// load max file size
		getAttrRes = logElement->QueryIntAttribute (SettingsTags::MaxFileSizeAttr, &intValue );
		if (getAttrRes == TIXML_SUCCESS)
			maxLogFileSize_ = intValue;

		TiXmlElement* pathElement = logElement->FirstChildElement (SettingsTags::PathElement);
		assert (pathElement);

		strValue = pathElement->GetText();
		if ( util::isNullOrEmpty(strValue) ) 
			throw settings_load_error ("Invalid log file template");
		
		logFileTemplate_ = strValue;
	}


	DirectorySettings HttpServerSettings::loadDirectory (TiXmlElement* directoryElem) throw (settings_load_error)
	{
		using namespace aconnect;
		assert (directoryElem);
		
		DirectorySettings ds;
		string strValue;
		string_constptr strPtrValue;
		int getAttrRes = 0;

		// load name
		getAttrRes = directoryElem->QueryValueAttribute( SettingsTags::NameAttr, &ds.name);
		if (getAttrRes != TIXML_SUCCESS)
			throw settings_load_error ("Directory does not have \"%s\" attribute",
				SettingsTags::NameAttr);
		
		// load browsing-enabled
		if (directoryElem->QueryValueAttribute( SettingsTags::BrowsingEnabledAttr, &strValue) == TIXML_SUCCESS) 
			ds.browsingEnabled = util::equals(strValue, SettingsTags::BooleanTrue) ? 1 : 0;

		// load charset
		if (directoryElem->QueryValueAttribute( SettingsTags::CharsetAttr, &strValue) == TIXML_SUCCESS) 
			ds.charset = strValue;
		
		// load parent
		getAttrRes = directoryElem->QueryValueAttribute( SettingsTags::ParentAttr, &strValue);
		if (getAttrRes == TIXML_SUCCESS) 
			ds.parentName = strValue;

		// path
		bool realPathDefined = false;
		TiXmlElement* pathElement = directoryElem->FirstChildElement (SettingsTags::PathElement);
		if (pathElement) {
			strPtrValue = pathElement->GetText();
			realPathDefined = true;

			if ( util::isNullOrEmpty(strPtrValue) ) 
				throw settings_load_error ("Empty path attribute for directory: %s", ds.name.c_str());
			ds.realPath = strPtrValue;
			
			updateAppLocationInPath (ds.realPath);
		}

		// virtual-path
		pathElement = directoryElem->FirstChildElement (SettingsTags::VirtualPathElement);
		if (pathElement) {
			strPtrValue = pathElement->GetText();
			if ( !util::isNullOrEmpty(strPtrValue) )
				ds.virtualPath = strPtrValue;
		}

		// relative-path
		pathElement = directoryElem->FirstChildElement (SettingsTags::RelativePathElement);
		if (pathElement) {
			if (realPathDefined)
				throw settings_load_error ("<%s> and <%s> must not be defined together,\
						directory: %s",
						SettingsTags::PathElement,
						SettingsTags::RelativePathElement,
						ds.name.c_str());
			
			strPtrValue = pathElement->GetText();
			if ( !util::isNullOrEmpty(strPtrValue) ) 
				ds.relativePath = strPtrValue;
		}


		TiXmlElement* fileTemplate = directoryElem->FirstChildElement (SettingsTags::FileTemplateElement);
		TiXmlElement* directoryTemplate = directoryElem->FirstChildElement (SettingsTags::DirectoryTemplateElement);
		TiXmlElement* virtualDirectoryTemplate = directoryElem->FirstChildElement (SettingsTags::VirtualDirectoryTemplateElement);
		TiXmlElement* parentDirectoryTemplate = directoryElem->FirstChildElement (SettingsTags::ParentDirectoryTemplateElement);

		if ( (!fileTemplate && directoryTemplate) || (fileTemplate && !directoryTemplate))
			throw settings_load_error ("<directory-template> and <file-template> should be defined together, directory: %s", ds.name.c_str());

		if (ds.browsingEnabled == 1 && !fileTemplate)
			throw settings_load_error ("<directory-template> and <file-template> must be defined together,\
									   when browsing enabled, directory: %s", ds.name.c_str());

		if (fileTemplate && directoryTemplate) {
			strPtrValue = fileTemplate->GetText();		if (strPtrValue) ds.fileTemplate = strPtrValue;
			strPtrValue = directoryTemplate->GetText();	if (strPtrValue) ds.directoryTemplate = strPtrValue;
		}
		if (virtualDirectoryTemplate && (strPtrValue = virtualDirectoryTemplate->GetText()) ) 
			ds.virtualDirectoryTemplate = strPtrValue;
		
		if (parentDirectoryTemplate && (strPtrValue = parentDirectoryTemplate->GetText())) 
			ds.parentDirectoryTemplate = strPtrValue;
		

		// add tabulators
		algo::replace_all (ds.fileTemplate, SettingsTags::TabulatorMark, "\t");
		algo::replace_all (ds.directoryTemplate, SettingsTags::TabulatorMark, "\t");
		algo::replace_all (ds.virtualDirectoryTemplate, SettingsTags::TabulatorMark, "\t");
		algo::replace_all (ds.parentDirectoryTemplate, SettingsTags::TabulatorMark, "\t");


		TiXmlElement* headerTemplate = directoryElem->FirstChildElement (SettingsTags::HeaderTemplateElement);
		if (headerTemplate) {
			strPtrValue = headerTemplate->GetText(); 
			if (strPtrValue) 
				ds.headerTemplate = strPtrValue;
		}

		TiXmlElement* footerTemplate = directoryElem->FirstChildElement (SettingsTags::FooterTemplateElement);
		if (footerTemplate) {
			strPtrValue = footerTemplate->GetText(); 
			if (strPtrValue) 
				ds.footerTemplate = strPtrValue;
		}
		
		loadLocalDirectorySettings (directoryElem, ds);

		return ds;
	}

	void HttpServerSettings::loadLocalDirectorySettings (class TiXmlElement* directoryElem, DirectorySettings& dirInfo) 
		throw (settings_load_error) 
	{
		// load default documents
		TiXmlElement* defDocumentsElem = directoryElem->FirstChildElement (SettingsTags::DefaultDocumentsElement);
		if (defDocumentsElem)
			loadDirectoryIndexDocuments (defDocumentsElem, dirInfo);
		
		// load directory handlers
		TiXmlElement* handlersElem = directoryElem->FirstChildElement (SettingsTags::HandlersElement);
		if (handlersElem)
			loadDirectoryHandlers (handlersElem, dirInfo);

		// load directory mappings
		TiXmlElement* mappings = directoryElem->FirstChildElement (SettingsTags::MappingsElement);
		if (mappings)
			loadDirectoryMappings (mappings, dirInfo);

	}


	void HttpServerSettings::updateAppLocationInPath (aconnect::string &pathStr) const 
	{
		if ( pathStr.find(SettingsTags::AppPathMark) != aconnect::string::npos)
				algo::replace_first (pathStr, SettingsTags::AppPathMark, appLocaton_);
	}

	void HttpServerSettings::loadMimeTypes (class TiXmlElement* mimeTypesElement) throw (settings_load_error)
	{
		aconnect::string_constptr filePath =
			mimeTypesElement->Attribute(SettingsTags::FileAttr);

		if ( filePath )
		{
			aconnect::string filePathStr (filePath);
			
			updateAppLocationInPath (filePathStr);
		
			TiXmlDocument doc;
			bool loaded = doc.LoadFile (filePathStr);

			if ( !loaded ) {
				boost::format msg ("Could not load MIME-types definition file \"%s\". Error=\"%s\".");
				msg % filePathStr;
				msg % doc.ErrorDesc();
				throw settings_load_error (boost::str (msg).c_str());
			}

			TiXmlElement* root = doc.RootElement( );
			assert ( root );
			if ( !aconnect::util::equals (root->Value(), SettingsTags::MimeTypesElement, false) ) 
				throw settings_load_error ("Invalid root element in MIME-types definition file");

			loadMimeTypes (root);
		}

		// load sub-nodes

		TiXmlElement* typeElem = mimeTypesElement->FirstChildElement (SettingsTags::TypeElement);
		if ( !typeElem ) 
			return;
		
		aconnect::string_constptr strPtrValue;
		aconnect::string strValue;
		int getAttrRes = 0;

		do {
			// load name
			getAttrRes = typeElem->QueryValueAttribute( SettingsTags::ExtAttr, &strValue);
			if (getAttrRes == TIXML_NO_ATTRIBUTE)
				throw settings_load_error ("<%s> does not have \"%s\" attribute",
					SettingsTags::TypeElement, 
					SettingsTags::ExtAttr);

			strPtrValue = typeElem->GetText(); 
			if (strPtrValue) 
				mimeTypes_ [strValue] = strPtrValue;

			typeElem = typeElem->NextSiblingElement (SettingsTags::TypeElement);

		} while (typeElem);
	}

	void HttpServerSettings::loadHandlers (class TiXmlElement* handlersElement) 
		throw (settings_load_error)
	{
		using namespace aconnect;

		string_constptr strPtrValue;
		string handlerName, strValue;
		int getAttrRes;
		TiXmlElement* childElem;

		TiXmlElement* handlerElem = handlersElement->FirstChildElement (SettingsTags::HandlerItemElement);
		
		while (handlerElem) 
		{
			HandlerInfo info;

			getAttrRes = handlerElem->QueryValueAttribute( SettingsTags::NameAttr, &handlerName);
			if (getAttrRes == TIXML_NO_ATTRIBUTE)
				throw settings_load_error ("Global <%s> does not have \"%s\" attribute - it is required",
					SettingsTags::HandlerItemElement, 
					SettingsTags::NameAttr);

			if ((handlerElem->QueryValueAttribute( SettingsTags::DefaultExtAttr, &strValue) == TIXML_SUCCESS)) {
				if (strValue.empty())
					throw settings_load_error ("Handler \"%s\" has empty \"%s\" attribute",
						handlerName.c_str(), 
						SettingsTags::DefaultExtAttr);
				info.defaultExtension = strValue;
			}

			childElem = handlerElem->FirstChildElement(SettingsTags::PathElement);
			if (!childElem)
				throw settings_load_error ("Handler \"%s\" has no <%s> element ", 
					handlerName.c_str(), 
					SettingsTags::PathElement);

			strPtrValue = childElem->GetText();
			if (util::isNullOrEmpty(strPtrValue))
				throw settings_load_error ("Handler \"%s\" has empty <%s> element ", 
					handlerName.c_str(), 
					SettingsTags::PathElement);

			info.pathToLoad = strPtrValue;

			// load parameters
			childElem = handlerElem->FirstChildElement (SettingsTags::ParameterElement);
			
			while (childElem) 
			{
				getAttrRes = childElem->QueryValueAttribute( SettingsTags::NameAttr, &strValue);
				if (getAttrRes == TIXML_NO_ATTRIBUTE)
					throw settings_load_error ("<%s> for handler \"%s\" have no \"%s\" attribute",
						SettingsTags::ParameterElement, 
						handlerName.c_str(), 
						SettingsTags::NameAttr );

				strPtrValue = childElem->GetText();
				if (NULL != strPtrValue)
					info.params[strValue] = strPtrValue;

				childElem = childElem->NextSiblingElement (SettingsTags::HandlerItemElement);
			}
			
			registerHandler (handlerName, info);

			handlerElem = handlerElem->NextSiblingElement (SettingsTags::ParameterElement);
		}
	}


	void HttpServerSettings::registerHandler (aconnect::string_constref handlerName, HandlerInfo& info) 
		throw (settings_load_error)
	{
		aconnect::string pathToLoad (info.pathToLoad);
		updateAppLocationInPath (pathToLoad);

		if (registeredHandlers_.find (handlerName) != registeredHandlers_.end() )
			throw settings_load_error ("Handler \"%s\" has been already loaded", handlerName.c_str());

#if defined (WIN32)		
		HMODULE dll = ::LoadLibraryA (pathToLoad.c_str());

		if (NULL == dll)
			throw settings_load_error ("Handler loading failed, library: %s", pathToLoad.c_str());

		info.processRequestFunc = ::GetProcAddress (dll, SettingsTags::ProcessRequestFunName);
		if (!info.processRequestFunc) 
			throw settings_load_error ("Request processing function loading failed, "
				"library: %s, error code: %d", pathToLoad.c_str(), ::GetLastError());

		info.initFunc = ::GetProcAddress (dll, SettingsTags::InitFunName);
		if (!info.initFunc) 
			throw settings_load_error ("Handler initialization function loading failed, "
				"library: %s, error code: %d", pathToLoad.c_str(), ::GetLastError());
#else
		void * dll = dlopen (pathToLoad.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND );
		if (NULL == dll) {
			aconnect::string_constptr errorMsg = dlerror (); 
			throw settings_load_error ("Handler loading failed, library: %s, error: %s", 
				pathToLoad.c_str(), errorMsg ? errorMsg : "Unknown error");
		}

		info.processRequestFunc = dlsym (dll, SettingsTags::ProcessRequestFunName);
		if (!info.processRequestFunc) 
			throw settings_load_error ("Request processing function loading failed, "
				"library: %s, error: %s", pathToLoad.c_str(), dlerror());
		
		info.initFunc = dlsym (dll, SettingsTags::InitFunName);
		if (!info.initFunc) 
			throw settings_load_error ("Handler initialization function loading failed, "
				"library: %s, error: %s", pathToLoad.c_str(), dlerror());

#endif
		
		registeredHandlers_[handlerName] = info;
	}

	void HttpServerSettings::loadDirectoryIndexDocuments (class TiXmlElement* documentsElement, DirectorySettings& ds) 
		throw (settings_load_error)
	{
		aconnect::string_constptr strPtrValue;

		TiXmlElement* elem = documentsElement->FirstChildElement (SettingsTags::AddElement);
		while (elem) {
			if ( (strPtrValue = elem->GetText()) ) 
				ds.defaultDocuments.push_back (std::make_pair (true, strPtrValue));

			elem = elem->NextSiblingElement (SettingsTags::AddElement);
		}
		
		elem = documentsElement->FirstChildElement(SettingsTags::RemoveElement);
		while (elem) {
			if ( (strPtrValue = elem->GetText()) ) 
				ds.defaultDocuments.push_back (std::make_pair (false, strPtrValue));
			elem = elem->NextSiblingElement (SettingsTags::RemoveElement);
		}
	}

	void HttpServerSettings::loadDirectoryHandlers (class TiXmlElement* handlersElement, DirectorySettings& dirInfo) 
		throw (settings_load_error)
	{
		aconnect::string ext, name;
		int getAttrRes;

		TiXmlElement* item = handlersElement->FirstChildElement (SettingsTags::RegisterElement);
		
		while (item) 
		{
			getAttrRes = item->QueryValueAttribute( SettingsTags::NameAttr, &name);
			if (getAttrRes == TIXML_NO_ATTRIBUTE)
				throw settings_load_error ("<%s> does not have \"%s\" attribute, directory: %s",
					SettingsTags::RegisterElement, SettingsTags::NameAttr, dirInfo.name.c_str());
			
			if (registeredHandlers_.find(name) == registeredHandlers_.end())
				throw settings_load_error ("Handler \"%s\" is not registered, directory: %s",
					name.c_str(), dirInfo.name.c_str());
			
			const HandlerInfo& info = registeredHandlers_[name];

			getAttrRes = item->QueryValueAttribute( SettingsTags::ExtAttr, &ext);
			if (info.defaultExtension.empty() && (getAttrRes == TIXML_NO_ATTRIBUTE || ext.empty() ))
				throw settings_load_error ("Handler \"%s\" has not link to file extension, directory: %s",
					name.c_str(), dirInfo.name.c_str());
			else if (ext.empty())
				ext = info.defaultExtension;

			dirInfo.handlers.insert ( std::make_pair (ext, info.processRequestFunc) );

			item = item->NextSiblingElement (SettingsTags::RegisterElement);
		}

	}

	void HttpServerSettings::loadDirectoryMappings (class TiXmlElement* mappingsElement, DirectorySettings& dirInfo) 
		throw (settings_load_error)
	{
		
		TiXmlElement* item = mappingsElement->FirstChildElement (SettingsTags::RegisterElement);

		while (item) 
		{
			TiXmlElement* reElem = item->FirstChildElement(SettingsTags::RegexElement);
			if (NULL == reElem)
				throw settings_load_error ("<%s> does not have <%s> child element, directory: %s",
					SettingsTags::RegisterElement, SettingsTags::RegexElement, dirInfo.name.c_str());

			aconnect::string_constptr re = reElem->GetText();

			if ( aconnect::util::isNullOrEmpty(re) )
				throw settings_load_error ("<%s> element is empty, directory: %s",
					SettingsTags::RegexElement, dirInfo.name.c_str());

			TiXmlElement* urlElem = item->FirstChildElement(SettingsTags::UrlElement);
			if (NULL == urlElem)
				throw settings_load_error ("<%s> does not have <%s> child element, directory: %s",
					SettingsTags::RegisterElement, SettingsTags::UrlElement, dirInfo.name.c_str());


			aconnect::string_constptr url = urlElem->GetText();
			if ( aconnect::util::isNullOrEmpty(url) )
				throw settings_load_error ("<%s> element is empty, directory: %s",
					SettingsTags::UrlElement, dirInfo.name.c_str());


			dirInfo.mappings.push_back( std::make_pair(boost::regex(re), url));

			item = item->NextSiblingElement (SettingsTags::RegisterElement);
		}


	}

	void HttpServerSettings::initHandlers ()
	{
		assert (logger_);
		global_handlers_map::const_iterator iter;
		for (iter = registeredHandlers_.begin(); iter != registeredHandlers_.end(); ++iter)
		{
			bool inited = reinterpret_cast<init_handler_function> (iter->second.initFunc) 
				(iter->second.params, this);
			
			if (!inited)
				throw settings_load_error ("Handler \"%s\" initialization failed failed",
					iter->first.c_str() );
		}
	}

} // namespace ahttp

