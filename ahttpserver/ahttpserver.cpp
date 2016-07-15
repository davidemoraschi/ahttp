#include "aconnect/boost_format_safe.hpp"

#include <iostream>
#include <assert.h>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/timer.hpp>


#if defined (WIN32)
#	include <signal.h>
#elif defined (__GNUC__)
#	include <sys/signal.h>
#endif  //__GNUC__

// aconnect
#include "ahttplib.hpp"
#include "aconnect/util.hpp"

#include "constants.hpp"

namespace algo = boost::algorithm;
namespace fs = boost::filesystem;

// globals
namespace Global
{
	fs::path appPath;
	aconnect::string settingsFilePath;
	
	ahttp::HttpServerSettings globalSettings;
	aconnect::FileLogger logger;
	aconnect::Server httpServer;
	aconnect::Server commandServer;
}

// declarations
void init (aconnect::string_constptr relativeAppPath);
void destroy ();
void loadSettings ();
void initHandlers ();
void initLogger ();
void processException (aconnect::string_constptr message, int exitCode);
void processSignal (int sig); 
aconnect::socket_type findRunningServer();
void processCommand (const aconnect::ClientInfo& client);
void sendCommand (const aconnect::socket_type sock, aconnect::string_constref command);
void processServerCommand (aconnect::string command);

// definitions
void init (aconnect::string_constptr relativeAppPath) 
{
	signal (SIGINT, processSignal); 
	signal (SIGTERM, processSignal);
	// run-time errors processing
	signal (SIGSEGV, processSignal);
	signal (SIGFPE, processSignal);
	signal (SIGILL, processSignal);
	signal (SIGABRT, processSignal); 

#ifdef WIN32
	signal (SIGBREAK, processSignal);
#endif

	try {
		Global::appPath = aconnect::util::getAppLocation (relativeAppPath);
		
		fs::path configFilePath = fs::complete (Settings::ConfigFileName, Global::appPath.branch_path());
		Global::settingsFilePath = configFilePath.file_string();
		
	} catch (std::exception &err) {
		processException (err.what(), ReturnCodes::InitizalizationFailed);
	}

    // init socket library
	try {
		aconnect::Initializer::init();
	} catch (std::exception &ex) {
		processException (ex.what(), ReturnCodes::InitizalizationFailed);
	}
}

void destroy () 
{
	try {
		// force stop
		Global::httpServer.stop();

        // unload socket library
		aconnect::Initializer::destroy ();

	} catch (std::exception &ex) {
		std::cerr << ex.what() << std::endl;
		Global::logger.error(ex);
	}
    
	// close logger
    Global::logger.info ( "Server stopped" );
    Global::logger.destroy ();
}


void loadSettings ()
{
	try 
	{
		Global::globalSettings.setAppLocaton ( 
			fs::path(Global::appPath).remove_leaf().directory_string().c_str() );
		
		Global::globalSettings.load ( Global::settingsFilePath.c_str() );
		
	} catch (ahttp::settings_load_error &ex) {
		processException (ex.what(), ReturnCodes::SettingsLoadFailed);
	}

}

void initHandlers ()
{
	try 
	{
		Global::globalSettings.initHandlers();
	
	} catch (ahttp::settings_load_error &ex) {
		processException (ex.what(), ReturnCodes::SettingsLoadFailed);
	}

}

void initLogger () {
	using namespace Global;
	using namespace aconnect;

	try {
		// init logger
		string logFileTemplate = globalSettings.logFileTemplate();
		Global::globalSettings.updateAppLocationInPath (logFileTemplate);

		fs::path logFilesDir = fs::path (logFileTemplate, fs::native).branch_path();
		if (!fs::exists (logFilesDir))
			fs::create_directories(logFilesDir);


		logger.init (globalSettings.logLevel(), logFileTemplate.c_str(), globalSettings.maxLogFileSize());
		logger.info ( "Server started" );

	} catch (std::exception &ex) {
		processException (ex.what(), ReturnCodes::LoggerSetupFailed);
	}
	

}

