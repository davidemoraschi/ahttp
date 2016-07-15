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

#include <cerrno>

#include "util.hpp"
#include "time_util.hpp"
#include "network.hpp"
#include "aconnect.hpp"

namespace aconnect 
{
	//////////////////////////////////////////////////////////////////////////
	//
	//		Utility
	class ThreadGuard {
		
	public:
		Server		*server; 

		ThreadGuard (Server	*srv)
			:server (srv) {	assert(server); }
		
		~ThreadGuard () {
			server->removeWorker();
		}
	};

	void WorkerThreadProcAdapter::operator () () {
		
		ThreadGuard guard (client_.server);
	
		try {
			
			do {
				// process request
				proc_ (client_);
				
				guard.server->logDebug("Close socket: %d", client_.socket);
				util::closeSocket (client_.socket);
				
			} while (guard.server->settings().enablePooling 
				&& !guard.server->isStopped()
				&& guard.server->waitRequest (client_));
			
		} catch (std::exception &err) {
			client_.server->logError(err);
		}
	};
	//
	//
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//
	//		ClientInfo

	ClientInfo::ClientInfo()
	{
		reset();
	}

	void ClientInfo::reset() {
		port = 0; 
		socket = INVALID_SOCKET;
		server = NULL;
		util::zeroMemory(ip, sizeof(ip));
	}

	string ClientInfo::getRequest (SocketStateCheck &stateCheck) const {
		return util::readFromSocket (socket, stateCheck);
	}

	void ClientInfo::writeResponse (string_constref response) const {
		util::writeToSocket (socket, response);
	}
	
	//
	//
	//////////////////////////////////////////////////////////////////////////
	

	//////////////////////////////////////////////////////////////////////////
	//
	//		Server
	void Server::run (Server *server) 
	{
		socket_type serverSock = server->socket();
		const int maxWorkersCount = server->settings().workersCount;

		try 
		{
       		struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof (clientAddr);
			
			while (true)
			{
                socket_type clientSock = accept (serverSock, (sockaddr* ) &clientAddr, &clientAddrLen);
				
				if (server->isStopped())
					break;

				try {

					if (clientSock == INVALID_SOCKET)
						throw socket_error (clientSock, "Client connection accepting failed");

					server->logDebug("Socket accepted: %d", clientSock);

					ClientInfo clientInfo;
					clientInfo.socket = clientSock;
					clientInfo.port = clientAddr.sin_port;
					clientInfo.server = server;
					util::readIpAddress (clientInfo.ip, clientAddr.sin_addr);
					
					if (server->settings().enablePooling && 
						server->currentPendingWorkersCount() > 0)  
					{
						boost::mutex::scoped_lock lock (server->pendingMutex());
						
						if (server->currentPendingWorkersCount() > 0) {
							server->requests().push_back (clientInfo);
							server->pendingCondition().notify_one();
							continue;
						}
					}
					
					
					if (server->currentWorkersCount () < maxWorkersCount) {
						runWorkerThread (server, clientInfo);

					} else {
						// wait for worker finish
						boost::mutex::scoped_lock lock (server->finishMutex());
						server->finishCondition().wait (lock);
						runWorkerThread (server, clientInfo);
					}
				
				} catch (socket_error &err) { 

					if (server->isStopped())
						break;

#if defined (__GNUC__)
					if (err.sockerErrorCode() != EAGAIN)
#endif
					{						
						server->logError ("'socket_error' caught at connection accepting: %s", err.what());
					}

				} catch (std::exception &ex)  {

					if (server->isStopped())
						break;

					// thread starting exception, but connection is open:
					//  - server unavailable (boost::thread_resource_error can be caught)
					server->logError ("Exception caught (%s): %s", 
						typeid(ex).name(), ex.what());
					
					if (clientSock != INVALID_SOCKET && 
							server->errorProcessProc())
						server->errorProcessProc()(clientSock);

					try {
						if (clientSock != INVALID_SOCKET)
							util::closeSocket ( clientSock );
					} catch (socket_error &err) {
						server->logError ("Client socket closing failed: %s", err.what());
					}   
				} 
				
			}

		} catch (socket_error &err) {
			server->logError ("socket_error caught in server main thread: %s", err.what());
			// close socket
            server->stop();
		
		} catch (...)  {
			server->logError ("Unknown exception caught in main aconnect server thread procedure" );
			server->stop();
		} 
	}

