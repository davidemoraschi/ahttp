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

#if defined (WIN32)
#	include <signal.h>
#elif defined (__GNUC__)
#	include <sys/signal.h>
#endif  //__GNUC__

#include <boost/scoped_array.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/crc.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>

#include "util.hpp"
#include "network.hpp"
#include "complex_types.hpp"

namespace fs = boost::filesystem;
namespace algo = boost::algorithm;

namespace 
{
	inline bool isSafeForUrlPart (aconnect::char_type ch) {
		/*
		if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') )
		return true;
		if (ch == '.' || ch == '-' || ch == '_' || ch == '\'')
		return true;
		return false;
		*/

		static const bool validChars[] = { 
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, true,
			false, false, false, false, false, true, true, false, true, true,
			true, true, true, true, true, true, true, true, false, false,
			false, false, false, false, false, true, true, true, true, true,
			true, true, true, true, true, true, true, true, true, true,
			true, true, true, true, true, true, true, true, true, true,
			true, false, false, false, false, true, false, true, true, true,
			true, true, true, true, true, true, true, true, true, true,
			true, true, true, true, true, true, true, true, true, true,
			true, true, true, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false };
			
		return validChars [(unsigned char) ch];
	}
}

namespace aconnect {
	namespace util {
	
	// create socket with selected domain (Address family) and type
	socket_type createSocket (int domain, int type) throw (socket_error)
	{
		socket_type s = socket (domain, type, 0);
		if (s == INVALID_SOCKET)
			throw socket_error (s, "Socket creation");

		return s;
	}

	void closeSocket (socket_type s) throw (socket_error)
	{
		int res = 0;
#ifdef WIN32
		res = closesocket (s);
#else
		res = close (s);
#endif 
		if (0 != res)
			throw socket_error (s, "Socket closing");
	};

	void writeToSocket (socket_type s, string_constref data) throw (socket_error)
	{
		if (!data.size())
			return;
		writeToSocket (s, data.c_str(), (int) data.length());	
	}
	

	void writeToSocket (socket_type s, string_constptr buff, const int buffLen) throw (socket_error)
	{
		if (0 == buffLen)
			return;

		string_constptr curPos = buff;
		int bytesCount = buffLen, 
			written = 0;

		do {
			written = send (s, curPos, bytesCount, 0);
			if (written == SOCKET_ERROR)
				throw socket_error (s, "Writing data to socket");

			bytesCount -= written;
			curPos += written;

		} while (bytesCount > 0);
	};

	string readFromSocket (const socket_type s, 
		SocketStateCheck &stateCheck, 
		bool throwOnConnectionReset,
		const int buffSize) throw (socket_error)
	{
		string data;
		data.reserve (buffSize);

		boost::scoped_array<char_type> buff (new char_type [buffSize]);
		// zeroMemory (buff.get(), buffSize);
		int bytesRead = 0;
		
		stateCheck.prepare (s);

		if (!stateCheck.isDataAvailable (s))
			return data;

		while ( (bytesRead = recv (s, buff.get(), buffSize, 0)) > 0 ) {
			data.append (buff.get(), bytesRead);

			if (stateCheck.readCompleted (s, data))
				break;
		}

		if (bytesRead == SOCKET_ERROR) {
			err_type errCode = socket_error::getSocketError(s);

			if (!throwOnConnectionReset &&
				 (errCode == network::ConnectionAbortCode
				 || errCode == network::ConnectionResetCode)) 
			{
				stateCheck.setConnectionWasClosed (true);
			} else {
				throw socket_error (s, "Reading data from socket");
			}
		}

		return data;
	};

	void setSocketReadTimeout (const socket_type sock, const int timeoutIn /*sec*/)	throw (socket_error)
	{

#if defined (WIN32)
		int timeout = timeoutIn * 1000;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout)) == SOCKET_ERROR) 
			throw socket_error (sock, "Socket option SO_RCVTIMEO setup failed");