void processException (aconnect::string_constptr message, int exitCode) 
{
	std::cerr << message << std::endl;
	Global::logger.error ("Exception occurred: %s", message);
	exit (exitCode);
}


void processSignal (int sig) {
	
	Global::logger.error ("Server retrieved signal: %d",sig);
	
	destroy ();
	exit (ReturnCodes::ForceStopped);
}

aconnect::socket_type findRunningServer() 
{
	using namespace aconnect;
	socket_type clientSock = util::createSocket (AF_INET);
	
	struct sockaddr_in local;
	util::zeroMemory (&local, sizeof( local ));

	local.sin_family = AF_INET;
	local.sin_port = htons ( Global::globalSettings.commandPort() );
	local.sin_addr.s_addr = inet_addr ( "127.0.0.1" );

	int connectRes = connect (clientSock, (struct sockaddr*) &local, sizeof( local ) );
	if ( connectRes != 0 )
		clientSock = INVALID_SOCKET;
	
	return clientSock;
}



void processCommand (const aconnect::ClientInfo& client)
{
	using namespace aconnect;
	string command = Settings::CommandUnknown, response;
	bool stopServer = false;
	int retCode = ReturnCodes::Success;

	try
	{
		string endMark = Settings::EndMark;

		EndMarkSocketStateCheck check (endMark);
		command = client.getRequest (check);
		algo::erase_tail (command, (int) endMark.length());
		
		if (util::equals (command, Settings::CommandStop)) {
			response = "Stopped";
			stopServer = true;
			
		} else if (util::equals (command, Settings::CommandStat)) {
			const int buffSize = 1024;
			char_type buff[buffSize];

			int formattedCount = snprintf (buff, buffSize, 
				Settings::StatisticsFormat,
				(long) ahttp::HttpServer::RequestsCount,
				(long) Global::httpServer.currentWorkersCount(),
				(long) Global::httpServer.currentPendingWorkersCount());
			
			response.append (buff, util::min2(formattedCount, buffSize));
		
		} else if (util::equals (command, Settings::CommandReload)) {

			response = "Directories settings reloaded";
			try 
			{
				Global::httpServer.stop (true);
				Global::globalSettings.load ( Global::settingsFilePath.c_str() );
				Global::httpServer.start();

			} catch (ahttp::settings_load_error &ex) {
				response = string ("Settings reload failed: ") + ex.what();

				stopServer = true;
				retCode = ReturnCodes::SettingsLoadFailed;
			}
			
		} else {
			response = "Unknown command: " + command;
		}

		client.writeResponse(response + endMark);

		if (util::equals (command, Settings::CommandStop))  
		{
			destroy ();
			exit (retCode);
		}
	}
	catch (std::exception &ex)
	{
		Global::logger.error("Command processing failed, command: %s, error: %s",
			command.c_str(), ex.what() );
	}
}

void sendCommand (const aconnect::socket_type sock, aconnect::string_constref command)
{
	using namespace aconnect;
	try
	{
		string endMark = Settings::EndMark;
		util::writeToSocket(sock, command + endMark);

		EndMarkSocketStateCheck check (endMark);
		string response = util::readFromSocket(sock, check);
		
		algo::erase_tail (response, (int) endMark.length());
		std::cout << response << std::endl;
	}
	catch (std::exception &ex)
	{
		std::cout << "Command sending failed: " << ex.what() << std::endl;
	}
}

