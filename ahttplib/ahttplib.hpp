/*
This file is part of [ahttplib] library. 

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


#ifndef AHTTP_COMMON_INCLUDES_H
#define AHTTP_COMMON_INCLUDES_H
#pragma once

#include "aconnect/aconnect.hpp"
#include "ahttp/http_server.hpp"

typedef bool (*process_request_function) (ahttp::HttpContext& context);


#if defined (WIN32)
#	define HANDLER_EXPORT extern "C" __declspec( dllexport )
#else
#	define HANDLER_EXPORT extern "C" 
#endif

#endif //AHTTP_COMMON_INCLUDES_H



