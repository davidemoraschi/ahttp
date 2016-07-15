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

#ifndef ACONNECT_UTIL_H
#define ACONNECT_UTIL_H

#include <boost/algorithm/string.hpp>
#include <map>

#include "types.hpp"
#include "error.hpp"

namespace aconnect 
{
	// set of utility functions
	namespace util 
	{
		
		// miscellaneous 
		string calculateFileCrc (string_constref fileName, const std::time_t modifTime);
		void zeroMemory (void *p, int size); 
		string getAppLocation (string_constptr relativePath) throw (std::runtime_error);
		
		bool fileExists (string_constref filePath);
		
		unsigned long getCurrentThreadId();
		void detachFromConsole() throw (std::runtime_error);

		template<typename T> T min2 (T n1, T n2) {
			return (n1 < n2 ? n1 : n2);
		};
		template<typename T> T max2 (T n1, T n2) {
			return (n1 > n2 ? n1 : n2);
		};

		// string processing functions
		string decodeUrl (string_constref url);
		string encodeUrlPart (string_constref url);
		
		inline char_type parseHexSymbol (char_type symbol) throw (std::out_of_range) 
		{
			switch (symbol) {
			case 'A': case 'a':
				return 10;
			case 'B': case 'b':
				return 11;
			case 'C': case  'c':
				return 12;
			case 'D': case  'd':
				return 13;
			case 'E': case  'e':
				return 14;
			case 'F': case  'f':
				return 15;
			case '0': case '1':	case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
				return  ( symbol - 48 );
			default:
				throw std::out_of_range ("Invalid hex symbol");
			}

			return 0;
		}

		inline bool isNullOrEmpty (string_constptr str) {	
			return (NULL == str || str[0] == '\0'); 
		}; 
		inline int compare (string_constptr str1, string_constptr str2, bool ignoreCase = true) {
			if (ignoreCase)
				return _stricmp (str1, str2);
			else
				return strcmp (str1, str2);
		};
		
		inline bool equals (string_constptr str1, string_constptr str2, bool ignoreCase = true) {
			return (compare (str1, str2, ignoreCase) == 0);
		};
		inline bool equals (string_constref str1, string_constptr str2, bool ignoreCase = true) {
			return (compare (str1.c_str(), str2, ignoreCase) == 0);
		};
		inline bool equals (string_constref str1, string_constref str2, bool ignoreCase = true) {
			return (compare (str1.c_str(), str2.c_str(), ignoreCase) == 0);
		};
		
		string escapeHtml (string_constref str);

		inline string escapeHtml (string_constptr str) 
		{
			if (isNullOrEmpty(str))
				return "";
			return escapeHtml ( string (str) );
		};
		
		void parseKeyValuePairs (string str, std::map<string, string>& pairs, 
			string_constptr delimiter = ";",
			string_constptr valueTrimSymbols = "\"");

		inline string::size_type findSequence (string_constref input, string_constref seq)
		{
			assert (seq.size() && "Empty sequence to find");

			string::size_type startPos = -1;
			do {
				startPos = input.find (seq[0], startPos + 1);
				if (startPos == string::npos)
					return string::npos;

				string::const_iterator it, seqIt;
				for ( it = input.begin() + startPos + 1, seqIt = seq.begin() + 1
					; it != input.end() && seqIt != seq.end() && *it == *seqIt
					; ++it, ++seqIt);

				if (it == input.end() || seqIt == seq.end()) // found all symbols from sequence beginning
					return startPos;

			} while (startPos != string::npos);
			
			return string::npos;
		}
	}
}


#endif // ACONNECT_UTIL_H

