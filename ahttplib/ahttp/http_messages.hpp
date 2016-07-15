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

#ifndef AHTTP_MESSAGES_H
#define AHTTP_MESSAGES_H

#include "aconnect/types.hpp"

namespace ahttp { namespace messages 
{

	aconnect::string_constant ErrorUndefined = 
		"Detailed error description is not available.";

	// 301-303
	aconnect::string_constant ErrorDocumentMoved = 
		"This document may be found <a HREF=\"%s\">here</a>";

	aconnect::string_constant Error403_BrowseContent = 
		"This Virtual Directory does not allow contents to be listed.";

	aconnect::string_constant Error403_AccessDenied = 
		"Access to filesystem object is denied.";

	aconnect::string_constant Error404 = 
		"The requested page cannot be found: \"%s\"";

	aconnect::string_constant Error405 = 
		"The requested HTTP method is not allowed for the resource: %s.<br /> Allowed methods: %s.";

	aconnect::string_constant Error406_CharsetNotAllowed = 
		"The response's content charset is not allowed by client";

	aconnect::string_constant Error500 = 
		"Internal server error.<hr />%s";

	aconnect::string_constant ServerError_FileInsteadDirectory = 
		"File path retrieved instead of directory";

	aconnect::string_constant Error503 = 
		"The server is currently unable to handle the request due to a temporary overloading or maintenance of the server.";

	aconnect::string_constant Error501_MethodNotImplemented = 
		"Requested method (%s) is not implemented, try GET/POST/HEAD.";

	aconnect::string_constant Error500_RequestNotLoaded = 
		"HTTP request was not loaded correctly";

	aconnect::string_constant MessageFormat = 
		"<html><head><title>%s</title> \
		<style> BODY { padding: 10px; margin: 10px; font: 10pt Tahoma, Arial; color: #000;}\
		H1 {color: #cc0000; font: 14pt Tahoma, Arial; font-weight: bold; } \
		HR {height:1px; border: 1px solid #333; color: #333;} \
		TABLE {font-size: 100%%;}</style> \
		</head><body><h1>%s</h1>%s</body></html>";

	aconnect::string_constant MessageFormatInline = "<hr /><b style=\"color:#cc0000;\">%s</b><br/>%s";

}}


#endif // AHTTP_MESSAGES_H