#else
		timeval sockTimeout;
		sockTimeout.tv_sec = (long) (timeoutIn);	sockTimeout.tv_usec = 0;

		if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*) &sockTimeout, sizeof(sockTimeout)) == SOCKET_ERROR) 
			throw socket_error (sock, "Socket option SO_RCVTIMEO setup failed");
#endif
	}

	void setSocketWriteTimeout (const socket_type sock, const int timeoutIn /*sec*/)	throw (socket_error)
	{

#if defined (WIN32)
		int timeout = timeoutIn * 1000;
		if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*) &timeout, sizeof(timeout)) == SOCKET_ERROR) 
			throw socket_error (sock, "Socket option SO_SNDTIMEO setup failed");
#else
		timeval sockTimeout;
		sockTimeout.tv_sec = (long) (timeoutIn);	sockTimeout.tv_usec = 0;
		if(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (void*) &sockTimeout, sizeof(sockTimeout)) == SOCKET_ERROR) 
			throw socket_error (sock, "Socket option SO_SNDTIMEO setup failed");
#endif
	}


	bool checkSocketState (const socket_type sock, const int timeout /*sec*/, bool checkWrite)	throw (socket_error)
	{
		fd_set	sockSet;
		timeval sockTimeout;
		sockTimeout.tv_sec = timeout;
		sockTimeout.tv_usec = 0;

		FD_ZERO ( &sockSet);
		FD_SET (sock, &sockSet);

		int selectRes = 0;
		if (checkWrite)
			selectRes = select ( (int) sock + 1, NULL, &sockSet, NULL, &sockTimeout);
		else
			selectRes = select ( (int) sock + 1, &sockSet, NULL, NULL, &sockTimeout);
			
		if (SOCKET_ERROR == selectRes)	
			throw socket_error (sock, "Checking socket state - 'select' failed");

		if ( 0  == selectRes) // timeout expired
			return false;

		return true; // socket connected
	}

	void zeroMemory (void *p, int size)
	{
#ifdef WIN32
		memset(p, 0, (int) size);
#else
		bzero (p, size );
#endif 
	};

	void readIpAddress (ip_addr_type ip, const in_addr &addr) {
#ifdef WIN32
        ip[0] = addr.s_net;
        ip[1] = addr.s_host;
        ip[2] = addr.s_lh;
        ip[3] = addr.s_impno;
#else
        ip[0] = (byte_type) (addr.s_addr >> 24);
		ip[1] = (byte_type) (addr.s_addr >> 16) % 255;
		ip[2] = (byte_type) (addr.s_addr >> 8) % 255;
		ip[3] = (byte_type) addr.s_addr % 255;
        
#endif 
	};

	string formatIpAddr (const ip_addr_type ip) {
		char_type buff[16];
		int cnt = snprintf (buff, 16,
			"%d.%d.%d.%d", 
  			(int) ip[0], (int) ip[1], 
   			(int) ip[2], (int) ip[3]);
		
		return string (buff, cnt);
	}

	string calculateFileCrc (string_constref fileName, const std::time_t modifTime)
	{
		boost::crc_32_type  result;
		result.process_bytes ( fileName.c_str(), fileName.size() );
		result.process_bytes ( &modifTime, sizeof(modifTime) );

		return boost::lexical_cast <string, boost::crc_32_type::value_type> (result.checksum());
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//		Filesystem related
	//
	bool fileExists (string_constref filePath)
	{
		return fs::exists (fs::path (filePath, fs::native));
	}

	string getAppLocation (string_constptr relativePath) throw (std::runtime_error)
	{
		if ( !relativePath || relativePath[0] == '\0' ) 
			throw std::runtime_error ("Empty relative application path");

		fs::path fullPath;
#ifdef WIN32
		const DWORD bufLen = 1024;
		char_type buff[bufLen ];

		DWORD resLen = 0;
		if ( (resLen = ::GetModuleFileNameA( ::GetModuleHandle(NULL), buff, bufLen)) == 0) 
			throw std::runtime_error("Cannot load application startup location");
		
		fullPath.append (buff, buff + resLen);
#else
		fullPath = fs::system_complete (fs::path (relativePath, fs::native) );

#endif
		return fullPath.file_string();
	}

	unsigned long getCurrentThreadId()
	{
		unsigned long id = (unsigned long) -1;
#if defined (BOOST_HAS_WINTHREADS)
		id = GetCurrentThreadId();
#elif defined(BOOST_HAS_PTHREADS)
		id = pthread_self();
#elif defined(BOOST_HAS_MPTASKS)
		id = MPCurrentTaskID(); // not tested!
#endif
		return id;
	}
	
	//////////////////////////////////////////////////////////////////////////
	//
	// string translation
	//
	string decodeUrl (string_constref url) 
	{
		string::size_type pos = url.find ('%'), prevPos = 0;
		str_stream res;
		
		do {
			res << algo::replace_all_copy (url.substr (prevPos, pos - prevPos), "+", " ");

			if (pos == string::npos)
				break;

			char_type ch = (parseHexSymbol (url [pos + 1]) << 4 ) + parseHexSymbol (url [pos + 2]);
			res << ch;
			
			prevPos = pos + 3;
			pos = url.find ('%', prevPos);

		} while ( true );
		
		return res.str();
	}

	// input must be in UTF-8
	string encodeUrlPart (string_constref url)
	{
		str_stream encodedUrl;
		const aconnect::char_type hex[] = "0123456789ABCDEF";
		
		string::const_iterator it = url.begin();
		while (it != url.end()) {
		
			if (isSafeForUrlPart (*it)) {
				encodedUrl << *it;
			} else {
				encodedUrl << '%' << hex[(*it >> 4) & 0xF] << hex[*it & 0xF];
			}

			++it;
		}
				
		return encodedUrl.str();
	}

	string escapeHtml (string_constref str) 
	{
		using namespace boost::algorithm;
		// javasript: String (s_).replace (/&/g, "&amp;").replace (/</g, "&lt;").replace (/>/g, "&gt;");	

		string result = replace_all_copy (str, "&", "&amp;");
		replace_all (result, "<", "&lt;" );
		replace_all (result, ">", "&gt;" );

		return result;
	};

	void parseKeyValuePairs (string str, std::map<string, string>& pairs, 
		string_constptr delimiter, string_constptr valueTrimSymbols)
	{
		pairs.clear();
		
		const size_t delimLength = strlen(delimiter);
		string::size_type pos = str.find (delimiter), valuePos;
		do 
		{
			valuePos = str.find ('=');
			if (valuePos < pos) 
				pairs [str.substr (0, valuePos)] = algo::trim_copy_if (str.substr (valuePos + 1, pos - valuePos - 1), 
					algo::is_any_of(valueTrimSymbols) );
			else
				pairs [str.substr (0, pos)] = "";

			str.erase (0, pos + delimLength );
			algo::trim_left (str);

			pos = str.find (delimiter);
			if (pos == string::npos && !str.empty())
				pos = str.size() - 1;

		} while (pos != string::npos);
	}

	//////////////////////////////////////////////////////////////////////////

	void detachFromConsole() throw (std::runtime_error)
	{
		fflush (stdin);
		fflush (stdout);
		fflush (stderr);

#if defined (WIN32)
		/* Detach from the console */
		if ( !FreeConsole() ) 
			throw application_error ("Failed to detach from console (Win32 error = %ld).", GetLastError());
		
#else
		/* Convert to a daemon */
		pid_t pid = fork();
		if (pid < 0)
			throw application_error ("First fork() function failed");
		else if (pid != 0) /* Parent -- exit and leave child running */
			exit (0);

		/* Detach from controlling terminal */
		setsid();

		/* ... and make sure we don't aquire another */
		signal( SIGHUP, SIG_IGN );
		pid = fork();
		if ( pid < 0 )
			throw application_error ("Second fork() function failed");
		
		if ( pid != 0 )
			exit( 0 );
#endif

		/* Close stdio streams because they now have nowhere to go! */
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
	}

}}
