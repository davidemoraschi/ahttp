#ifndef AHTTP_SERVER_CONSTANTS_H
#define AHTTP_SERVER_CONSTANTS_H

#include "aconnect/types.hpp"

namespace ReturnCodes
{
	const int Success = 0;
	const int InitizalizationFailed = 1;
	const int SettingsLoadFailed = 2;
	const int LoggerSetupFailed = 3;
	const int ServerStartupFailed = 4;
	const int ForceStopped = 10;
}

namespace Settings
{
	const aconnect::string_constant ConfigFileName = "server.config";

	const aconnect::string_constant CommandsList = 
		"ahttpserver commands: \r\n"
		"- to start server run \"ahttpserver start\"\r\n"
		"- to stop server run \"ahttpserver stop\"\r\n"
		"- to get statistics run \"ahttpserver stat\"\r\n";
	const aconnect::string_constant StatisticsFormat = 
		"ahttpserver statistics\r\nprocessed requests count: %d\r\n"
		"worker threads count: %d\r\n"
		"pending threads count: %d\r\n";

	const aconnect::string_constant CommandStat = "stat";
	const aconnect::string_constant CommandStart = "start";
	const aconnect::string_constant CommandRun = "run";
	const aconnect::string_constant CommandStop = "stop";
	const aconnect::string_constant CommandReload = "reload";
	const aconnect::string_constant CommandUnknown = "unknown";
	
	const aconnect::string_constant BreakLine = "----------------------------------------------------------------";
	const aconnect::string_constant EndMark = "\r\n\r\n";

}



#endif // AHTTP_SERVER_CONSTANTS_H

