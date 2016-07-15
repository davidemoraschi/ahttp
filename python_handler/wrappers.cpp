#include <boost/scoped_array.hpp>

#include "ahttplib.hpp"
#include "aconnect/util.hpp"

#include "wrappers.hpp"

//////////////////////////////////////////////////////////////////////////
// 
//	
aconnect::string_constptr RequestWrapper::param (aconnect::string_constref key)  
{
  processRequest();

  aconnect::str2str_map::const_iterator iter;
  if ((iter = context_->GetParameters.find(key)) != context_->GetParameters.end())
	  return iter->second.c_str();

  if ((iter = context_->PostParameters.find(key)) != context_->PostParameters.end())
	  return iter->second.c_str();

  if ((iter = context_->Cookies.find(key)) != context_->Cookies.end())
	  return iter->second.c_str();

  return NULL;
}

std::string RequestWrapper::rawRead (int buffSize)	{ 

  if (requestLoaded_) 
	  throwRequestProcessedError ();

  requestReadInRawForm_ = true;

  boost::scoped_array<aconnect::char_type> buff (new aconnect::char_type [buffSize]);
  int bytesRead = context_->RequestStream.read (buff.get(), buffSize);

  return std::string (buff.get(), bytesRead);
}

void RequestWrapper::processRequest() 
{
  if (requestReadInRawForm_)
	  return throwRequestReadRawError ();

  if (requestLoaded_)
	  return;

  // load request data
  context_->parseQueryStringParams ();
  context_->parseCookies();
  context_->parsePostParams ();

  requestLoaded_ = true;
}

//////////////////////////////////////////////////////////////////////////
// 
//	wrapper for ahttp::HttpContext - cover some HttpContext functionality
void HttpContextWrapper::write (aconnect::string_constref data) {
  assert (context_);
  if (!contentWritten_)
	  contentWritten_ = true;

  context_->setHtmlResponse();
  context_->Response.write (data);
}

void HttpContextWrapper::writeEscaped (aconnect::string_constref data) {
  write ( aconnect::util::escapeHtml(data) );
}

void HttpContextWrapper::flush () {
  assert (context_);
  context_->Response.flush();
}

void HttpContextWrapper::setContentType (aconnect::string_constref contentType, aconnect::string_constref charset) {
  assert (context_);
  if (contentWritten_)
	  return throwResponseWrittenError();

  context_->Response.Header.setContentType (contentType, charset);
}


