/*
This file is part of [aconnect] library. 

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

#ifndef ACONNECT_BOOST_FORMAT_SAFE_H
#define ACONNECT_BOOST_FORMAT_SAFE_H

#	ifndef _CRT_SECURE_NO_WARNINGS
#		define _CRT_SECURE_NO_WARNINGS
#		define _CRT_SECURE_NO_WARNINGS_DEFINED_BY_ME
#	endif

#	ifndef _SCL_SECURE_NO_WARNINGS
#		define _SCL_SECURE_NO_WARNINGS
#		define _SCL_SECURE_NO_WARNINGS_DEFINED_BY_ME
#	endif

#	include <boost/format.hpp>

#	ifdef _CRT_SECURE_NO_WARNINGS_DEFINED_BY_ME
#		undef _CRT_SECURE_NO_WARNINGS
#	endif

#	ifdef _SCL_SECURE_NO_WARNINGS_DEFINED_BY_ME
#		undef _SCL_SECURE_NO_WARNINGS
#	endif

#endif


