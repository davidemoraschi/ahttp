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

#ifndef ACONNECT_H
#define ACONNECT_H

#include <boost/utility.hpp>
#include <boost/thread.hpp>
#include <boost/detail/atomic_count.hpp>
#include <list>


#include "types.hpp"
#include "logger.hpp"
#include "server_settings.hpp"

namespace aconnect 
{
	typedef void (*worker_thread_proc) (const struct ClientInfo&);
	typedef void (*process_error_fun) (const socket_type clientSock);
	typedef void (*server_thread_proc) (class Server *);   

	interface IStopable {
		virtual bool isStopped() = 0;
		virtual ~IStopable () {} ;
	};

	struct ClientInfo
	{
		port_type		port;
		ip_addr_type	ip;
		socket_type		socket;
		class Server	*server;

		// constructor
		ClientInfo();
		
		// methods
		void reset();
  		string getRequest (SocketStateCheck &stateCheck) const;
		void writeResponse (string_constref response) const;
	};
	
	struct WorkerThreadProcAdapter
	{
		WorkerThreadProcAdapter (worker_thread_proc proc, ClientInfo client) : // must be copied
			proc_(proc), client_(client) { }
		
		void operator () ();

	private:
        worker_thread_proc  proc_;   
		ClientInfo			client_;
	};
	

	//////////////////////////////////////////////////////////////////////////
	//
	//		Server class
	
	class Server : public IStopable, private boost::noncopyable
	{
    public:
		Server () : 
			port_ (-1), 
			workerProc_ (NULL),
			errorProcessProc_ (NULL),
			mainThread_(NULL), 
			socket_(INVALID_SOCKET),
			workersCount_ (0),
			pendingWorkersCount_ (0),
            logger_( NULL ),
			isStopped_ (false)
		{ }

		virtual ~Server () { 
			clear ();
		}

		void init (port_type port, worker_thread_proc workerProc, 
			const ServerSettings &settings = ServerSettings())
		{
			port_ = port;
			workerProc_ = workerProc;
			settings_ = settings;
		}

		// Start server - it will process request in own thread
		void start (bool inCurrentThread = false) throw (socket_error);
		void stop (bool waitAllWorkers = false);
		bool waitRequest (ClientInfo &client);

		// server thread function
		void static run (Server *server);

		inline void join ()
        {
			if (mainThread_)
				mainThread_->join();
		}
		

        //////////////////////////////////////////////////////////////////////////////////////////
        //   
        //      Propertie
		virtual bool isStopped ()							{   return isStopped_;      }   

		inline port_type port()	const						{	return port_;		    }
		inline socket_type socket() const					{	return socket_;		    }
		inline const ServerSettings& settings()	const		{	return settings_;	    } 
		inline worker_thread_proc workerProc()const			{	return workerProc_;     } 
		inline process_error_fun errorProcessProc() const	{	return errorProcessProc_;} 
		
		inline boost::mutex&  finishMutex()					{	return finishMutex_;	}
		inline boost::condition&  finishCondition()			{	return finishCondition_;}
		inline boost::mutex&  pendingMutex()				{	return pendingMutex_;	}
		inline boost::condition&  pendingCondition()		{	return pendingCondition_;}

		inline std::list<ClientInfo>& requests()			{	return requests_;		}

        inline void setLog (Logger *log)					{   logger_ = log;          }
        inline Logger* log () const							{   return logger_;         }   
		
		inline void setErrorProcessProc(process_error_fun proc)	{	errorProcessProc_ = proc;} 

        inline void addWorker () {	
			++workersCount_;		
		}

		inline long currentWorkersCount () {	
			return workersCount_;		
		}
		
		inline void removeWorker () {	
			boost::mutex::scoped_lock lock (finishMutex());
			--workersCount_;
            
			assert (workersCount_ >= 0 && "Negative workers count!");
			finishCondition_.notify_one();
		}

		inline long currentPendingWorkersCount () {	
			return pendingWorkersCount_;		
		}
			
		inline void logDebug (string_constptr format, ...)	{	
			if (logger_) {
				FORMAT_VA_MESSAGE (format, formattedMessage);
				logger_->debug (formattedMessage.c_str());
			}
		}
		inline void logWarning (string_constptr format, ...)	{	
			if (logger_) {
				FORMAT_VA_MESSAGE (format, formattedMessage);
				logger_->warn (formattedMessage.c_str());
			}
		}
		inline void logError( string_constptr format, ...)	{	
			if (logger_) {
				FORMAT_VA_MESSAGE (format, formattedMessage);
				logger_->error (formattedMessage.c_str());
			}
		}
		inline void logError (const std::exception &err) {
			if (logger_)
				logger_->error(err);
		}

	protected:
		void applySettings ();
		void static runWorkerThread (Server *server, const ClientInfo &clientInfo);
		
		inline void clear () {
			if (mainThread_) {
				delete mainThread_; 
				mainThread_ = NULL;
			}
		}

	
	// fields
	protected:
		port_type port_;
        worker_thread_proc workerProc_;
		process_error_fun  errorProcessProc_;

		ServerSettings settings_;
        
        boost::thread *mainThread_;
        socket_type socket_;
        
        boost::detail::atomic_count workersCount_;
		boost::detail::atomic_count pendingWorkersCount_;
        Logger     *logger_;   
		
		boost::mutex finishMutex_;
		boost::condition finishCondition_;
		
		boost::mutex pendingMutex_;
		boost::condition pendingCondition_;


		boost::mutex stopMutex_;
		bool isStopped_;
		std::list<ClientInfo> requests_;
	};

	//
	//////////////////////////////////////////////////////////////////////////
}

#endif // ACONNECT_H