void processServerCommand (aconnect::string command)
{
	using namespace aconnect;
	// find running server
	socket_type sock = findRunningServer ();

	if (util::equals(command, Settings::CommandStart))
	{
		if (sock != INVALID_SOCKET) {
			std::cerr << "Server already started - 'stat' command will be sent" << std::endl;
			command = Settings::CommandStat;

		} else {
#if defined (WIN32)
			string startString = "start \"\" \"" + Global::appPath.file_string() 
				+ "\"  " + string(Settings::CommandRun);

			int res = system (startString.c_str());
			if (res == -1)
				std::cerr << "Server startup failed, errno: " << errno << std::endl;
#else
			std::cerr << "Wrong execution path - 'start' command cannot be processed" << std::endl;
#endif
			return;
		}
	}

	// write commands list
	std::cout << Settings::CommandsList << std::endl
		<< Settings::BreakLine << std::endl;

	if (sock != INVALID_SOCKET) {
		sendCommand (sock, command);
		util::closeSocket (sock);
	
	} else {
		std::cerr << "Server is not started" << std::endl;
	}
}

//////////////////////////////////////////////////////////////////////////
//		Entry point
//////////////////////////////////////////////////////////////////////////
int main (int argc, char* args[]) 
{
	using namespace aconnect;
	namespace fs = boost::filesystem;

	init (args[0]);
	
	boost::timer loadTimer;
	double settingsLoadTime = 0,
		loggerInitTime = 0,
		handlersInitTime = 0,
		serverStartupTime = 0;
	
	loadSettings ();
	settingsLoadTime = loadTimer.elapsed(); loadTimer.restart();

	string command = Settings::CommandStat;
	if (argc > 1) 
		command = args[1];
	
#if !defined (WIN32)
	if (util::equals (Settings::CommandStart, command))
		command = Settings::CommandRun;
#endif
	
	if (!util::equals (Settings::CommandRun, command)) 
	{
		processServerCommand (command);
		destroy();
		return ReturnCodes::Success;
	}

	// start server
	ScopedGuard<simple_callback>  guard (destroy);

	initLogger ();
	loggerInitTime = loadTimer.elapsed(); loadTimer.restart();

	Global::globalSettings.setLogger ( &Global::logger);
	ahttp::HttpServer::setGlobalSettings ( &Global::globalSettings);

	initHandlers ();
	handlersInitTime = loadTimer.elapsed(); loadTimer.restart();

	// init HTTP server (in child thread)
	Global::httpServer.setLog ( &Global::logger);
	Global::httpServer.init (Global::globalSettings.port(), 
		ahttp::HttpServer::processConnection, 
		Global::globalSettings.serverSettings());
	
	Global::httpServer.setErrorProcessProc (ahttp::HttpServer::processWorkerCreationError);

	// init command server
	ServerSettings cmdServerSettings;
	cmdServerSettings.socketReadTimeout = 
		cmdServerSettings.socketWriteTimeout = Global::globalSettings.commandSocketTimeout();

	Global::commandServer.setLog ( &Global::logger);
	Global::commandServer.init (Global::globalSettings.commandPort(), 
		processCommand,
		cmdServerSettings);

	try {
		util::detachFromConsole ();	

		Global::httpServer.start();
		serverStartupTime = loadTimer.elapsed(); loadTimer.restart();

		// write timing
		boost::format record ("%s: elapsed time - %f sec");
		record % "Settings load" % settingsLoadTime;
		Global::logger.info(record.str().c_str());

		record.clear(); record % "Logger initialization" % loggerInitTime;
		Global::logger.info(record.str().c_str());

		record.clear(); record % "Handlers initialization" % handlersInitTime;
		Global::logger.info(record.str().c_str());

		record.clear(); record % "Server startup" % serverStartupTime;
		Global::logger.info(record.str().c_str());

		// start command retrieving cycle
		Global::commandServer.start ();
		Global::commandServer.join();
		
	} catch (std::exception &ex)  {
		
		Global::logger.error ("Exception caught at server startup (%s): %s", 
			typeid(ex).name(), ex.what());
		
		std::cerr << "Server startup failed: " << typeid(ex).name() <<  ex.what() << std::endl;

		exit (ReturnCodes::ServerStartupFailed);
	} 

	return ReturnCodes::Success;
}

