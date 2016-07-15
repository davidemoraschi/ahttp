#ifndef PYTHON_HANDLER_WRAPPERS_H
#define PYTHON_HANDLER_WRAPPERS_H

#include <boost/noncopyable.hpp>
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp> 
#include <boost/python/suite/indexing/map_indexing_suite.hpp> 

namespace python = boost::python;

#include "aconnect/types.hpp"

//////////////////////////////////////////////////////////////////////////
class TracebackLoaderWrapper : private boost::noncopyable
{
public:
	inline void write (aconnect::string_constref data)	{ 
		content += data;	
	}
	aconnect::string content;
};



//////////////////////////////////////////////////////////////////////////
// 
//	
class RequestHeaderWrapper : private boost::noncopyable
{
public:
	RequestHeaderWrapper (ahttp::HttpRequestHeader *header) : header_(header) {
		assert (header_);
	}

	inline size_t getLength() {
		return header_->Headers.size(); 
	}
	inline std::string getHeader (aconnect::string_constref key) {
		return header_->getHeader(key); 
	}
	inline bool hasHeader (aconnect::string_constref key) {
		return header_->hasHeader(key); 
	}
	inline aconnect::str2str_map& items () {
		return header_->Headers; 
	}
	inline std::string requestMethod()		{ return header_->Method;		}
	inline int requestHttpVerHigh()			{ return header_->VersionHigh;	}
	inline int requestHttpVerLow()			{ return header_->VersionLow;	}
	inline std::string userAgent()			{ return header_->getHeader ( ahttp::detail::HeaderUserAgent);	}

protected:
	ahttp::HttpRequestHeader *header_;

};



//////////////////////////////////////////////////////////////////////////
// 
//	
class RequestWrapper : private boost::noncopyable
{
public:
	RequestWrapper (ahttp::HttpContext *context) : 
		  context_(context), 
		  requestLoaded_ (false), 
		  requestReadInRawForm_(false)
	  {
		  assert (context);
	  }

	  inline const aconnect::str2str_map& getParameters ()		{ 
		  processRequest(); return context_->GetParameters; 
	  }
	  inline const aconnect::str2str_map& postParameters()		{ 
		  processRequest(); return context_->PostParameters;
	  }
	  inline const aconnect::str2str_map& cookies ()			{ 
		  processRequest(); return context_->Cookies; 
	  }
	  inline const std::map <aconnect::string, ahttp::UploadFileInfo>& files ()			{ 
		  processRequest(); return context_->UploadedFiles; 
	  }

	  aconnect::string_constptr param (aconnect::string_constref key);
	  std::string rawRead (int buffSize);
	  void processRequest();


protected:
	inline void throwRequestReadRawError () {
		PyErr_SetString (PyExc_RuntimeError, "HTTP request has been read in raw form");
		python::throw_error_already_set();
	}
	inline void throwRequestProcessedError () {
		PyErr_SetString (PyExc_RuntimeError, "HTTP request has been loaded to collections: use 'get' or 'post'");
		python::throw_error_already_set();
	}

	ahttp::HttpContext *context_;
	bool requestLoaded_;
	bool requestReadInRawForm_;

};


//////////////////////////////////////////////////////////////////////////
// 
//	wrapper for ahttp::HttpContext - cover some HttpContext functionality
class HttpContextWrapper : private boost::noncopyable
{
public:
	HttpContextWrapper (ahttp::HttpContext *context) : 
	  context_ (context), contentWritten_ (false), 
		  requestHeader_ (context ? &context->RequestHeader : NULL),
		  request_ (context) 
	  {
		  assert (context);
	  }

	  // request info
	  inline std::string virtualPath() {
		  return context_->VirtualPath; 
	  }
	  inline std::string scriptPath() {
		  return context_->FileSystemPath.string(); 
	  }

	  // client info
	  inline std::string clientIpAddr() {
		  return aconnect::util::formatIpAddr (context_->Client->ip); 
	  }
	  inline aconnect::port_type clientPort() {
		  return context_->Client->port; 
	  }
	  inline aconnect::port_type serverPort () {
		  return context_->Client->server->port(); 
	  }
	  //////////////////////////////////////////////////////////////////////////

	  inline void throwResponseWrittenError () {
		  PyErr_SetString (PyExc_RuntimeError, "HTTP response header cannot be set - response content writing already started");
		  python::throw_error_already_set();
	  }

	  inline bool isClientConnected () {
		  return context_->isClientConnected();
	  }


	  //////////////////////////////////////////////////////////////////////////
	  //
	  //	response modification

	  void write (aconnect::string_constref data);
	  void writeEscaped (aconnect::string_constref data);
	  void flush ();
	  void setContentType (aconnect::string_constref contentType, aconnect::string_constref charset="");

	  inline void setUtf8Html () {
		  setContentType (ahttp::detail::ContentTypeTextHtml, ahttp::detail::ContentCharsetUtf8);
	  }

protected:	
	ahttp::HttpContext *context_;
	bool contentWritten_;

public:
	RequestHeaderWrapper requestHeader_;
	RequestWrapper request_;

};



#endif // PYTHON_HANDLER_WRAPPERS_H

