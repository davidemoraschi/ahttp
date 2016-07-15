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

// aconnect headers

#include "boost_format_safe.hpp"

#include <cerrno>
#include <cstdlib>

#include "error.hpp"
#include "network.hpp"

#ifdef __GNUC__
extern int errno;
#endif // __GNUC__

namespace aconnect
{
#ifdef WIN32
	string_constptr WinSocketErrors[] =
	{
		"Resource temporarily unavailable",
		"Operation now in progress",
		"Operation already in progress",
		"Socket operation on non-socket",
		"Destination address required",
		"Message too long",
		"Protocol wrong type for socket",
		"Bad protocol option",
		"Protocol not supported",
		"Socket type not supported",
		"Operation not supported",
		"Protocol family not supported",
		"Address family not supported by protocol family",
		"Address already in use",
		"Can't assign requested address",
		"Network is down",
		"Network is unreachable",
		"Network dropped connection on reset",
		"Software caused connection abort",
		"Connection reset by peer",
		"No buffer space available",
		"Socket is already connected",
		"Socket is not connected",
		"Cannot send after socket shutdown",
		"Too many references: can't splice",
		"Connection timed out",
		"Connection refused",
		"Too many levels of symbolic links",
		"File name too long",
		"Host is down",
		"No route to host"
	};

	const int WinSocketErrorFirst = WSAEWOULDBLOCK;
	const int WinSocketErrorLast = WinSocketErrorFirst + ( sizeof( WinSocketErrors ) / sizeof( WinSocketErrors[0] ) );
#endif

    // Selected errors
	const string  ErrorUnknown  = "Unknown error";
	const string  ErrorInvalidProtocol  = "Protocol is not supported";

	// WS errors
	const string  ErrorNetworkDisabled  = "Network subsystem is unusable";
	const string  ErrorInvalidWsaVersion = "This version of Windows Sockets not supported";
	const string  ErrorWsaStartupFailed = "Successful WSAStartup not yet performed";

	const string  ErrorDnsNonRecoverable = "DNS error: nonrecoverable error";
	const string  ErrorDnsNoData = "DNS error: Valid name, no data record of requested type";


	// socket_error methods
	err_type socket_error::getSocketError (socket_type sock)
	{
		err_type errCode = 0;
	
#ifdef WIN32
		errCode =  WSAGetLastError();

#elif defined(__GNUC__)
		errCode = errno;
		
		if (sock != INVALID_SOCKET) {
			err_type code = 0;
			unsigned int size = sizeof (code);

			if (SOCKET_ERROR != getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*) &code, &size)
					&& code != 0)
				errCode = code;
		}
#endif
		return errCode;
	}

	string socket_error::getSocketErrorDesc (err_type errCode, socket_type sock, string_constptr msg)
	{
        using boost::format;
        
		string errorDesc = ErrorUnknown;

	#ifdef WIN32			
		if ( errCode >= WinSocketErrorFirst && errCode < WinSocketErrorLast ) {
			errorDesc = WinSocketErrors[ errCode - WinSocketErrorFirst ];

		} else {
			switch (errCode)
			{
			case WSASYSNOTREADY:
				errorDesc = ErrorNetworkDisabled; break;
			case WSAVERNOTSUPPORTED:
				errorDesc = ErrorInvalidWsaVersion; break;
			case WSANOTINITIALISED:
				errorDesc = ErrorWsaStartupFailed; break;
			case WSANO_RECOVERY: 
				errorDesc = ErrorDnsNonRecoverable; break;
			case WSANO_DATA: 
				errorDesc = ErrorDnsNoData; break;

			default:
				if ( errCode > 0 )
					errorDesc = strerror (errCode);
			};
		}
	
	#elif defined(__GNUC__)
		if ( errCode == EPROTONOSUPPORT) {
			errorDesc = ErrorInvalidProtocol; 
		
		} else if ( errCode > 0 ) {
			errorDesc = strerror (errCode);
		}
	#endif

		if (msg != NULL && msg[0] != '\0') {
			errorDesc.insert (0, ": ");
			errorDesc.insert (0, msg);
		}

        return errorDesc + str(format (", code = %d, socket = %d") % errCode % sock);
	}

};

