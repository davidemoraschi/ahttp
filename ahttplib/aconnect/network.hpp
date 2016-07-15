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

#ifndef ACONNECT_COMMON_H
#define ACONNECT_COMMON_H

// socket related functionality
#ifdef WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <process.h>

#	pragma comment (lib, "ws2_32.lib")
#endif  //WIN32

#ifdef __GNUC__
#	include <fcntl.h> 
#	include <sys/types.h>
#	include <sys/stat.h> 
#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#endif  //__GNUC__

#include <cerrno>
#include <boost/algorithm/string.hpp>

#include "types.hpp"
#include "error.hpp"

namespace aconnect 
{
	struct socket_error;

	namespace network
	{
		const int SocketReadBufferSize = 512*1024; // bytes
#if defined (WIN32)
		const err_type ConnectionAbortCode = WSAECONNABORTED;
		const err_type ConnectionResetCode = WSAECONNRESET;
#else
		const err_type ConnectionAbortCode = ECONNABORTED;
		const err_type ConnectionResetCode = ECONNRESET;
#endif
	}


	class Initializer 
	{
	public:
		Initializer () {
			init ();
		}
		~Initializer () {
			destroy ();
		}        

		static void init () {
#ifdef WIN32
			WSADATA wsaData;
			int errCode = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
			if ( errCode != 0 ) 
				throw socket_error ( socket_error::getSocketErrorDesc(errCode, INVALID_SOCKET, "WSAStartup failed") );
#endif
		};
		static void destroy () {
#ifdef WIN32
			if (WSACleanup() == SOCKET_ERROR)	
				throw socket_error ( "WSACleanup failed");
#endif
		};
	};

	// Socket state checkers (check read/write availability)
	class SocketStateCheck {
		public:
			SocketStateCheck () : connectionWasClosed_ (false) { };
			virtual  ~SocketStateCheck() {};
			virtual void prepare (socket_type s) {	};
			virtual bool readCompleted (socket_type s, string_constref data) = 0;
			virtual bool isDataAvailable (socket_type s) { return true; }; // default behavior

			inline void setConnectionWasClosed (bool closed)	{	connectionWasClosed_ = closed;	}
			inline bool connectionWasClosed() const				{	return connectionWasClosed_;	}

	protected:
		bool				connectionWasClosed_;
	};
	
	class FastSelectReadSocketStateCheck : public SocketStateCheck
	{
		protected:
			fd_set	set_;
			timeval readTimeout_;

		public:
			FastSelectReadSocketStateCheck (long timeoutSec = 0) {
				readTimeout_.tv_sec = timeoutSec;
				readTimeout_.tv_usec = 0;
			}

			virtual void prepare (socket_type s) {
				FD_ZERO ( &set_);
				FD_SET (s, &set_);
			};

			virtual bool isDataAvailable (socket_type s) {
				int selectRes = select ( (int) s + 1, &set_, NULL, NULL, &readTimeout_);
			
				if (SOCKET_ERROR == selectRes)	
					throw socket_error (s, "Reading data from socket: select failed");

				if ( 0  == selectRes) // timeout expired
					return false;
			
				return true; // there is data to read
			}

			virtual bool readCompleted (socket_type s, string_constref data) {
				return !isDataAvailable(s); 
			}

	};

	class EndMarkSocketStateCheck : public SocketStateCheck
	{
		public:
			EndMarkSocketStateCheck (string_constref endMark = "\r\n\r\n") : endMark_(endMark) {	}

			virtual bool readCompleted (socket_type s, string_constref data) {
				return boost::algorithm::ends_with (data, endMark_);
			}
			
			inline const string& endMark() const {	return endMark_; }  
	protected:
		string endMark_;
	};
	
	namespace util 
	{

		// sockets support
		/*
		* create socket with selected domain (Address family) and type
		*/
		socket_type createSocket (int domain = AF_INET, int type = SOCK_STREAM) throw (socket_error);

		void closeSocket (socket_type s) throw (socket_error);

		/*
		*	Write content of 'data' to socket
		*	@param[in]	sock		Opened client socket
		*	@param[in]	data		Data to write (can contain '\0')
		*/
		void writeToSocket (socket_type sock, string_constref data) throw (socket_error);
		void writeToSocket (socket_type s, string_constptr buff, const int buffLen) throw (socket_error);
		string readFromSocket (const socket_type s, SocketStateCheck &stateCheck, bool throwOnConnectionReset = true, 
				const int buffSize = network::SocketReadBufferSize) throw (socket_error);
		void readIpAddress (ip_addr_type ip, const in_addr &addr);
		string formatIpAddr (const ip_addr_type ip);

		void setSocketReadTimeout (const socket_type sock, const int timeoutIn /*sec*/)	throw (socket_error);
		void setSocketWriteTimeout (const socket_type sock, const int timeoutIn /*sec*/)	throw (socket_error);

		bool checkSocketState (const socket_type sock, const int timeout /*sec*/, bool checkWrite = false)	throw (socket_error);
		



	}
}

#endif // ACONNECT_COMMON_H