	// Start server - it will process request in own thread
	void Server::start (bool inCurrentThread) throw (socket_error)
	{
		boost::mutex::scoped_lock lock (stopMutex_);
		assert (port_ != -1);
		assert (workerProc_);

		isStopped_ = false;

		if (!inCurrentThread && NULL != mainThread_)
			throw server_started_error();
	
		socket_ = util::createSocket (settings_.domain);
		logDebug ("aconnect server socket created: %d, port: %d", 
			socket_, port_);
	
		applySettings ();

		// bind server
		struct sockaddr_in local;
		util::zeroMemory (&local, sizeof( local ));

		local.sin_family = settings().domain;
		local.sin_port = htons ( port() );
		local.sin_addr.s_addr = htonl ( INADDR_ANY );

		if ( bind( socket_, (sockaddr*) &local, sizeof(local) ) != 0 )
            throw socket_error (socket_, "Could not bind socket");

        if ( listen( socket_, settings().backlog ) != 0 )
            throw socket_error (socket_, "Listen to socket failed");

		// run thread
		if (inCurrentThread) {
			Server::run (this);
		} else {
			
			mainThread_ = new boost::thread (ThreadProcAdapter<server_thread_proc, Server*>
				(Server::run, this) );
			boost::thread::yield ();
		}
	}

	
	void Server::stop (bool waitAllWorkers)
	{
		boost::mutex::scoped_lock lock (stopMutex_);
		if (isStopped_)
			return;

		isStopped_ = true;
		
		if (waitAllWorkers) 
		{
			while (pendingWorkersCount_ > 0) 
			{
				boost::mutex::scoped_lock lock (pendingMutex());
				pendingCondition().notify_one();
			}
			while (workersCount_ > 0) 
			{
				boost::mutex::scoped_lock lock (finishMutex() );
				finishCondition().wait (lock);
			}
		}

		try {
			if (socket_ != INVALID_SOCKET) {
				util::closeSocket ( socket_ );
				socket_ = INVALID_SOCKET;
			}
		} catch (socket_error &err) {
			logError (err);
		}

		clear ();      
	}

	
	void Server::runWorkerThread (Server *server, const ClientInfo &clientInfo) 
	{
		if (server->isStopped())
			return;

		// process request
		WorkerThreadProcAdapter adapter (server->workerProc(), clientInfo);
		server->addWorker();
		boost::thread worker (adapter);

	}

	
	bool Server::waitRequest (ClientInfo &client) 
	{
		// wait for new request
		boost::mutex::scoped_lock lock (pendingMutex_);

		++pendingWorkersCount_;
		assert (pendingWorkersCount_ <= workersCount_ && "Too many pending workers!");

		bool waitResult = pendingCondition_.timed_wait (lock, 
				aconnect::util::createTimePeriod (settings().workerLifeTime) );

		--pendingWorkersCount_;
		assert (pendingWorkersCount_ >= 0 && "Negative pending workers count!");

		if (waitResult && !requests().empty()) 
		{ 
			client = requests().front();
			requests().pop_front();

			assert (client.socket != INVALID_SOCKET);
		}
		
		return waitResult;
	}
	
	void Server::applySettings () {
		int intValue = 0;

		if (settings_.reuseAddr) {
			intValue = 1;
			if ( setsockopt( socket_, SOL_SOCKET, SO_REUSEADDR, 
				(char *) &intValue, sizeof(intValue) ) != 0 )
				throw socket_error (socket_, "Reuse address option setup failed");
		}

		util::setSocketReadTimeout ( socket_, settings_.socketReadTimeout );
		util::setSocketWriteTimeout ( socket_, settings_.socketWriteTimeout );
	}
	//
	//
	//////////////////////////////////////////////////////////////////////////
}
