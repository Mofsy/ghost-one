/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "ghost.h"
#include "util.h"
#include "crc32.h"
#include "sha1.h"
#include "csvparser.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "ghostdbsqlite.h"
#include "ghostdbmysql.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game_base.h"
#include "game.h"
#include "game_admin.h"
#include "gcbiprotocol.h"
#include "bnetprotocol.h"

#include <cstring>
#include <signal.h>
#include <stdlib.h>

#ifdef WIN32
 #include <ws2tcpip.h>		// for WSAIoctl
#endif

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
using namespace boost :: filesystem;

#define __STORMLIB_SELF__
#include <stormlib/StormLib.h>

/*

#include "ghost.h"
#include "util.h"
#include "crc32.h"
#include "sha1.h"
#include "csvparser.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "commandpacket.h"
#include "ghostdb.h"
#include "ghostdbsqlite.h"
#include "ghostdbmysql.h"
#include "bncsutilinterface.h"
#include "warden.h"
#include "bnlsprotocol.h"
#include "bnlsclient.h"
#include "bnetprotocol.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "replay.h"
#include "gameslot.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game_base.h"
#include "game.h"
#include "game_admin.h"
#include "stats.h"
#include "statsdota.h"
#include "sqlite3.h"

*/

#ifdef WIN32
 #include <process.h>
#else
 #include <unistd.h>
#endif

#ifdef WIN32
 #include <windows.h>
 #include <wininet.h>
 #include <winsock.h>
#endif

#include <time.h>

#ifndef WIN32
 #include <sys/time.h>
#endif

#ifdef __APPLE__
 #include <mach/mach_time.h>
#endif

#ifdef WIN32
 LARGE_INTEGER gHighPerfStart;
 LARGE_INTEGER gHighPerfFrequency;
#endif

string gCFGFile;
string gLogFile;
uint32_t gLogMethod;
ofstream *gLog = NULL;
CGHost *gGHost = NULL;

#define WHATISMYIP "www.whatismyip.com"
#define WHATISMYIP2 "whatismyip.oceanus.ro"

string GetExternalIP()
{	
	string ip;
	// Create socket object
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(sock == SOCKET_ERROR)	// bad socket?
	{
//		printf("failed making socket");
		return "";
	}

	sockaddr_in sa;	
	sa.sin_family	= AF_INET;
	sa.sin_port		= htons(80);	// HTTP service uses port 80

	// Time to get the hostname of www.whatismyip.com

	hostent *h = gethostbyname(WHATISMYIP);
	if(!h)
	{
		//		printf("failed getting host");
		return "";
	}

	memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], 4);

	if( connect(sock, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR )
	{
		//		printf("failed connecting");
		return "";
	}

	// This is our packet that we are going to send to the HTTP server.
	// 
	char Packet[] = 
		"GET /automation/n09230945.asp HTTP/1.1\r\n"			// GET request fetches a page
		"Host: " WHATISMYIP "\r\n"		// we MUST use this param as of HTTP/1.1
		"Referrer: \r\n" // Heh... you should remove this.
		"\r\n";							// final \r\n

	int rtn = 0;

	int iOptVal = 3;
	int iOptLen = sizeof(int);
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&iOptVal, iOptLen);

	rtn = send(sock, Packet, sizeof(Packet)-1, 0);
	if(rtn <= 0)
	{
		//		printf("failed sending packet");
		return "";
	}

	char Buffer[16384] = {0};


	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&iOptVal, iOptLen);

	rtn = recv(sock, Buffer, sizeof(Buffer), 0);
	closesocket(sock);
	if(rtn <= 0) 
	{
		//		printf("failed recving packet");
		return "";
	}

	// Did we get a valid reply back from the server?
	if( _strnicmp(Buffer, "HTTP/1.1 200 OK", 15))
	{
		//		printf("Invalid response.");
		return "";
	}

	char *p = strstr(Buffer,"Cache-control: private");

	if (p==NULL)
		return "";
	if (strlen(p)<26)
		return "";
	
	if  (p[26]<0x30 || p[26]>0x39)
		return "";
	
	for (int i=26;p[i]!=0;i++)
	{
		ip+=p[i];
	}

//	ip=Buffer;
//	ip="yahoo";
	return ip;
}

int GetPID()
{
	// This is used to create an unique warden cookie number.
#ifdef WIN32
    return _getpid();
#else
    return getpid();
#endif
}

string GetExternalIP2()
{
	string ip;
	// Create socket object
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(sock == SOCKET_ERROR)	// bad socket?
	{
		//		printf("failed making socket");
		return "";
	}

	sockaddr_in sa;	
	sa.sin_family	= AF_INET;
	sa.sin_port		= htons(80);	// HTTP service uses port 80

	// Time to get the hostname of www.whatismyip.com

	hostent *h = gethostbyname(WHATISMYIP2);
	if(!h)
	{
		//		printf("failed getting host");
		return "";
	}

	memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], 4);

	if( connect(sock, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR )
	{
		//		printf("failed connecting");
		return "";
	}

	// This is our packet that we are going to send to the HTTP server.
	// 
	char Packet[] = 
		"GET /myip.php HTTP/1.1\r\n"			// GET request fetches a page
		"Host: " WHATISMYIP2 "\r\n"		// we MUST use this param as of HTTP/1.1
		"Referrer: \r\n" // Heh... you should remove this.
		"\r\n";							// final \r\n

	int rtn = 0;

	int iOptVal = 3;
	int iOptLen = sizeof(int);
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&iOptVal, iOptLen);

	rtn = send(sock, Packet, sizeof(Packet)-1, 0);
	if(rtn <= 0)
	{
		//		printf("failed sending packet");
		return "";
	}

	char Buffer[16384] = {0};

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&iOptVal, iOptLen);


	rtn = recv(sock, Buffer, sizeof(Buffer), 0);
	closesocket(sock);
	if(rtn <= 0) 
	{
		//		printf("failed recving packet");
		return "";
	}

	// Did we get a valid reply back from the server?
	if( _strnicmp(Buffer, "HTTP/1.1 200 OK", 15))
	{
		//		printf("Invalid response.");
		return "";
	}

//	char *p = strstr(Buffer,"Cache-control: private");
	char *p = strstr(Buffer,"My IP is:");

	if (p==NULL)
		return "";
	if (strlen(p)<11)
		return "";
	

	if  (p[10]<0x30 || p[10]>0x39)
		return "";

	for (int i=10;p[i]!=0 && p[i]!=0x20;i++)
	{
		ip+=p[i];
	}

	//	ip=Buffer;
	//	ip="yahoo";
	return ip;
}

uint32_t GetTime( )
{
	return GetTicks( ) / 1000;
}

uint32_t GetTicks( )
{
#ifdef WIN32
	// don't use GetTickCount anymore because it's not accurate enough (~16ms resolution)
	// don't use QueryPerformanceCounter anymore because it isn't guaranteed to be strictly increasing on some systems and thus requires "smoothing" code
	// use timeGetTime instead, which typically has a high resolution (5ms or more) but we request a lower resolution on startup
	if (gGHost)
	if (gGHost->m_newTimer)
		return timeGetTime( );
	return GetTickCount();
#elif __APPLE__
	uint64_t current = mach_absolute_time( );
	static mach_timebase_info_data_t info = { 0, 0 };
	// get timebase info
	if( info.denom == 0 )
		mach_timebase_info( &info );
	uint64_t elapsednano = current * ( info.numer / info.denom );
	// convert ns to ms
	return elapsednano / 1e6;
#else
	uint32_t ticks;
	struct timespec t;
	clock_gettime( CLOCK_MONOTONIC, &t );
	ticks = t.tv_sec * 1000;
	ticks += t.tv_nsec / 1000000;
	return ticks;
#endif
}

void SignalCatcher2( int s )
{
	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting NOW" );

	if( gGHost )
	{
		if( gGHost->m_Exiting )
			exit( 1 );
		else
			gGHost->m_Exiting = true;
	}
	else
		exit( 1 );
}

void SignalCatcher( int s )
{
	// signal( SIGABRT, SignalCatcher2 );
	signal( SIGINT, SignalCatcher2 );

	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting nicely" );

	if( gGHost )
		gGHost->m_ExitingNice = true;
	else
		exit( 1 );
}

// initialize ghost
#ifdef WIN32
typedef int _stdcall _UDPInit(void);
typedef _UDPInit* UDPInit;
typedef int _stdcall _UDPClose(void);
typedef _UDPClose* UDPClose;
typedef int _stdcall _UDPSend(PCHAR s);
typedef _UDPSend* UDPSend;
typedef PCHAR _stdcall _UDPWhoIs(PCHAR c, PCHAR s);
typedef _UDPWhoIs* UDPWhoIs;

UDPInit myudpinit;
UDPClose myudpclose;
UDPSend myudpsend;
UDPWhoIs myudpwhois;

HMODULE m_UDP;					// the chat server
#endif
void CONSOLE_Print( string message )
{
	if (gGHost)
	if (!gGHost->m_Console)
		return;
	string::size_type loc = message.find( "]", 0 );
/*
	if (loc<15)
		message.insert(1,15-loc,' ');
	else
*/
//#ifdef WIN32
	if (loc<34)
		message.insert(1,34-loc,' ');
	else
	if (loc<45)
		message.insert(1,45-loc,' ');
//#endif

	if (gGHost!=NULL)
	{
		if (!gGHost->m_inconsole)
		if (gGHost->m_UDPConsole)
		{
			char  *msg;
			msg = new char[message.length() + 1];
			strncpy(msg,message.c_str(), message.length());
			msg[message.length()]=0;
			gGHost->UDPChatSend(msg);
			string hname = gGHost->m_VirtualHostName;
			if (hname.substr(0,2)=="|c")
				hname = hname.substr(10,hname.length()-10);

			if (message.find("]") == string :: npos)
				gGHost->UDPChatSend("|Chate "+UTIL_ToString(hname.length())+" " +hname+" "+message);
		}
		else
			cout << message << endl;
	}
	else
		cout << message << endl;

	// logging

	if (gGHost)
		if (!gGHost->m_Log)
			return;

	if( !gLogFile.empty( ) )
	{
		if( gLogMethod == 1 )
		{
			ofstream Log;
			Log.open( gLogFile.c_str( ), ios :: app );

			if( !Log.fail( ) )
			{
				time_t Now = time( NULL );
				string Time = asctime( localtime( &Now ) );

				// erase the newline

				Time.erase( Time.size( ) - 1 );
				Log << "[" << Time << "] " << message << endl;
				Log.close( );
			}
		}
		else if( gLogMethod == 2 )
		{
			if( gLog && !gLog->fail( ) )
			{
				time_t Now = time( NULL );
				string Time = asctime( localtime( &Now ) );

				// erase the newline

				Time.erase( Time.size( ) - 1 );
				*gLog << "[" << Time << "] " << message << endl;
				gLog->flush( );
			}
		}
	}
}

void DEBUG_Print( string message )
{
	cout << message << endl;
}

void DEBUG_Print( BYTEARRAY b )
{
	cout << "{ ";

	for( unsigned int i = 0; i < b.size( ); i++ )
		cout << hex << (int)b[i] << " ";

	cout << "}" << endl;
}

//
// main
//

int main( int argc, char **argv )
{
#ifdef WIN32
	string ProcessID = UTIL_ToString(GetCurrentProcessId( ));
	string ProcessFile = "data\\process.ini";
#else
	string ProcessID = UTIL_ToString(getpid( ));
	string ProcessFile = "data/process.ini";
#endif
	ofstream myfile;
	myfile.open (ProcessFile.c_str());
	myfile << "[GHOST]" << endl;
	myfile << "ProcessID = " << ProcessID << endl;
	myfile << "ProcessStatus = Online";
	myfile.close();
	CONSOLE_Print(ProcessID);
	gCFGFile = "ghost.cfg";

	if( argc > 1 && argv[1] )
		gCFGFile = argv[1];

	// read config file

	CConfig CFG;
	CFG.Read( gCFGFile );
	gLogFile = CFG.GetString( "bot_log", string( ) );
	gLogMethod = CFG.GetInt( "bot_logmethod", 1 );

	if( !gLogFile.empty( ) )
	{
		if( gLogMethod == 1 )
		{
			// log method 1: open, append, and close the log for every message
			// this works well on Linux but poorly on Windows, particularly as the log file grows in size
			// the log file can be edited/moved/deleted while GHost++ is running
		}
		else if( gLogMethod == 2 )
		{
			// log method 2: open the log on startup, flush the log for every message, close the log on shutdown
			// the log file CANNOT be edited/moved/deleted while GHost++ is running

			gLog = new ofstream( );
			gLog->open( gLogFile.c_str( ), ios :: app );
		}
	}

	CONSOLE_Print( "[GHOST] starting up" );

	if( !gLogFile.empty( ) )
	{
		if( gLogMethod == 1 )
			CONSOLE_Print( "[GHOST] using log method 1, logging is enabled and [" + gLogFile + "] will not be locked" );
		else if( gLogMethod == 2 )
		{
			if( gLog->fail( ) )
				CONSOLE_Print( "[GHOST] using log method 2 but unable to open [" + gLogFile + "] for appending, logging is disabled" );
			else
				CONSOLE_Print( "[GHOST] using log method 2, logging is enabled and [" + gLogFile + "] is now locked" );
		}
	}
	else
		CONSOLE_Print( "[GHOST] no log file specified, logging is disabled" );

	// catch SIGABRT and SIGINT

	// signal( SIGABRT, SignalCatcher );
	signal( SIGINT, SignalCatcher );

#ifndef WIN32
	// disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

	signal( SIGPIPE, SIG_IGN );
#endif

unsigned int TimerResolution = 0;

#ifdef WIN32
	// initialize timer resolution
	// attempt to set the resolution as low as possible from 1ms to 5ms

	for( unsigned int i = 1; i <= 5; i++ )
	{
		if( timeBeginPeriod( i ) == TIMERR_NOERROR )
		{
			TimerResolution = i;
			break;
		}
		else if( i < 5 )
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution to " + UTIL_ToString( i ) + " milliseconds, trying a higher resolution" );
		else
		{
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution" );
			return 1;
		}
	}

	CONSOLE_Print( "[GHOST] using Windows timer with resolution " + UTIL_ToString( TimerResolution ) + " milliseconds" );
#elif __APPLE__
	// not sure how to get the resolution
#else
	// print the timer resolution

	struct timespec Resolution;

	if( clock_getres( CLOCK_MONOTONIC, &Resolution ) == -1 )
		CONSOLE_Print( "[GHOST] error getting monotonic timer resolution" );
	else
	{
		CONSOLE_Print( "[GHOST] using monotonic timer with resolution " + UTIL_ToString( (double)( Resolution.tv_nsec / 1000 ), 2 ) + " microseconds" );
		TimerResolution = ( Resolution.tv_nsec / 1000000 );
	}
#endif

#ifdef WIN32
	// initialize winsock

	CONSOLE_Print( "[GHOST] starting winsock" );
	WSADATA wsadata;

	if( WSAStartup( MAKEWORD( 2, 2 ), &wsadata ) != 0 )
	{
		CONSOLE_Print( "[GHOST] error starting winsock" );
		return 1;
	}

	// increase process priority

	CONSOLE_Print( "[GHOST] setting process priority to \"above normal\"" );
	SetPriorityClass( GetCurrentProcess( ), ABOVE_NORMAL_PRIORITY_CLASS );
#endif

	// initialize ghost

	gGHost = new CGHost( &CFG );

	while( 1 )
	{
		// block for 50ms on all sockets - if you intend to perform any timed actions more frequently you should change this
		// that said it's likely we'll loop more often than this due to there being data waiting on one of the sockets but there aren't any guarantees

		if( gGHost->Update( 50000 ) )
			break;
	}

	// shutdown ghost

	gGHost->UDPChatSend("|shutdown");

	remove(ProcessFile.c_str());
/*
	myfile.open (ProcessFile.c_str());
	myfile << "[GHOST]" << endl;
	myfile << "ProcessID = 0" << endl;
	myfile << "ProcessStatus = Offline";
	myfile.close();
*/

	CONSOLE_Print( "[GHOST] shutting down" );

	TimerResolution = gGHost->m_newTimerResolution;
	bool TimerStarted = gGHost->m_newTimerStarted;
	delete gGHost;
	gGHost = NULL;

#ifdef WIN32
	// shutdown winsock

	CONSOLE_Print( "[GHOST] shutting down winsock" );
	WSACleanup( );

	// shutdown timer

if (TimerStarted)
	timeEndPeriod( TimerResolution );
#endif

	if( gLog )
	{
		if( !gLog->fail( ) )
			gLog->close( );

		delete gLog;
	}

	return 0;
}

vector<string> m_UsersinChannel;
string m_ChannelName;

vector<string> Channel_Users()
{
	return m_UsersinChannel;
}

void Channel_Clear(string name)
{
	m_UsersinChannel.clear();
	m_ChannelName = name;
	gGHost->m_NewChannel = true;
	gGHost->m_ChannelJoinTime = GetTime();
}

void Channel_Add(string name)
{
	for( vector<string> :: iterator i = m_UsersinChannel.begin( ); i != m_UsersinChannel.end( ); i++ )
	{
		if( *i == name )
		{
			return;
		}
	}
	m_UsersinChannel.push_back( name );
	gGHost->m_ChannelJoinTime = GetTime();
}

void Channel_Join(string server, string name)
{
	for( vector<string> :: iterator i = m_UsersinChannel.begin( ); i != m_UsersinChannel.end( ); i++ )
	{
		if( *i == name )
		{
			return;
		}
	}
	m_UsersinChannel.push_back( name );
	gGHost->UDPChatSend("|channeljoin "+name);

	bool Ban = false;
	if (gGHost->m_KickUnknownFromChannel)
	{
		for( vector<CBNET *> :: iterator i = gGHost->m_BNETs.begin( ); i != gGHost->m_BNETs.end( ); i++ )
		{
			if ((*i)->GetServer()==server)
				if (!(*i)->IsSafe(name) && !(*i)->IsAdmin(name) && !(*i)->IsRootAdmin(name) )
					(*i)->QueueChatCommand("/kick "+name);
		}
	}
	if (gGHost->m_KickBannedFromChannel)
	{
		for( vector<CBNET *> :: iterator i = gGHost->m_BNETs.begin( ); i != gGHost->m_BNETs.end( ); i++ )
		{
			if ((*i)->GetServer()==server)
				if ((*i)->IsBannedName(name))
					(*i)->QueueChatCommand("/kick "+name);
		}
	}
	if (gGHost->m_BanBannedFromChannel)
	{
		for( vector<CBNET *> :: iterator i = gGHost->m_BNETs.begin( ); i != gGHost->m_BNETs.end( ); i++ )
		{
			if ((*i)->GetServer()==server)
				if ((*i)->IsBannedName(name))
					(*i)->QueueChatCommand("/ban "+name);
		}
	}
}

void Channel_Del(string name)
{
	for( vector<string> :: iterator i = m_UsersinChannel.begin( ); i != m_UsersinChannel.end( ); i++ )
	{
		if( *i == name )
		{
			m_UsersinChannel.erase(i);
			gGHost->UDPChatSend("|channelleave "+name);
			return;
		}
	}
}

bool Patch21 ( )
{
	return gGHost->m_patch21;
}

string FixPath(string Path, string End)
{
	if (Path!="")
	if (Path.substr(Path.length()-1,1)!=End)
		Path += End;
	return Path;
}


bool CMDCheck (uint32_t cmd, uint32_t acc)
{
	uint32_t Mask = 1;
	if (cmd != 0)
		for (unsigned int k=1;k<=cmd;k++)
			Mask = Mask * 2;
	if (acc & Mask)
		return true;
	else
		return false;
}

uint32_t CMDAccessAll ()
{
	uint32_t Result = 1;
	uint32_t Mask = 1;
	for (unsigned int k=1;k<=gGHost->m_Commands.size()-1;k++)
	{
		Mask = Mask*2;
		Result += Mask;
	}
	return Result;
}

//
// CGHost
//

CGHost :: CGHost( CConfig *CFG )
{
	m_Console = true;
	m_Log = true;
	m_UDPConsole=false;
#ifdef WIN32
	LPCWSTR udpl=L"udpserver.dll";
	m_UDP = LoadLibraryW(udpl);

	myudpinit=(UDPInit)::GetProcAddress(m_UDP,"UDPInit");
	myudpclose=(UDPClose)::GetProcAddress(m_UDP,"UDPClose");
	myudpsend=(UDPSend)::GetProcAddress(m_UDP,"UDPSend");
	myudpwhois=(UDPWhoIs)::GetProcAddress(m_UDP,"UDPWhoIs");

	myudpinit();
#endif
	m_UDPSocket = new CUDPSocket( );
	m_UDPSocket->SetBroadcastTarget( CFG->GetString( "udp_broadcasttarget", string( ) ) );
	m_UDPSocket->SetDontRoute( CFG->GetInt( "udp_dontroute", 0 ) == 0 ? false : true );
	m_ReconnectSocket = NULL;
	m_GPSProtocol = new CGPSProtocol( );
	m_GCBIProtocol = new CGCBIProtocol( );
	m_UDPConsole = CFG->GetInt( "bot_udpconsole", 1 ) == 0 ? false : true;
	m_CRC = new CCRC32( );
	m_CRC->Initialize( );
	m_SHA = new CSHA1( );
	m_CurrentGame = NULL;
	string DBType = CFG->GetString( "db_type", "sqlite3" );
	CONSOLE_Print( "[GHOST] opening primary database" );
	
	if( DBType == "mysql" )
	{
#ifdef GHOST_MYSQL
		m_DB = new CGHostDBMySQL( CFG );
#else
		CONSOLE_Print( "[GHOST] warning - this binary was not compiled with MySQL database support, using SQLite database instead" );
		m_DB = new CGHostDBSQLite( CFG );
#endif
	}
	else
		m_DB = new CGHostDBSQLite( CFG );

	CONSOLE_Print( "[GHOST] opening secondary (local) database" );
	m_DBLocal = new CGHostDBSQLite( CFG );

	// get a list of local IP addresses
	// this list is used elsewhere to determine if a player connecting to the bot is local or not

	CONSOLE_Print( "[GHOST] attempting to find local IP addresses" );

#ifdef WIN32
	// use a more reliable Windows specific method since the portable method doesn't always work properly on Windows
	// code stolen from: http://tangentsoft.net/wskfaq/examples/getifaces.html

	SOCKET sd = WSASocket( AF_INET, SOCK_DGRAM, 0, 0, 0, 0 );

	if( sd == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to create socket (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
	else
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;

		if( WSAIoctl( sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList, sizeof(InterfaceList), &nBytesReturned, 0, 0 ) == SOCKET_ERROR )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - WSAIoctl failed (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
		else
		{
			int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

			for( int i = 0; i < nNumInterfaces; i++ )
			{
				sockaddr_in *pAddress;
				pAddress = (sockaddr_in *)&(InterfaceList[i].iiAddress);
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( pAddress->sin_addr ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)pAddress->sin_addr.s_addr, false ) );
			}
		}

		closesocket( sd );
	}
#else
	// use a portable method

	char HostName[255];

	if( gethostname( HostName, 255 ) == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to get local hostname" );
	else
	{
		CONSOLE_Print( "[GHOST] local hostname is [" + string( HostName ) + "]" );
		struct hostent *HostEnt = gethostbyname( HostName );

		if( !HostEnt )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - gethostbyname failed" );
		else
		{
			for( int i = 0; HostEnt->h_addr_list[i] != NULL; i++ )
			{
				struct in_addr Address;
				memcpy( &Address, HostEnt->h_addr_list[i], sizeof(struct in_addr) );
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( Address ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)Address.s_addr, false ) );
			}
		}
	}
#endif

	m_UDPCommandSocket = new CUDPServer();
	m_UDPCommandSocket->Bind(CFG->GetString( "udp_cmdbindip", "0.0.0.0" ), CFG->GetInt( "udp_cmdport", 6969 ));
	m_UDPCommandSpoofTarget = CFG->GetString( "udp_cmdspooftarget", string( ) );
#ifdef WIN32
	sPathEnd = "\\";
#else
	sPathEnd = "/";
#endif
	m_Language = NULL;
	m_Exiting = false;
	m_ExitingNice = false;
	m_Enabled = true;
	m_GHostVersion = "17.0 One";
	m_Version = "("+m_GHostVersion+")";
	stringstream SS;
	string istr = string();
	m_DisableReason = string();
	m_RootAdmin = "One";
	m_CookieOffset = GetPID() * 10;
	m_CallableDownloadFile = NULL;
	UTIL_ExtractStrings(CMD_string, m_Commands);
	m_HostCounter = 1;
	m_AutoHostServer = string();
	m_AutoHostOwner = string();
	m_AutoHostGameName = string();
	m_AutoHostLocal = false;
	m_AutoHostGArena = false;
	m_AutoHostCountries = string();
	m_AutoHostCountries2 = string();
	m_AutoHostCountryCheck = false;
	m_AutoHostCountryCheck2 = false;
	m_AutoHostMaximumGames = 0;
	m_AutoHostAutoStartPlayers = 0;
	m_LastAutoHostTime = 0;
	m_AutoHostMatchMaking = false;
	m_AutoHostMinimumScore = 0.0;
	m_AutoHostMaximumScore = 0.0;
	m_AllGamesFinished = false;
	m_AllGamesFinishedTime = 0;
	m_TFT = CFG->GetInt( "bot_tft", 1 ) == 0 ? false : true;

	if( m_TFT )
		CONSOLE_Print( "[GHOST] acting as Warcraft III: The Frozen Throne" );
	else
		CONSOLE_Print( "[GHOST] acting as Warcraft III: Reign of Chaos" );

	m_Reconnect = CFG->GetInt( "bot_reconnect", 1 ) == 0 ? false : true;
	m_ReconnectPort = CFG->GetInt( "bot_reconnectport", 6114 );
	m_ScoresCount = 0;
	m_ScoresCountSet = false;
	m_AutoHosted = false;
	m_CalculatingScores = false;
	m_QuietRehost = false;
	m_Rehosted = false;
	m_Hosted = false;
	m_LastGameName = string();

	ReloadConfig();
	// load the battle.net connections
	// we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)

	for( uint32_t i = 1; i < 10; i++ )
	{
		string Prefix;

		if( i == 1 )
			Prefix = "bnet_";
		else
			Prefix = "bnet" + UTIL_ToString( i ) + "_";

		string Server = CFG->GetString( Prefix + "server", string( ) );
		string ServerAlias = CFG->GetString( Prefix + "serveralias", string( ) );
		string CDKeyROC = CFG->GetString( Prefix + "cdkeyroc", string( ) );
		string CDKeyTFT = CFG->GetString( Prefix + "cdkeytft", string( ) );
		string CountryAbbrev = CFG->GetString( Prefix + "countryabbrev", "USA" );
		string Country = CFG->GetString( Prefix + "country", "United States" );
		string Locale = CFG->GetString( Prefix + "locale", "system" );
		uint32_t LocaleID;

		if( Locale == "system" )
		{
#ifdef WIN32
			LocaleID = GetUserDefaultLangID( );
#else
			LocaleID = 1033;
#endif
		}
		else
			LocaleID = UTIL_ToUInt32( Locale );

		string UserName = CFG->GetString( Prefix + "username", string( ) );
		string UserPassword = CFG->GetString( Prefix + "password", string( ) );
		string FirstChannel = CFG->GetString( Prefix + "firstchannel", "The Void" );
		string RootAdmin = CFG->GetString( Prefix + "rootadmin", string( ) );
		if (!RootAdmin.empty())
			m_RootAdmin = RootAdmin;
		string BNETCommandTrigger = CFG->GetString( Prefix + "commandtrigger", "!" );

		if( BNETCommandTrigger.empty( ) )
			BNETCommandTrigger = "!";

		bool Whereis = false;
		uint32_t tmp = CFG->GetInt( Prefix + "whereis", 2 );
		Whereis = (tmp == 0 ? false : true);
		if (Server == "server.eurobattle.net")
			if (tmp == 2)
				Whereis = true;
			else;
		else if (tmp ==2) 
			Whereis = false;
		bool HoldFriends = CFG->GetInt( Prefix + "holdfriends", 1 ) == 0 ? false : true;
		bool HoldClan = CFG->GetInt( Prefix + "holdclan", 1 ) == 0 ? false : true;
		bool PublicCommands = CFG->GetInt( Prefix + "publiccommands", 1 ) == 0 ? false : true;
		string BNLSServer = CFG->GetString( Prefix + "bnlsserver", string( ) );
		int BNLSPort = CFG->GetInt( Prefix + "bnlsport", 9367 );
		int BNLSWardenCookie = i + m_CookieOffset;
		unsigned char War3Version = CFG->GetInt( Prefix + "custom_war3version", 24 );
		BYTEARRAY EXEVersion = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversion", string( ) ), 4 );
		BYTEARRAY EXEVersionHash = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversionhash", string( ) ), 4 );
		string PasswordHashType = CFG->GetString( Prefix + "custom_passwordhashtype", string( ) );
		string PVPGNRealmName = CFG->GetString( Prefix + "custom_pvpgnrealmname", "PvPGN Realm" );
		uint32_t MaxMessageLength = CFG->GetInt( Prefix + "custom_maxmessagelength", 200 );

		if( Server.empty( ) )
			break;

		if( CDKeyROC.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeyroc, skipping this battle.net connection" );
			continue;
		}

		if( m_TFT && CDKeyTFT.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeytft, skipping this battle.net connection" );
			continue;
		}

		if( UserName.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "username, skipping this battle.net connection" );
			continue;
		}

		if( UserPassword.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "password, skipping this battle.net connection" );
			continue;
		}

		CONSOLE_Print( "[GHOST] found battle.net connection #" + UTIL_ToString( i ) + " for server [" + Server + "]" );

		if( Locale == "system" )
		{
#ifdef WIN32
			CONSOLE_Print( "[GHOST] using system locale of " + UTIL_ToString( LocaleID ) );
#else
			CONSOLE_Print( "[GHOST] unable to get system locale, using default locale of 1033" );
#endif
		}

		m_BNETs.push_back( new CBNET( this, Server, ServerAlias, BNLSServer, (uint16_t)BNLSPort, (uint32_t)BNLSWardenCookie, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, LocaleID, UserName, UserPassword, FirstChannel, RootAdmin, BNETCommandTrigger[0], HoldFriends, HoldClan, PublicCommands, War3Version, EXEVersion, EXEVersionHash, PasswordHashType, PVPGNRealmName, MaxMessageLength, i ) );

		m_BNETs[m_BNETs.size()-1]->SetWhereis(Whereis);
		if (m_AutoHostServer.length()==0)
			m_AutoHostServer = Server;
	}

	if( m_BNETs.empty( ) )
		CONSOLE_Print( "[GHOST] warning - no battle.net connections found in config file" );

	// extract common.j and blizzard.j from War3Patch.mpq if we can
	// these two files are necessary for calculating "map_crc" when loading maps so we make sure to do it before loading the default map
	// see CMap :: Load for more information

	ExtractScripts( );

	// load the default maps (note: make sure to run ExtractScripts first)

	if( m_DefaultMap.size( ) < 4 || m_DefaultMap.substr( m_DefaultMap.size( ) - 4 ) != ".cfg" )
	{
		m_DefaultMap += ".cfg";
		CONSOLE_Print( "[GHOST] adding \".cfg\" to default map -> new default is [" + m_DefaultMap + "]" );
	}

	CConfig MapCFG;
	MapCFG.Read( m_MapCFGPath + m_DefaultMap );
	m_Map = new CMap( this, &MapCFG, m_MapCFGPath + m_DefaultMap );

	if( !m_AdminGameMap.empty( ) )
	{
		if( m_AdminGameMap.size( ) < 4 || m_AdminGameMap.substr( m_AdminGameMap.size( ) - 4 ) != ".cfg" )
		{
			m_AdminGameMap += ".cfg";
			CONSOLE_Print( "[GHOST] adding \".cfg\" to default admin game map -> new default is [" + m_AdminGameMap + "]" );
		}

		CONSOLE_Print( "[GHOST] trying to load default admin game map" );
		CConfig AdminMapCFG;
		AdminMapCFG.Read( m_MapCFGPath + m_AdminGameMap );
		m_AdminMap = new CMap( this, &AdminMapCFG, m_MapCFGPath + m_AdminGameMap );

		if( !m_AdminMap->GetValid( ) )
		{
			CONSOLE_Print( "[GHOST] default admin game map isn't valid, using hardcoded admin game map instead" );
			delete m_AdminMap;
			m_AdminMap = new CMap( this );
		}
	}
	else
	{
		CONSOLE_Print( "[GHOST] using hardcoded admin game map" );
		m_AdminMap = new CMap( this );
	}
	
	for( int i = 0; i < 100; i++)
	{
		string AutoHostMapCFGString = CFG->GetString( "autohost_map" + UTIL_ToString( i ) , string( ) );
		
		if( AutoHostMapCFGString.empty( ) )
		{
			continue;
		}
		
    	if( AutoHostMapCFGString.size( ) < 4 || AutoHostMapCFGString.substr( AutoHostMapCFGString.size( ) - 4 ) != ".cfg" )
		{
			AutoHostMapCFGString += ".cfg";
			CONSOLE_Print( "[GHOST] adding \".cfg\" to autohost game map -> new name is [" + AutoHostMapCFGString + "]" );
			CONSOLE_Print( "[GHOST] hosting [" + AutoHostMapCFGString + "]" );
		}
		
		CConfig AutoHostMapCFG;
		AutoHostMapCFG.Read( m_MapCFGPath + AutoHostMapCFGString );
		CMap *AutoHostMap = new CMap( this, &AutoHostMapCFG, m_MapCFGPath + AutoHostMapCFGString );
		m_AutoHostMap.push_back( new CMap( *AutoHostMap ) );
	}
	
	m_AutoHostMapCounter = 0;
	
	m_SaveGame = new CSaveGame( );

	// load the iptocountry data

	LoadIPToCountryDataOpt();

	// external ip and country

//	m_ExternalIP="";
	if (m_ExternalIP!="")
	{
		m_ExternalIPL=ntohl(inet_addr(m_ExternalIP.c_str()));
		m_Country=m_DBLocal->FromCheck(m_ExternalIPL);
		CONSOLE_Print( "[GHOST] External IP is " + m_ExternalIP);
		CONSOLE_Print( "[GHOST] Country is " + m_Country);
	}
	if (m_FindExternalIP)
	{
		if (!m_AltFindIP)
			CONSOLE_Print( "[GHOST] Finding External IP ");
		else
			CONSOLE_Print( "[GHOST] Finding External IP, alternate method ");
		if (!m_AltFindIP)
			m_ExternalIP=GetExternalIP();
		else
			m_ExternalIP=GetExternalIP2();

		if (m_ExternalIP=="" || m_ExternalIP.length()<3)
			if (!m_AltFindIP)
				m_ExternalIP=GetExternalIP2();
			else
				m_ExternalIP=GetExternalIP();
		if (m_ExternalIP!="")	
		{
			m_ExternalIPL=ntohl(inet_addr(m_ExternalIP.c_str()));
			m_Country=m_DBLocal->FromCheck(m_ExternalIPL);
			CONSOLE_Print( "[GHOST] External IP is " + m_ExternalIP);
			CONSOLE_Print( "[GHOST] Country is " + m_Country);
	//		CONSOLE_Print( "[GHOST] Provider is " + UDPChatWhoIs(m_Country, m_ExternalIP));
		} else
		UDPChatSend("|ip");
	} else 
		UDPChatSend("|ip");
	// create the admin game

	if( m_AdminGameCreate )
	{
		CONSOLE_Print( "[GHOST] creating admin game" );
		m_AdminGame = new CAdminGame( this, m_AdminMap, NULL, m_AdminGamePort, 0, "GHost++ Admin Game", m_AdminGamePassword );

		if( m_AdminGamePort == m_HostPort )
			CONSOLE_Print( "[GHOST] warning - admingame_port and bot_hostport are set to the same value, you won't be able to host any games" );
	}
	else
		m_AdminGame = NULL;

	if( m_BNETs.empty( ) && !m_AdminGame )
		CONSOLE_Print( "[GHOST] warning - no battle.net connections found and no admin game created" );

#ifdef GHOST_MYSQL
	CONSOLE_Print( "[GHOST] GHost++ Version " + m_Version + " (with MySQL support)" );
#else
	CONSOLE_Print( "[GHOST] GHost++ Version " + m_Version + " (without MySQL support)" );
#endif
}

CGHost :: ~CGHost( )
{
#ifdef WIN32
	myudpclose();
#endif
	m_UDPConsole = false;
	delete m_UDPCommandSocket;
	delete m_UDPSocket;
	delete m_ReconnectSocket;

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); i++ )
		delete *i;

	delete m_GPSProtocol;
	delete m_GCBIProtocol;
	delete m_CRC;
	delete m_SHA;

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		delete *i;

	delete m_CurrentGame;
	delete m_AdminGame;

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
		delete *i;

	delete m_DB;
	delete m_DBLocal;

	// warning: we don't delete any entries of m_Callables here because we can't be guaranteed that the associated threads have terminated
	// this is fine if the program is currently exiting because the OS will clean up after us
	// but if you try to recreate the CGHost object within a single session you will probably leak resources!

	if( !m_Callables.empty( ) )
		CONSOLE_Print( "[GHOST] warning - " + UTIL_ToString( m_Callables.size( ) ) + " orphaned callables were leaked (this is not an error)" );

	delete m_Language;
	delete m_Map;
	delete m_AdminMap;
	ClearAutoHostMap( );
	delete m_SaveGame;
}

bool CGHost :: Update( unsigned long usecBlock )
{
	// todotodo: do we really want to shutdown if there's a database error? is there any way to recover from this?

	if( m_DB->HasError( ) )
	{
		CONSOLE_Print( "[GHOST] database error - " + m_DB->GetError( ) );
		return true;
	}

	if( m_DBLocal->HasError( ) )
	{
		CONSOLE_Print( "[GHOST] local database error - " + m_DBLocal->GetError( ) );
		return true;
	}

	// try to exit nicely if requested to do so

	if( m_ExitingNice )
	{
		if( !m_BNETs.empty( ) )
		{
			CONSOLE_Print( "[GHOST] deleting all battle.net connections in preparation for exiting nicely" );

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
				delete *i;

			m_BNETs.clear( );
		}

		if( m_CurrentGame )
		{
			CONSOLE_Print( "[GHOST] deleting current game in preparation for exiting nicely" );
			delete m_CurrentGame;
			m_CurrentGame = NULL;
		}

		if( m_AdminGame )
		{
			CONSOLE_Print( "[GHOST] deleting admin game in preparation for exiting nicely" );
			delete m_AdminGame;
			m_AdminGame = NULL;
		}

		if( m_Games.empty( ) )
		{
			if( !m_AllGamesFinished )
			{
				CONSOLE_Print( "[GHOST] all games finished, waiting 60 seconds for threads to finish" );
				CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads in progress" );
				m_AllGamesFinished = true;
				m_AllGamesFinishedTime = GetTime( );
			}
			else
			{
				if( m_Callables.empty( ) )
				{
					CONSOLE_Print( "[GHOST] all threads finished, exiting nicely" );
					m_Exiting = true;
				}
				else if( GetTime( ) - m_AllGamesFinishedTime >= 60 )
				{
					CONSOLE_Print( "[GHOST] waited 60 seconds for threads to finish, exiting anyway" );
					CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads still in progress which will be terminated" );
					m_Exiting = true;
				}
			}
		}
	}

	// update callables

	for( vector<CBaseCallable *> :: iterator i = m_Callables.begin( ); i != m_Callables.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_DB->RecoverCallable( *i );
			delete *i;
			i = m_Callables.erase( i );
		}
		else
			i++;
	}

	// create the GProxy++ reconnect listener

	if( m_Reconnect )
	{
		if( !m_ReconnectSocket )
		{
			m_ReconnectSocket = new CTCPServer( );

			if( m_ReconnectSocket->Listen( m_BindAddress, m_ReconnectPort ) )
				CONSOLE_Print( "[GHOST] listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort ) );
			else
			{
				CONSOLE_Print( "[GHOST] error listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort ) );
				delete m_ReconnectSocket;
				m_ReconnectSocket = NULL;

				m_Reconnect = false;
			}
		}
		else if( m_ReconnectSocket->HasError( ) )
		{
			CONSOLE_Print( "[GHOST] GProxy++ reconnect listener error (" + m_ReconnectSocket->GetErrorString( ) + ")" );
				delete m_ReconnectSocket;
				m_ReconnectSocket = NULL;

				m_Reconnect = false;
			}
	}

	unsigned int NumFDs = 0;

	// take every socket we own and throw it in one giant select statement so we can block on all sockets

	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	// 1. all battle.net sockets

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 2. the current game's server and player sockets

	if( m_CurrentGame )
		NumFDs += m_CurrentGame->SetFD( &fd, &send_fd, &nfds );

	// 3. the admin game's server and player sockets

	if( m_AdminGame )
		NumFDs += m_AdminGame->SetFD( &fd, &send_fd, &nfds );

	// 4. all running games' player sockets

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 5. the GProxy++ reconnect socket(s)

	if( m_Reconnect && m_ReconnectSocket )
	{
		m_ReconnectSocket->SetFD( &fd, &send_fd, &nfds );
		NumFDs++;
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); i++ )
	{
		(*i)->SetFD( &fd, &send_fd, &nfds );
		NumFDs++;
	}

	// 6. udp command receiving socket

	m_UDPCommandSocket->SetFD( &fd,  &send_fd, &nfds);
	// SetFD of the UDPServer does not return the number of sockets belonging to it as it's obviously one
	NumFDs++;

	// before we call select we need to determine how long to block for
	// previously we just blocked for a maximum of the passed usecBlock microseconds
	// however, in an effort to make game updates happen closer to the desired latency setting we now use a dynamic block interval
	// note: we still use the passed usecBlock as a hard maximum

	if (m_newLatency)
	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
	{
		if( (*i)->GetNextTimedActionTicks( ) * 1000 < usecBlock )
			usecBlock = (*i)->GetNextTimedActionTicks( ) * 1000;
	}

	// always block for at least 1ms just in case something goes wrong
	// this prevents the bot from sucking up all the available CPU if a game keeps asking for immediate updates
	// it's a bit ridiculous to include this check since, in theory, the bot is programmed well enough to never make this mistake
	// however, considering who programmed it, it's worthwhile to do it anyway

	if (m_newLatency)
	if( usecBlock < 1000 )
		usecBlock = 1000;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, NULL, NULL, &tv );
	select( 1, NULL, &send_fd, NULL, &send_tv );
#else
	select( nfds + 1, &fd, NULL, NULL, &tv );
	select( nfds + 1, NULL, &send_fd, NULL, &send_tv );
#endif

	if( NumFDs == 0 )
	{
		// we don't have any sockets (i.e. we aren't connected to battle.net maybe due to a lost connection and there aren't any games running)
		// select will return immediately and we'll chew up the CPU if we let it loop so just sleep for 50ms to kill some time

		MILLISLEEP( 50 );
	}

	bool AdminExit = false;
	bool BNETExit = false;

	// update current game

	if( m_CurrentGame )
	{
		if( m_CurrentGame->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting current game [" + m_CurrentGame->GetGameName( ) + "]" );

#ifdef WIN32
			if (m_wtv && m_CurrentGame->wtvprocessid != 0)
			{
				HANDLE hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, m_CurrentGame->wtvprocessid );

				if( m_CurrentGame->wtvprocessid != GetCurrentProcessId( ) && m_CurrentGame->wtvprocessid!= 0 )
				{
					CONSOLE_Print( "[WaaaghTV] shutting down wtvrecorder.exe" );

					if( !TerminateProcess(hProcess, 0) )
						CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (1-1)" );
					else
						CloseHandle( hProcess );
					m_CurrentGame->wtvprocessid = 0;
				}
				else
					CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (1-2)" );
			}
#endif
			UDPChatSend("|unhost");
			delete m_CurrentGame;
			m_CurrentGame = NULL;

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				(*i)->QueueGameUncreate( );
				(*i)->QueueEnterChat( );
			}

			if ( newGame ) 
			{
				for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
				{
					if ((*i)->GetServer()==newGameServer)
					{
						{
							CreateGame( m_Map, newGameGameState, false, newGameName, newGameUser, newGameUser, newGameServer, true );							
							if (m_CurrentGame)
							{
								m_CurrentGame->m_GarenaOnly = newGameGArena;
								m_CurrentGame->m_Countries = newGameCountries;
								m_CurrentGame->m_Countries2 = newGameCountries2;
								m_CurrentGame->m_CountryCheck = newGameCountryCheck;
								m_CurrentGame->m_CountryCheck2 = newGameCountry2Check;
								m_CurrentGame->m_Providers = newGameProviders;
								m_CurrentGame->m_Providers2 = newGameProviders2;
								m_CurrentGame->m_ProviderCheck = newGameProvidersCheck;
								m_CurrentGame->m_ProviderCheck2 = newGameProviders2Check;
								newGame = false;
							}
						}							
					}
				}

			}
		}
		else if( m_CurrentGame )
			m_CurrentGame->UpdatePost( &send_fd );
	}

	// update admin game

	if( m_AdminGame )
	{
		if( m_AdminGame->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting admin game" );
			delete m_AdminGame;
			m_AdminGame = NULL;
			AdminExit = true;
		}
		else if( m_AdminGame )
			m_AdminGame->UpdatePost( &send_fd );
	}

	// update running games
	uint32_t GameNr = 0;
	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); )
	{
		if( (*i)->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting game [" + (*i)->GetGameName( ) + "]" );
			UDPChatSend("|endgame "+UTIL_ToString(GameNr)+" "+(*i)->GetGameName( ));

#ifdef WIN32
			if (m_wtv && (*i)->wtvprocessid != 0)
			{
				HANDLE hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, (*i)->wtvprocessid );

				if( (*i)->wtvprocessid != GetCurrentProcessId( ) && (*i)->wtvprocessid != 0 )
				{
					CONSOLE_Print( "[WaaaghTV] shutting down wtvrecorder.exe" );

					if( !TerminateProcess( hProcess, 0 ) )
						CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (2-1)" );
					else
						CloseHandle( hProcess );
					(*i)->wtvprocessid = 0;
				}
				else
					CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (2-2)" );
			}
#endif

/*
			if (m_HoldPlayersForRMK)
			{
				for( vector<CGamePlayer *> :: iterator j = (*i)->m_Players.begin( ); j != (*i)->m_Players.end( ); j++ )
				{
					if (m_PlayersfromRMK.length()==0)
						m_PlayersfromRMK +=" ";
					m_PlayersfromRMK +=(*j)->GetName();
				}
				CONSOLE_Print( "[GHOST] reserving players from [" + (*i)->GetGameName( ) + "]" );
			}
*/
			for( vector<CBNET *> :: iterator l = m_BNETs.begin( ); l != m_BNETs.end( ); l++ )
			{
				(*l)->m_LastGameCountRefreshTime = 0;
			}

			EventGameDeleted( *i );
			delete *i;
			i = m_Games.erase( i );
		}
		else
		{
			(*i)->UpdatePost( &send_fd );
			i++;
		}
		GameNr++;
	}

	if (m_Rehosted)
	{
		m_Rehosted = false;
		UDPChatSend("|rehost "+m_RehostedName);
	}

	if (m_Hosted)
	{
		m_Hosted = false;
		UDPChatSend("|host "+m_HostedName);
	}

	// update mysql queue
	if (GetTicks() - m_MySQLQueueTicks > 1000)
	{
		bool done = false;
		m_MySQLQueueTicks = GetTicks();
		if (m_WarnForgetQueue.size()>0)
		{
			string wfname = m_WarnForgetQueue[0];
			m_WarnForgetQueue.erase(m_WarnForgetQueue.begin());
			m_Callables.push_back( m_DB->ThreadedWarnForget( wfname, m_GameNumToForgetAWarn ));
			done = true;			
		}
	}

	// update dynamic latency printout

	if (GetTicks() - m_LastDynamicLatencyConsole > 36000)
	{
		m_LastDynamicLatencyConsole = GetTicks();
	}

	// update GProxy++ reliable reconnect sockets

	if( m_Reconnect && m_ReconnectSocket )
	{
		CTCPSocket *NewSocket = m_ReconnectSocket->Accept( &fd );

		if( NewSocket )
			m_ReconnectSockets.push_back( NewSocket );
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); )
	{
		if( (*i)->HasError( ) || !(*i)->GetConnected( ) || GetTime( ) - (*i)->GetLastRecv( ) >= 10 )
		{
			delete *i;
			i = m_ReconnectSockets.erase( i );
			continue;
		}

		(*i)->DoRecv( &fd );
		string *RecvBuffer = (*i)->GetBytes( );
		BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

		// a packet is at least 4 bytes

		if( Bytes.size( ) >= 4 )
		{
			if( Bytes[0] == GPS_HEADER_CONSTANT )
			{
				// bytes 2 and 3 contain the length of the packet

				uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

				if( Length >= 4 )
				{
					if( Bytes.size( ) >= Length )
					{
						if( Bytes[1] == CGPSProtocol :: GPS_RECONNECT && Length == 13 )
						{
							unsigned char PID = Bytes[4];
							uint32_t ReconnectKey = UTIL_ByteArrayToUInt32( Bytes, false, 5 );
							uint32_t LastPacket = UTIL_ByteArrayToUInt32( Bytes, false, 9 );

							// look for a matching player in a running game

							CGamePlayer *Match = NULL;

							for( vector<CBaseGame *> :: iterator j = m_Games.begin( ); j != m_Games.end( ); j++ )
							{
								if( (*j)->GetGameLoaded( ) )
								{
									CGamePlayer *Player = (*j)->GetPlayerFromPID( PID );

									if( Player && Player->GetGProxy( ) && Player->GetGProxyReconnectKey( ) == ReconnectKey )
									{
										Match = Player;
										break;
									}
								}
							}

							if( Match )
							{
								// reconnect successful!

								*RecvBuffer = RecvBuffer->substr( Length );
								Match->EventGProxyReconnect( *i, LastPacket );
								i = m_ReconnectSockets.erase( i );
								continue;
							}
							else
							{
								(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_NOTFOUND ) );
								(*i)->DoSend( &send_fd );
								delete *i;
								i = m_ReconnectSockets.erase( i );
								continue;
							}
						}
						else
						{
							(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
							(*i)->DoSend( &send_fd );
							delete *i;
							i = m_ReconnectSockets.erase( i );
							continue;
						}
					}
				}
				else
				{
					(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
					(*i)->DoSend( &send_fd );
					delete *i;
					i = m_ReconnectSockets.erase( i );
					continue;
				}
			}
			else
			{
				(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
				(*i)->DoSend( &send_fd );
				delete *i;
				i = m_ReconnectSockets.erase( i );
				continue;
			}
		}

		(*i)->DoSend( &send_fd );
		i++;
	}


	// update battle.net connections

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( (*i)->Update( &fd, &send_fd ) )
			BNETExit = true;
	}

	// UDP COMMANDSOCKET CODE
	sockaddr_in recvAddr;
	string udpcommand;
	m_UDPCommandSocket->RecvFrom( &fd, &recvAddr, &udpcommand);
//	UDPChatAdd(recvAddr.sin_addr);
	if ( udpcommand.size() )
	{
		// default server to relay the message to
		string udptarget = m_UDPCommandSpoofTarget;
		int pos;
		bool relayed = false;

		// special case, a udp command starting with || . ex: ||readwelcome
		if (udpcommand.substr(0,2)=="||")
		{
			UDPCommands(udpcommand.substr(1,udpcommand.length()-1));
		} 
		else
		{
			// has the user specified a specific target the command should be sent to?
			// looks for "<someaddress>" at the beginning of the received command,
			// sets the target accordingly and strips it from the command
			if ( udpcommand.find("<") == 0 && (pos=udpcommand.find(">")) != string::npos )
			{
				udptarget = udpcommand.substr(1, pos - 1);
				udpcommand.erase(0, pos + 1);
			}
			// we expect commands not to start with the command trigger because this is a commandsocket,
			// we only except commands and therefore know we received one and not some chatting
			// this way the user sending the command does not have to have knowledge of the commandtrigger
			// set in GHost's config file
			udpcommand = string(1, m_CommandTrigger) + udpcommand;

			// loop through all connections to find the server the command should be issued on
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				// is this the right one or should we just send it to the first in list?
				if ( udptarget == (*i)->GetServer( ) || udptarget.empty() )
				{
					CONSOLE_Print("[UDPCMDSOCK] Relaying cmd [" + udpcommand + "] to server [" + (*i)->GetServer( ) + "]");
					// spoof a whisper from the rootadmin belonging to this connection
					CIncomingChatEvent *chatCommand = new CIncomingChatEvent( CBNETProtocol::EID_WHISPER, 0, (*i)->GetRootAdmin( ), udpcommand );
					(*i)->ProcessChatEvent( chatCommand );
					relayed = true;
					break;
				}
			}
			if (!relayed)
				CONSOLE_Print("[UDPCMDSOCK] Could not relay cmd [" + udpcommand + "] to server [" + udptarget + "]: server unknown");
		}
	}

	if (m_NewChannel)
		if (GetTime() - m_ChannelJoinTime>1)
		{
			UDPChatSend("|channeljoined "+m_ChannelName);
			m_NewChannel=false;
		}
		//dbg
		if (false)
		if (m_Games.size()<7 && m_BNETs[0]->GetLoggedIn())
		{
			CreateGame( m_Map, GAME_PUBLIC, false, "test"+UTIL_ToString(m_Games.size()+1), m_AutoHostOwner, m_AutoHostOwner, m_AutoHostServer, false );
			m_CurrentGame->m_StartedLoadingTicks = GetTicks( );
			m_CurrentGame->m_LastLoadInGameResetTime = GetTime( );
			m_CurrentGame->m_StartedLoadingTime = GetTime( );
			m_CurrentGame->m_GameLoading = true;
			m_CurrentGame->CreateFakePlayer();
			m_CurrentGame->EventGameStarted( );
		}

	// autohost

	if( !m_AutoHostGameName.empty( ) && m_AutoHostMaximumGames != 0 && m_AutoHostAutoStartPlayers != 0 && GetTime( ) - m_LastAutoHostTime >= 15 )
	{
		// copy all the checks from CGHost :: CreateGame here because we don't want to spam the chat when there's an error
		// instead we fail silently and try again soon

		if( !m_ExitingNice && m_Enabled && !m_CurrentGame && m_Games.size( ) < m_MaxGames && m_Games.size( ) < m_AutoHostMaximumGames && !m_AutoHostMap.empty( ) )
		{
			if( m_AutoHostMapCounter >= m_AutoHostMap.size( ) )
				m_AutoHostMapCounter = 0;
			
			CMap *AutoHostMap = m_AutoHostMap[m_AutoHostMapCounter];
			m_AutoHostMapCounter = ( m_AutoHostMapCounter + 1 ) % m_AutoHostMap.size( );
			
			if( AutoHostMap->GetValid( ) )
			{
				string GameName = m_AutoHostGameName + " #" + UTIL_ToString( m_HostCounter );

				if( GameName.size( ) <= 31 )
				{
					m_AutoHosted = true;
					CreateGame( AutoHostMap, GAME_PUBLIC, false, GameName, m_AutoHostOwner, m_AutoHostOwner, m_AutoHostServer, false );
					m_AutoHosted = false;

					if( m_CurrentGame )
					{
						m_CurrentGame->SetAutoStartPlayers( m_AutoHostAutoStartPlayers );

						if( m_AutoHostMatchMaking )
						{
							if( !m_Map->GetMapMatchMakingCategory( ).empty( ) )
							{
								if( m_Map->GetMapGameType( ) != GAMETYPE_CUSTOM )
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found but matchmaking can only be used with custom maps, matchmaking disabled" );
								else
								{
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found, matchmaking enabled" );

									m_CurrentGame->SetMatchMaking( true );
									m_CurrentGame->SetMinimumScore( m_AutoHostMinimumScore );
									m_CurrentGame->SetMaximumScore( m_AutoHostMaximumScore );
								}
							}
							else
								CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory not found, matchmaking disabled" );
						}
					}
				}
				else
				{					
					CONSOLE_Print( "[GHOST] stopped auto hosting, next game name [" + GameName + "] is too long (the maximum is 31 characters)" );
					m_AutoHostGameName.clear( );
					m_AutoHostOwner.clear( );
					m_AutoHostServer.clear( );
					m_AutoHostMaximumGames = 0;
					m_AutoHostAutoStartPlayers = 0;
					m_AutoHostMatchMaking = false;
					m_AutoHostMinimumScore = 0.0;
					m_AutoHostMaximumScore = 0.0;
				}
			}
			else
			{
				CONSOLE_Print( "[GHOST] stopped auto hosting, map config file [" + AutoHostMap->GetCFGFile( ) + "] is invalid" );
				m_AutoHostGameName.clear( );
				m_AutoHostOwner.clear( );
				m_AutoHostServer.clear( );
				m_AutoHostMaximumGames = 0;
				m_AutoHostAutoStartPlayers = 0;
				m_AutoHostMatchMaking = false;
				m_AutoHostMinimumScore = 0.0;
				m_AutoHostMaximumScore = 0.0;
			}
		}

		m_LastAutoHostTime = GetTime( );
	}

	return m_Exiting || AdminExit || BNETExit;
}

void CGHost :: EventBNETConnecting( CBNET *bnet )
{
	if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->ConnectingToBNET( bnet->GetServer( ) ) );

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectingToBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETConnected( CBNET *bnet )
{
	if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->ConnectedToBNET( bnet->GetServer( ) ) );

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectedToBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETDisconnected( CBNET *bnet )
{
	if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->DisconnectedFromBNET( bnet->GetServer( ) ) );

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->DisconnectedFromBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETLoggedIn( CBNET *bnet )
{
	if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->LoggedInToBNET( bnet->GetServer( ) ) );

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->LoggedInToBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETGameRefreshed( CBNET *bnet )
{
	if(m_CurrentGame)
	{
		m_LastGameName = m_CurrentGame->GetGameName();
		if(m_CurrentGame->m_Rehost)
		{
			m_Rehosted = true;
			m_RehostedName = m_CurrentGame->GetGameName();
			m_RehostedServer = m_CurrentGame->GetCreatorServer();
			m_CurrentGame->m_Rehost = false;
			CONSOLE_Print( "[GAME: " + m_CurrentGame->GetGameName() + "] rehost worked");
			m_CurrentGame->m_LastPlayerJoinedTime = GetTime( );
			string s = string();
			string st = string ();
			
			if(m_RehostPrintingDelay + 3 > m_ActualRehostPrintingDelay)
			{			
				s = m_CurrentGame->GetGameName();
				s = s.substr(s.size() - 2, 2 );
				st = st + " " + s ;
				m_ActualRehostPrintingDelay++;
			}
			else
			{
				m_ActualRehostPrintingDelay = 0;
				st = "Rehosted as \""+ st +"\"";
				m_CurrentGame->SendAllChat(st);			
			}			
			UDPChatSend("|rehostw "+m_CurrentGame->GetGameName());
		} else
		{
			m_Hosted = true;
			m_HostedName = m_CurrentGame->GetGameName();
		}
	} 
// commented as it spams too much for now
	//	if( m_AdminGame )
		//m_AdminGame->SendAllChat( m_Language->BNETGameHostingSucceeded( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETGameRefreshFailed( CBNET *bnet )
{
	if( m_CurrentGame )
	{
		m_LastGameName = m_CurrentGame->GetGameName();
		if (!m_QuietRehost)
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

			if( (*i)->GetServer( ) == m_CurrentGame->GetCreatorServer( ) )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ), m_CurrentGame->GetCreatorName( ), true );
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->BNETGameHostingFailed( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

		if (!m_RehostIfNameTaken)
		{
			m_CurrentGame->SendAllChat( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

			// we take the easy route and simply close the lobby if a refresh fails
			// it's possible at least one refresh succeeded and therefore the game is still joinable on at least one battle.net (plus on the local network) but we don't keep track of that
			// we only close the game if it has no players since we support game rehosting (via !priv and !pub in the lobby)

			if( m_CurrentGame->GetNumHumanPlayers( ) == 0 )
				m_CurrentGame->SetExiting( true );
		}

		// if game name is taken, rehost with game name + 1
		if (m_RehostIfNameTaken)
		{
			m_CurrentGame->SetGameName(IncGameNr(m_CurrentGame->GetGameName()));
			m_CurrentGame->AddGameName(m_CurrentGame->GetGameName());
			m_HostCounter++;
			SaveHostCounter();
			if (m_MaxHostCounter>0)
			if (m_HostCounter>m_MaxHostCounter)
				m_HostCounter = 1;
			m_CurrentGame->SetHostCounter(m_HostCounter);
			m_QuietRehost = true;
			m_CurrentGame->SetRefreshError(false);
			m_CurrentGame->SetRehost(true);
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				(*i)->QueueGameUncreate( );
				(*i)->QueueEnterChat( );

				// the game creation message will be sent on the next refresh
			}
			m_CurrentGame->SetCreationTime(GetTime( ));
			m_CurrentGame->SetLastRefreshTime(GetTime( ));
			m_CurrentGame->m_LastPlayerJoinedTime = GetTime( )+5;
			return;
		}

		m_CurrentGame->SetRefreshError( true );
	}
}

void CGHost :: EventBNETConnectTimedOut( CBNET *bnet )
{
	if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->ConnectingToBNETTimedOut( bnet->GetServer( ) ) );

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectingToBNETTimedOut( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETWhisper( CBNET *bnet, string user, string message )
{
	if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
			(*i)->SendLocalAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}
}

void CGHost :: EventBNETChat( CBNET *bnet, string user, string message )
{
	if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
			(*i)->SendLocalAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}
}

void CGHost :: EventBNETEmote( CBNET *bnet, string user, string message )
{
	if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
			(*i)->SendLocalAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}
}

void CGHost :: EventGameDeleted( CBaseGame *game )
{
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ) );

		if( (*i)->GetServer( ) == game->GetCreatorServer( ) )
			(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ), game->GetCreatorName( ), true );
	}
}
void CGHost :: ClearAutoHostMap( )
{
	for( vector<CMap *> :: iterator i = m_AutoHostMap.begin( ); i != m_AutoHostMap.end( ); ++i )
		delete *i;
	
	m_AutoHostMap.clear( );
}

void CGHost :: ReloadConfigs( )
{
	CConfig CFG;
	CFG.Read( gCFGFile );
	SetConfigs( &CFG );
}

void CGHost :: SetConfigs( CConfig *CFG )
{
	// this doesn't set EVERY config value since that would potentially require reconfiguring the battle.net connections
	// it just set the easily reloadable values

	m_LanguageFile = CFG->GetString( "bot_language", "language.cfg" );
	delete m_Language;
	m_Language = new CLanguage( m_LanguageFile );
	m_Warcraft3Path = UTIL_AddPathSeperator( CFG->GetString( "bot_war3path", "C:\\Program Files\\Warcraft III\\" ) );
	m_BindAddress = CFG->GetString( "bot_bindaddress", string( ) );
	m_ReconnectWaitTime = CFG->GetInt( "bot_reconnectwaittime", 3 );
	m_MaxGames = CFG->GetInt( "bot_maxgames", 60 );
	string BotCommandTrigger = CFG->GetString( "bot_commandtrigger", "!" );

	if( BotCommandTrigger.empty( ) )
		BotCommandTrigger = "!";

	m_CommandTrigger = BotCommandTrigger[0];
	m_MapCFGPath = UTIL_AddPathSeperator( CFG->GetString( "bot_mapcfgpath", string( ) ) );
	m_SaveGamePath = UTIL_AddPathSeperator( CFG->GetString( "bot_savegamepath", string( ) ) );
	m_MapPath = UTIL_AddPathSeperator( CFG->GetString( "bot_mappath", string( ) ) );
	m_SaveReplays = CFG->GetInt( "bot_savereplays", 0 ) == 0 ? false : true;
	m_ReplayPath = UTIL_AddPathSeperator( CFG->GetString( "bot_replaypath", string( ) ) );
	m_VirtualHostName = CFG->GetString( "bot_virtualhostname", "|cFF4080C0GHost" );
	m_HideIPAddresses = CFG->GetInt( "bot_hideipaddresses", 0 ) == 0 ? false : true;
	m_CheckMultipleIPUsage = CFG->GetInt( "bot_checkmultipleipusage", 1 ) == 0 ? false : true;

	if( m_VirtualHostName.size( ) > 15 )
	{
		m_VirtualHostName = "|cFF4080C0GHost";
		CONSOLE_Print( "[GHOST] warning - bot_virtualhostname is longer than 15 characters, using default virtual host name" );
	}

	m_SpoofChecks = CFG->GetInt( "bot_spoofchecks", 2 );
	m_RequireSpoofChecks = CFG->GetInt( "bot_requirespoofchecks", 0 ) == 0 ? false : true;
	m_RefreshMessages = CFG->GetInt( "bot_refreshmessages", 0 ) == 0 ? false : true;
	m_AutoLock = CFG->GetInt( "bot_autolock", 0 ) == 0 ? false : true;
	m_AutoSave = CFG->GetInt( "bot_autosave", 0 ) == 0 ? false : true;
	m_AllowDownloads = CFG->GetInt( "bot_allowdownloads", 0 );
	m_PingDuringDownloads = CFG->GetInt( "bot_pingduringdownloads", 0 ) == 0 ? false : true;
	m_LCPings = CFG->GetInt( "bot_lcpings", 1 ) == 0 ? false : true;
	m_AutoKickPing = CFG->GetInt( "bot_autokickping", 1450 );
	m_BanMethod = CFG->GetInt( "bot_banmethod", 1 );
	m_IPBlackListFile = CFG->GetString( "bot_ipblacklistfile", "ipblacklist.txt" );
	m_LobbyTimeLimit = CFG->GetInt( "bot_lobbytimelimit", 111 );
	m_Latency = CFG->GetInt( "bot_latency", 100 );
	m_SyncLimit = CFG->GetInt( "bot_synclimit", 50 );
	m_VoteKickAllowed = CFG->GetInt( "bot_votekickallowed", 1 ) == 0 ? false : true;
	m_VoteStartAllowed = CFG->GetInt( "bot_votestartallowed", 1 ) == 0 ? false : true;
	m_VoteStartAutohostOnly = CFG->GetInt( "bot_votestartautohostonly", 1 ) == 0 ? false : true;
	m_VoteStartMinPlayers = CFG->GetInt( "bot_votestartplayers", 6 );
	m_VoteKickPercentage = CFG->GetInt( "bot_votekickpercentage", 100 );

	if( m_VoteKickPercentage > 100 )
	{
		m_VoteKickPercentage = 100;
		CONSOLE_Print( "[GHOST] warning - bot_votekickpercentage is greater than 100, using 100 instead" );
	}

	m_MOTDFile = CFG->GetString( "bot_motdfile", "motd.txt" );
	m_GameLoadedFile = CFG->GetString( "bot_gameloadedfile", "gameloaded.txt" );
	m_GameOverFile = CFG->GetString( "bot_gameoverfile", "gameover.txt" );
	m_LocalAdminMessages = CFG->GetInt( "bot_localadminmessages", 0 ) == 0 ? false : true;
	m_TCPNoDelay = CFG->GetInt( "tcp_nodelay", 0 ) == 0 ? false : true;
	m_dropifdesync = CFG->GetInt( "bot_dropifdesync", 1 ) == 0 ? false : true; //Metal_Koola
	m_MatchMakingMethod = CFG->GetInt( "bot_matchmakingmethod", 1 );
	m_PlayerBeforeStartPrintDelay = CFG->GetInt( "bot_playerbeforestartprintdelay", 4 );
	m_RehostPrintingDelay = CFG->GetInt( "bot_rehostprintingdelay", 4 );
}

void CGHost :: ExtractScripts( )
{
	string PatchMPQFileName = m_Warcraft3Path + "War3Patch.mpq";
	HANDLE PatchMPQ;

	if( SFileOpenArchive( PatchMPQFileName.c_str( ), 0, MPQ_OPEN_FORCE_MPQ_V1, &PatchMPQ ) )
	{
		CONSOLE_Print( "[GHOST] loading MPQ file [" + PatchMPQFileName + "]" );
		HANDLE SubFile;

		// common.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\common.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead ) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\common.j from MPQ file to [" + m_MapCFGPath + "common.j]" );
					UTIL_FileWrite( m_MapCFGPath + "common.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\common.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\common.j in MPQ file" );

		// blizzard.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\blizzard.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead ) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\blizzard.j from MPQ file to [" + m_MapCFGPath + "blizzard.j]" );
					UTIL_FileWrite( m_MapCFGPath + "blizzard.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\blizzard.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\blizzard.j in MPQ file" );

		SFileCloseArchive( PatchMPQ );
	}
	else
		CONSOLE_Print( "[GHOST] warning - unable to load MPQ file [" + PatchMPQFileName + "] - error code " + UTIL_ToString( GetLastError( ) ) );
}

void CGHost :: LoadIPToCountryDataOpt( )
{
	bool oldips = true;

	intmax_t file_len;
	file_len = file_size("ip-to-country.csv");

	if (exists("ips.cfg") && exists("ips.dbs"))
	{
		string File = "ips.cfg";
		CConfig CFGH;
		CFGH.Read( File );
		int filelen = CFGH.GetInt( "size", 0 );
		if (filelen==file_len)
			oldips = false;
	}

	if (oldips)
	{
		LoadIPToCountryData( );
		m_DBLocal->RunQuery("ATTACH DATABASE 'ips.dbs' AS ips");
		m_DBLocal->RunQuery("DELETE * FROM ips.iptocountry");
		m_DBLocal->RunQuery("INSERT INTO ips.iptocountry SELECT * FROM iptocountry");
		m_DBLocal->RunQuery("DETACH DATABASE ips");
		if (file_len!=0)
		{
			string File = "ips.cfg";
			ofstream tmpcfg;
			tmpcfg.open( File.c_str( ), ios :: trunc );
			int size = (int)file_len;
			tmpcfg << "size = " << UTIL_ToString(size) << endl;
			tmpcfg.close( );
		}
	}
	else
	{
		CONSOLE_Print( "[GHOST] started loading [ips.dbs]" );
		uint32_t tim = GetTicks();
		m_DBLocal->RunQuery("ATTACH DATABASE 'ips.dbs' AS ips");
		m_DBLocal->RunQuery("INSERT INTO iptocountry SELECT * FROM ips.iptocountry");
		m_DBLocal->RunQuery("DETACH DATABASE ips");		
		CONSOLE_Print("[GHOST] iptocountry loading finished in "+UTIL_ToString(GetTicks()-tim)+" ms");
	}
}

void CGHost :: LoadIPToCountryData( )
{
	ifstream in;
	in.open( "ip-to-country.csv" );

	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [ip-to-country.csv], iptocountry data not loaded" );
	else
	{
		CONSOLE_Print( "[GHOST] started loading [ip-to-country.csv]" );

		// the begin and commit statements are optimizations
		// we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time
		// todotodo: handle begin/commit failures a bit more gracefully

		if( !m_DBLocal->Begin( ) )
			CONSOLE_Print( "[GHOST] warning - failed to begin local database transaction, iptocountry data not loaded" );
		else
		{
			unsigned char Percent = 0;
			string Line;
			string IP1;
			string IP2;
			string Country;
			CSVParser parser;

			// get length of file for the progress meter

			in.seekg( 0, ios :: end );
			uint32_t FileLength = in.tellg( );
			in.seekg( 0, ios :: beg );

			uint32_t tim = GetTicks();

			while( !in.eof( ) )
			{
				getline( in, Line );

				if( Line.empty( ) )
					continue;

				parser << Line;
				parser >> IP1;
				parser >> IP2;
				parser >> Country;
				m_DBLocal->FromAdd( UTIL_ToUInt32( IP1 ), UTIL_ToUInt32( IP2 ), Country );

				// it's probably going to take awhile to load the iptocountry data (~10 seconds on my 3.2 GHz P4 when using SQLite3)
				// so let's print a progress meter just to keep the user from getting worried

				unsigned char NewPercent = (unsigned char)( (float)in.tellg( ) / FileLength * 100 );

				if( NewPercent != Percent && NewPercent % 10 == 0 )
				{
					Percent = NewPercent;
					CONSOLE_Print( "[GHOST] iptocountry data: " + UTIL_ToString( Percent ) + "% loaded" );
				}
			}
	
			CONSOLE_Print("[GHOST] iptocountry loading finished in "+UTIL_ToString(GetTicks()-tim)+" ms");

			if( !m_DBLocal->Commit( ) )
				CONSOLE_Print( "[GHOST] warning - failed to commit local database transaction, iptocountry data not loaded" );
			else
				CONSOLE_Print( "[GHOST] finished loading [ip-to-country.csv]" );
		}

		in.close( );
	}
}

void CGHost :: CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, string creatorName, string creatorServer, bool whisper )
{
	if( !m_Enabled )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameDisabled( gameName.substr(0,10)+"...", m_DisableReason ), creatorName, whisper );
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameDisabled( gameName.substr(0,10)+"...", m_DisableReason ) );

		return;
	}

	if( gameName.size( ) > 31 )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameNameTooLong( gameName ), creatorName, whisper );
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameNameTooLong( gameName ) );

		return;
	}

	if( !map->GetValid( ) )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidMap( gameName ), creatorName, whisper );
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameInvalidMap( gameName ) );

		return;
	}

	if( saveGame )
	{
		if( !m_SaveGame->GetValid( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidSaveGame( gameName ), creatorName, whisper );
			}

			if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->UnableToCreateGameInvalidSaveGame( gameName ) );

			return;
		}

		string MapPath1 = m_SaveGame->GetMapPath( );
		string MapPath2 = map->GetMapPath( );
		transform( MapPath1.begin( ), MapPath1.end( ), MapPath1.begin( ), (int(*)(int))tolower );
		transform( MapPath2.begin( ), MapPath2.end( ), MapPath2.begin( ), (int(*)(int))tolower );

		if( MapPath1 != MapPath2 )
		{
			CONSOLE_Print( "[GHOST] path mismatch, saved game path is [" + MapPath1 + "] but map path is [" + MapPath2 + "]" );

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameSaveGameMapMismatch( gameName ), creatorName, whisper );
			}

			if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->UnableToCreateGameSaveGameMapMismatch( gameName ) );

			return;
		}

		if( m_EnforcePlayers.empty( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameMustEnforceFirst( gameName ), creatorName, whisper );
			}

			if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->UnableToCreateGameMustEnforceFirst( gameName ) );

			return;
		}
	}

	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ), creatorName, whisper );
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ) );

		return;
	}

	if( m_Games.size( ) >= m_MaxGames )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ), creatorName, whisper );
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ) );

		return;
	}

	CONSOLE_Print( "[GHOST] creating game [" + gameName + "]" );
	UDPChatSend("|newgame "+gameName);
	
	m_LastGameName = gameName;

//	if (m_AutoHosted)
//	if( saveGame )
//		m_CurrentGame = new CGame( this, m_AutoHostMap, m_SaveGame, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
//	else
//		m_CurrentGame = new CGame( this, m_AutoHostMap, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
//	else
	unsigned char gs = gameState;
	if (gameState!=GAME_PRIVATE && gameState!=GAME_PUBLIC)
		gameState = GAME_PRIVATE;

	if( saveGame )
		m_CurrentGame = new CGame( this, map, m_SaveGame, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
	else
		m_CurrentGame = new CGame( this, map, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );

	// todotodo: check if listening failed and report the error to the user

	if( m_SaveGame )
	{
		m_CurrentGame->SetEnforcePlayers( m_EnforcePlayers );
		m_EnforcePlayers.clear( );
	}

	m_CurrentGame->m_GameStateS = gs;

	m_CurrentGame->SetAutoHosted(m_AutoHosted);

	// auto set HCL if map_defaulthcl is not empty
	m_CurrentGame->AutoSetHCL();

	// country checks
	if (!m_AutoHosted)
	if (m_AllowedCountries!="")
	{
		m_CurrentGame->m_Countries = m_AllowedCountries;
		m_CurrentGame->m_CountryCheck = true;
	}

	// score checks
	if (!m_AutoHosted)
		if (m_AllowedScores!=0)
		{
			m_CurrentGame->m_Scores = m_AllowedScores;
			m_CurrentGame->m_ScoreCheck = true;
		}
	
	// score checks in autohosted
	if (m_AutoHosted)
	if (m_AutoHostAllowedScores!=0)
	{
		m_CurrentGame->m_Scores = m_AllowedScores;
		m_CurrentGame->m_ScoreCheck = true;
	}

	if (!m_AutoHosted)
	if (m_DeniedCountries!="")
	{
		m_CurrentGame->m_Countries2 = m_DeniedCountries;
		m_CurrentGame->m_CountryCheck2 = true;
	}

	// add players from RMK if any

	if (m_HoldPlayersForRMK && m_PlayersfromRMK.length()>0)
	{
		CONSOLE_Print( "[GHOST] reserving players from last game");

		stringstream SS;
		string s;
		SS << m_PlayersfromRMK;
		while( !SS.eof( ) )
		{
			SS >> s;
			m_CurrentGame->AddToReserved(s, 255);
		}
		m_PlayersfromRMK = string();
	}
	
	bool AutoHostRefresh = m_AutoHostLocal && m_AutoHosted;
	// don't advertise the game if it's autohosted locally
	if (!AutoHostRefresh )
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( whisper && (*i)->GetServer( ) == creatorServer )
		{
			// note that we send this whisper only on the creator server

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ), creatorName, whisper );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ), creatorName, whisper );
		}
		else
		{
			// note that we send this chat message on all other bnet servers

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ) );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ) );
		}

		if( saveGame )
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, m_SaveGame, m_CurrentGame->GetHostCounter( ) );
		else
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, NULL, m_CurrentGame->GetHostCounter( ) );
	}

	if( m_AdminGame )
	{
		if( gameState == GAME_PRIVATE )
			m_AdminGame->SendAllChat( m_Language->CreatingPrivateGame( gameName, ownerName ) );
		else if( gameState == GAME_PUBLIC )
			m_AdminGame->SendAllChat( m_Language->CreatingPublicGame( gameName, ownerName ) );
	}

	// if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
	// unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
	// so don't rejoin the chat if we're using PVPGN

	if( gameState == GAME_PRIVATE )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetPasswordHashType( ) != "pvpgn" )
				(*i)->QueueEnterChat( );
		}
	}

	// hold friends and/or clan members

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( (*i)->GetHoldFriends( ) )
			(*i)->HoldFriends( m_CurrentGame );

		if( (*i)->GetHoldClan( ) )
			(*i)->HoldClan( m_CurrentGame );
	}

	// WaaaghTV

#ifdef WIN32
	if (m_wtv && m_CurrentGame->wtvprocessid == 0 && m_Map->GetMapObservers()>=3)
	{
		m_CurrentGame->CloseSlot( m_CurrentGame->m_Slots.size()-2, true );
		m_CurrentGame->CloseSlot( m_CurrentGame->m_Slots.size()-1, true );
		m_CurrentGame->CreateWTVPlayer( m_wtvPlayerName, true );

		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);
		ZeroMemory( &pi, sizeof(pi) );

		string wtvRecorderEXE = m_wtvPath + "wtvRecorder.exe";

		//hProcess = CreateProcess( _T("D:\\Programme\\wc3tv\\wtvRecorder.exe"), NULL, NULL, NULL, TRUE, HIGH_PRIORITY_CLASS, NULL, _T("D:\\Programme\\wc3tv\\" ), &si, &pi );
		//HANDLE hProcess = CreateProcess( wtvRecorderEXE, NULL, NULL, NULL, TRUE, HIGH_PRIORITY_CLASS, NULL, m_wtvPath, &si, &pi );
		int hProcess = CreateProcessA( wtvRecorderEXE.c_str( ), NULL, NULL, NULL, TRUE, HIGH_PRIORITY_CLASS, NULL, m_wtvPath.c_str( ), LPSTARTUPINFOA(&si), &pi );

		if( !hProcess )
			CONSOLE_Print( "[WaaaghTV] : Failed to start wtvRecorder.exe" );
		else
		{
			m_CurrentGame->wtvprocessid = int( pi.dwProcessId );
			CONSOLE_Print( "[WaaaghTV] : wtvRecorder.exe started!" );
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
#endif
}

void CGHost :: AdminGameMessage(string name, string message)
{
	if (m_AdminGame)
	{
		m_AdminGame->SendAllChat(name+": "+message);
	}
}


void CGHost :: UDPCommands( string Message )
{
	string IP;
	string Command;
	string Payload;
	string :: size_type CommandStart = Message.find( " " );

//	CONSOLE_Print( Message );

	if( CommandStart != string :: npos )
	{
		IP = Message.substr( 1, CommandStart-1 );
		Message = Message.substr( CommandStart + 1 );
	}
	else
		Message = Message.substr( 1 );

	m_LastIp = IP;

	string :: size_type PayloadStart = Message.find( " " );

	if( PayloadStart != string :: npos )
	{
		Command = Message.substr( 0, PayloadStart-0 );
		Payload = Message.substr( PayloadStart + 1 );
	}
	else
		Command = Message.substr( 0 );

	transform( Command.begin( ), Command.end( ), Command.begin( ), (int(*)(int))tolower );

//	CONSOLE_Print( "[GHOST] received UDP command [" + Command + "] with payload [" + Payload + "]"+" from IP ["+IP+"]" );

	if (Command == "connect" && !Payload.empty())
	{
		if (m_UDPPassword!="")
		if (Payload!=m_UDPPassword)
		{
			UDPChatSendBack("|invalidpassword");
			return;
		}
		UDPChatAdd(IP);
		UDPChatSend("|connected "+IP);
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			(*i)->m_LastFriendListTime = 0;
	}

	if (!UDPChatAuth(IP)) 
		return;

//	CONSOLE_Print( "[GHOST] received UDP command [" + Command + "] with payload [" + Payload + "]"+" from IP ["+IP+"]" );

	if (Command == "readwelcome")
	{
		ReadWelcome();
	}

	if (Command == "ping")
	{
		UDPChatSendBack("|pong");
	}

	if (Command == "disconnect" && !Payload.empty())
	{
		UDPChatDel(Payload);
		UDPChatSend("|disconnected "+Payload);
	}

	if (Command == "getchannel")
	{
		string Users="";

		if (m_UsersinChannel.size())
		{

		for( vector<string> :: iterator i = m_UsersinChannel.begin( ); i != m_UsersinChannel.end( ); i++ )
		{
			Users += (*i)+",";
		}
			UDPChatSendBack("|channelusers "+Users);
		}
	}

	if (Command == "start")
	{
		if (m_CurrentGame)
		if (m_CurrentGame->GetPlayers().size()>0)
		if (!m_CurrentGame->GetCountDownStarted( ))
		{
			if( Payload == "force" )
				m_CurrentGame->StartCountDown( true );
			else
				m_CurrentGame->StartCountDown( false );
		}	
	}

	if (Command == "lobbychat" && !Payload.empty())
	{
		bool CSaid = false;
		string hname = m_VirtualHostName;
		if (hname.substr(0,2)=="|c")
			hname = hname.substr(10,hname.length()-10);

		// handle bot commands

		if (m_CurrentGame->m_Players.size()==0) 
			return;

		string CMessage = Payload;

		if( !CMessage.empty( ) && CMessage[0] == m_CommandTrigger )
		{
			// extract the command trigger, the command, and the payload
			// e.g. "!say hello world" -> command: "say", payload: "hello world"

			CGamePlayer *Pl;
			string CCommand;
			string CPayload;
			string :: size_type CPayloadStart = CMessage.find( " " );

			if( CPayloadStart != string :: npos )
			{
				CCommand = CMessage.substr( 1, CPayloadStart - 1 );
				CPayload = CMessage.substr( CPayloadStart + 1 );
			}
			else
				CCommand = CMessage.substr( 1 );

			transform( CCommand.begin( ), CCommand.end( ), CCommand.begin( ), (int(*)(int))tolower );

			bool SearchPl = true;
			if (SearchPl)
				for( vector<CGamePlayer *> :: iterator k = m_CurrentGame->m_Players.begin( ); k != m_CurrentGame->m_Players.end( ); k++ )
				{
					if( (*k)->GetName( )== m_CurrentGame->GetOwnerName() && !(*k)->GetLeftMessageSent())
					{
						Pl = (*k);
						SearchPl = false;
						break;
					}
				}

				if (SearchPl)
					for( vector<CGamePlayer *> :: iterator k = m_CurrentGame->m_Players.begin( ); k != m_CurrentGame->m_Players.end( ); k++ )
					{
						if( !(*k)->GetLeftMessageSent( ) )
						{
							Pl = (*k);
							break;
						}
					}

					if (Pl!=NULL)
					{
						CONSOLE_Print( "[GHOST] received remote command [" + CCommand + "] with payload [" + CPayload + "]" );
						string n = m_RootAdmins;
						string sr = Pl->GetSpoofedRealm();
						bool sp = Pl->GetSpoofed();
						Pl->SetSpoofedRealm(m_CurrentGame->GetCreatorServer());
						if (!IsRootAdmin(Pl->GetName()))
							AddRootAdmin(Pl->GetName());
						Pl->SetSpoofed(true);
						m_CurrentGame->EventPlayerBotCommand( Pl, CCommand, CPayload );
						Pl->SetSpoofed(sp);
						Pl->SetSpoofedRealm(sr);
						m_RootAdmins = n;
						CSaid = true;
					}
		}
		if (!CSaid)
			m_CurrentGame->SendAllChat(Payload);
		else
			UDPChatSend("|lobby "+hname+" "+Payload);


	}

	if (Command == "say" && !Payload.empty())
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			(*i)->QueueChatCommand(Payload);
	}

	if (Command == "opendb")
	{
//		m_dbopen = true;
	}

	if (Command == "closedb")
	{
//		m_dbopen = false;
	}

	if (Command == "oneversion")
	{
		m_OneVersion = Payload;
		m_Version = m_OneVersion+" ("+m_GHostVersion+")";
	}

	if (Command == "gamechat" && !Payload.empty())
	{
		string :: size_type msgStart;
		string nr;
		uint32_t i;
		string msg;
		string name;

		msgStart = Payload.find( " " );
		nr = Payload.substr( 0, msgStart-0 );
		msg = Payload.substr( msgStart + 1 );
		sscanf(nr.c_str(), "%d", &i);
		msgStart = msg.find( " " );
		name = msg.substr( 0, msgStart-0 );
		msg = msg.substr( msgStart + 1 );

		if (m_Games.size()>i)
		{
			if (m_Games[i]->m_Players.size()==0) 
				return;
			CGamePlayer *Pl;
			Pl = m_Games[i]->GetPlayerFromName(name, false);
			if (Pl!=NULL)
				m_Games[i]->SendAllChat(Pl->GetPID(), msg);
			else m_Games[i]->SendAllChat(msg);
			// handle bot commands
			
			string CMessage = msg;

			if( !CMessage.empty( ) && CMessage[0] == m_CommandTrigger )
			{
				// extract the command trigger, the command, and the payload
				// e.g. "!say hello world" -> command: "say", payload: "hello world"

				string CCommand;
				string CPayload;
				string :: size_type CPayloadStart = CMessage.find( " " );

				if( CPayloadStart != string :: npos )
				{
					CCommand = CMessage.substr( 1, CPayloadStart - 1 );
					CPayload = CMessage.substr( CPayloadStart + 1 );
				}
				else
					CCommand = CMessage.substr( 1 );

				transform( CCommand.begin( ), CCommand.end( ), CCommand.begin( ), (int(*)(int))tolower );

				bool SearchPl = false;
			
				if (Pl==NULL)
					SearchPl = true;
				else if (Pl->GetLeftMessageSent())
					SearchPl = true;

				if (SearchPl)
				for( vector<CGamePlayer *> :: iterator k = m_Games[i]->m_Players.begin( ); k != m_Games[i]->m_Players.end( ); k++ )
				{
					if( (*k)->GetName( )== m_Games[i]->GetOwnerName() && !(*k)->GetLeftMessageSent())
					{
						Pl = (*k);
						SearchPl = false;
						break;
					}
				}

				if (SearchPl)
				for( vector<CGamePlayer *> :: iterator k = m_Games[i]->m_Players.begin( ); k != m_Games[i]->m_Players.end( ); k++ )
				{
					if( !(*k)->GetLeftMessageSent( ) )
					{
						Pl = (*k);
						break;
					}
				}

				if (Pl!=NULL)
				{
					CONSOLE_Print( "[GHOST] received remote command [" + CCommand + "] with payload [" + CPayload + "]" );
					string n = m_RootAdmins;
					string sr = Pl->GetSpoofedRealm();
					bool sp = Pl->GetSpoofed();
					Pl->SetSpoofedRealm(m_Games[i]->GetCreatorServer());
					if (!IsRootAdmin(Pl->GetName()))
						AddRootAdmin(Pl->GetName());
					Pl->SetSpoofed(true);
					m_Games[i]->EventPlayerBotCommand( Pl, CCommand, CPayload );
					Pl->SetSpoofed(sp);
					Pl->SetSpoofedRealm(sr);
					m_RootAdmins = n;
				}
			}
		}
	}

	if (Command == "gamechata" && !Payload.empty())
	{
		string :: size_type msgStart;
		string nr;
		uint32_t i;
		string msg;
		string name;

		msgStart = Payload.find( " " );
		nr = Payload.substr( 0, msgStart-0 );
		msg = Payload.substr( msgStart + 1 );
		sscanf(nr.c_str(), "%d", &i);
		msgStart = msg.find( " " );
		name = msg.substr( 0, msgStart-0 );
		msg = msg.substr( msgStart + 1 );

		if (m_Games.size()>i)
		{
			if (m_Games[i]->m_Players.size()==0) 
				return;
			CGamePlayer *Pl;
			Pl = m_Games[i]->GetPlayerFromName(name, false);
			if (Pl!=NULL)
				m_Games[i]->SendAllyChat(Pl->GetPID(), msg);
			else m_Games[i]->SendAllyChat(msg);
			// handle bot commands

			string CMessage = msg;

			if( !CMessage.empty( ) && CMessage[0] == m_CommandTrigger )
			{
				// extract the command trigger, the command, and the payload
				// e.g. "!say hello world" -> command: "say", payload: "hello world"

				string CCommand;
				string CPayload;
				string :: size_type CPayloadStart = CMessage.find( " " );

				if( CPayloadStart != string :: npos )
				{
					CCommand = CMessage.substr( 1, CPayloadStart - 1 );
					CPayload = CMessage.substr( CPayloadStart + 1 );
				}
				else
					CCommand = CMessage.substr( 1 );

				transform( CCommand.begin( ), CCommand.end( ), CCommand.begin( ), (int(*)(int))tolower );

				bool SearchPl = false;

				if (Pl==NULL)
					SearchPl = true;
				else if (Pl->GetLeftMessageSent())
					SearchPl = true;

				if (SearchPl)
					for( vector<CGamePlayer *> :: iterator k = m_Games[i]->m_Players.begin( ); k != m_Games[i]->m_Players.end( ); k++ )
					{
						if( (*k)->GetName( )== m_Games[i]->GetOwnerName() && !(*k)->GetLeftMessageSent())
						{
							Pl = (*k);
							SearchPl = false;
							break;
						}
					}

					if (SearchPl)
						for( vector<CGamePlayer *> :: iterator k = m_Games[i]->m_Players.begin( ); k != m_Games[i]->m_Players.end( ); k++ )
						{
							if( !(*k)->GetLeftMessageSent( ) )
							{
								Pl = (*k);
								break;
							}
						}

						if (Pl!=NULL)
						{
							CONSOLE_Print( "[GHOST] received remote command [" + CCommand + "] with payload [" + CPayload + "]" );
							string n = m_RootAdmins;
							string sr = Pl->GetSpoofedRealm();
							bool sp = Pl->GetSpoofed();
							Pl->SetSpoofedRealm(m_Games[i]->GetCreatorServer());
							if (!IsRootAdmin(Pl->GetName()))
								AddRootAdmin(Pl->GetName());
							Pl->SetSpoofed(true);
							m_Games[i]->EventPlayerBotCommand( Pl, CCommand, CPayload );
							Pl->SetSpoofed(sp);
							Pl->SetSpoofedRealm(sr);
							m_RootAdmins = n;
						}
			}
		}
	}


	if (Command == "language")
	{
		delete m_Language;
		m_Language = new CLanguage( m_LanguageFile );
	}

	if (Command == "updatefriends")
	{
		if (m_BNETs.size()>0)
			m_BNETs[0]->SendGetFriendsList();		
	}

	if (Command == "getfriends")
	{
		string friends = string();
		if (m_BNETs.size()>0)
		if (m_BNETs[0]->GetFriends()->size( )>0)
		{
			for( vector<CIncomingFriendList *> :: iterator g = m_BNETs[0]->GetFriends()->begin( ); g != m_BNETs[0]->GetFriends()->end( ); g++)
			{
				friends = friends + (*g)->GetAccount()+","+UTIL_ToString((*g)->GetStatus())+","+UTIL_ToString((*g)->GetArea())+",";				
			}
			UDPChatSendBack("|friends "+friends);
		}
	}

	if (Command == "getgames")
	{
		string games;
		if( m_CurrentGame )
			games = games + "L "+m_CurrentGame->GetGameName()+"|";
		games = games + UTIL_ToString(m_Games.size( ));
		games = "|games "+games+" ";
		if (m_Games.size( )>0)
		for( vector<CBaseGame *> :: iterator g = m_Games.begin( ); g != m_Games.end( ); g++)
		{
			games = games + (*g)->GetGameName()+'|';
		}

		UDPChatSendBack(games);
	}

	if (Command == "bang" && !Payload.empty())
	{	
		uint32_t ig;
		string name;
		stringstream SS;
		CBaseGame *Game;
		SS <<Payload;
		SS >>ig;
		SS >>name;

		if (m_Games.size()>=ig+1)
		{
			Game = m_Games[ig];
			string VictimLower = name;
			transform( VictimLower.begin( ), VictimLower.end( ), VictimLower.begin( ), (int(*)(int))tolower );
			uint32_t Matches = 0;
			CDBBan *LastMatch = NULL;

			// try to match each player with the passed string (e.g. "Gen" would be matched with "lock")
			// we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

			for( vector<CDBBan *> :: iterator i = Game->m_DBBans.begin( ); i != Game->m_DBBans.end( ); i++ )
			{
				string TestName = (*i)->GetName( );
				transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

				if( TestName.find( VictimLower ) != string :: npos )
				{
					Matches++;
					LastMatch = *i;
				}
			}
			if( Matches == 0 )
				Game->SendAllChat( m_Language->UnableToBanNoMatchesFound( name ) );
			else if( Matches == 1 )
			{
				bool isAdmin = Game->IsOwner(LastMatch->GetName());
				for( vector<CBNET *> :: iterator j = m_BNETs.begin( ); j != m_BNETs.end( ); j++ )
				{
					if( (*j)->IsAdmin(LastMatch->GetName() ) || (*j)->IsRootAdmin( LastMatch->GetName() ) )
					{
						isAdmin = true;
						break;
					}
				}

				if (isAdmin)
				{
//					Game->SendAllChat( "You can't ban an admin!");
					return;
				}

				uint32_t timehasleft = GetTime();
				
				for( vector<CDBGamePlayer *> :: iterator i = Game->m_DBGamePlayers.begin( ); i != Game->m_DBGamePlayers.end( ); i++ )
				{
					if ((*i)->GetName() == LastMatch->GetName())
						timehasleft = (*i)->GetLeavingTime();
				}

				string Reason = Game->CustomReason( timehasleft, string(), LastMatch->GetName() );

				CGame * Game2 = dynamic_cast <CGame *>(Game);

				if( !LastMatch->GetServer( ).empty( ) )
				{
					// the user was spoof checked, ban only on the spoofed realm

					Game2->m_PairedBanAdds.push_back( PairedBanAdd( m_RootAdmin, m_DB->ThreadedBanAdd( LastMatch->GetServer( ), LastMatch->GetName( ), LastMatch->GetIP( ), Game->GetGameName(), m_RootAdmin, Reason, 0, 0 ) ) );
					//							m_GHost->m_DB->BanAdd( LastMatch->GetServer( ), LastMatch->GetName( ), LastMatch->GetIP( ), m_GameName, User, Reason );
				}
				else
				{
					// the user wasn't spoof checked, ban on every realm

					for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
						Game2->m_PairedBanAdds.push_back( PairedBanAdd( m_RootAdmin, m_DB->ThreadedBanAdd( (*i)->GetServer( ), LastMatch->GetName( ), LastMatch->GetIP( ), Game->GetGameName(), m_RootAdmin, Reason, 0, 0 ) ) );
					//								m_GHost->m_DB->BanAdd( (*i)->GetServer( ), LastMatch->GetName( ), LastMatch->GetIP( ), m_GameName, User, Reason );
				}

				uint32_t GameNr = Game->GetGameNr();

				UDPChatSend("|ban "+UTIL_ToString(GameNr)+" "+LastMatch->GetName( ));
				CONSOLE_Print( "[GAME: " + Game->GetGameName() + "] player [" + LastMatch->GetName( ) + "] was banned by player [" + m_RootAdmin + "]" );

				string sBan = m_Language->PlayerWasBannedByPlayer(
					LastMatch->GetServer(),
					LastMatch->GetName( )+" ("+LastMatch->GetIP()+")", m_RootAdmin, UTIL_ToString(m_BanTime));
				string sBReason = sBan + ", "+Reason;

				if (Reason=="")
				{
					Game->SendAllChat( sBan );
				} else
				{
					if (sBReason.length()<220 && !m_TwoLinesBanAnnouncement)
						Game->SendAllChat( sBReason );
					else
					{
						Game->SendAllChat( sBan);
						Game->SendAllChat( "Ban reason: " + Reason);
					}
				}
				if (m_NotifyBannedPlayers && !m_DetourAllMessagesToAdmins)
				{
					sBReason = "You have been banned";
					if (Reason!="")
						sBReason = sBReason+", "+Reason;
					for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == Game->GetCreatorServer( ) )
							(*i)->QueueChatCommand( sBReason, LastMatch->GetName(), true );
					}
				}
			}
			else
				Game->SendAllChat( m_Language->UnableToBanFoundMoreThanOneMatch( name ) );	
		}
	}

	if (Command == "kickg" && !Payload.empty())
	{	
		uint32_t ig;
		string name;
		stringstream SS;
		CBaseGame *Game;
		SS <<Payload;
		SS >>ig;
		SS >>name;
		
		if (m_Games.size()>=ig+1)
		{
			Game = m_Games[ig];
			CGamePlayer *LastMatch = NULL;
			uint32_t Matches = Game->GetPlayerFromNamePartial( name, &LastMatch );

			if( Matches == 0 )
				Game->SendAllChat( m_Language->UnableToKickNoMatchesFound( name ) );
			else if( Matches == 1 )
			{
				LastMatch->SetDeleteMe( true );
				LastMatch->SetLeftReason( m_Language->WasKickedByPlayer( Game->GetOwnerName() ) );

				LastMatch->SetLeftCode( PLAYERLEAVE_LOBBY );

				Game->OpenSlot( Game->GetSIDFromPID( LastMatch->GetPID( ) ), false );
			}
			else
				Game->SendAllChat( m_Language->UnableToKickFoundMoreThanOneMatch( name ) );	
		}
	}

	if (Command == "kickl" && !Payload.empty())
	{	
		if (m_CurrentGame)
		{
			CGamePlayer *LastMatch = NULL;
			uint32_t Matches = m_CurrentGame->GetPlayerFromNamePartial( Payload, &LastMatch );

			if( Matches == 0 )
				m_CurrentGame->SendAllChat( m_Language->UnableToKickNoMatchesFound( Payload ) );
			else if( Matches == 1 )
			{
				LastMatch->SetDeleteMe( true );
				LastMatch->SetLeftReason( m_Language->WasKickedByPlayer( m_CurrentGame->GetOwnerName() ) );

				LastMatch->SetLeftCode( PLAYERLEAVE_LOBBY );

				m_CurrentGame->OpenSlot( m_CurrentGame->GetSIDFromPID( LastMatch->GetPID( ) ), false );
			}
			else
				m_CurrentGame->SendAllChat( m_Language->UnableToKickFoundMoreThanOneMatch( Payload ) );	
		}
	}

	if (Command == "ip")
	{
		if (m_ExternalIP=="")
		{
			m_ExternalIP = Payload;
			m_ExternalIPL=ntohl(inet_addr(m_ExternalIP.c_str()));
			m_Country=m_DBLocal->FromCheck(m_ExternalIPL);
			CONSOLE_Print( "[GHOST] External IP is " + m_ExternalIP);
			CONSOLE_Print( "[GHOST] Country is " + m_Country);
		}
	}

	if (Command == "rehost")
	{
		if (m_CurrentGame)
		if ( !Payload.empty() && !m_CurrentGame->GetCountDownStarted() && !m_CurrentGame->GetSaveGame() )
			if (Payload.size()<31)
			{
				m_CurrentGame->SetExiting(true);
				newGame = true;
				newGameCountry2Check = m_CurrentGame->m_CountryCheck2;
				newGameCountryCheck = m_CurrentGame->m_CountryCheck;
				newGameCountries = m_CurrentGame->m_Countries;
				newGameCountries2 = m_CurrentGame->m_Countries2;
				newGameProvidersCheck = m_CurrentGame->m_ProviderCheck;
				newGameProviders2Check = m_CurrentGame->m_ProviderCheck2;
				newGameProviders = m_CurrentGame->m_Providers;
				newGameProviders2 = m_CurrentGame->m_Providers2;
				newGameGameState = m_CurrentGame->GetGameState();
				newGameServer = m_CurrentGame->m_Server;
				newGameUser = m_CurrentGame->GetOwnerName();				
				newGameGArena = m_CurrentGame->m_GarenaOnly;				
				newGameName = Payload;				
//				UDPChatSend("|rehost");
			}		
	}

	if (Command == "provider" || Command =="provider2")
	{
		if (!m_CurrentGame) return;
		if (Payload.empty())
		{
			if (Command == "provider")
				m_CurrentGame->m_ProviderCheck = false;
			if (Command == "provider2")
				m_CurrentGame->m_ProviderCheck2 = false;
		}
		if (!Payload.empty())
		{
			string From;
			string Froms;
			string s;
			bool isAdmin = false;
			bool allowed = false;

			if (Command == "provider")
			if (m_CurrentGame->m_ProviderCheck)
				m_CurrentGame->m_Providers +=" "+Payload;
			else
			{
				m_CurrentGame->m_Providers = Payload;
				m_CurrentGame->m_ProviderCheck = true;
			}
			if (Command == "provider2")
				if (m_CurrentGame->m_ProviderCheck2)
					m_CurrentGame->m_Providers2 +=" "+Payload;
				else
				{
					m_CurrentGame->m_Providers2 = Payload;
					m_CurrentGame->m_ProviderCheck2 = true;
				}
			{
				transform( m_CurrentGame->m_Providers.begin( ), m_CurrentGame->m_Providers.end( ), m_CurrentGame->m_Providers.begin( ), (int(*)(int))toupper );
				transform( m_CurrentGame->m_Providers2.begin( ), m_CurrentGame->m_Providers2.end( ), m_CurrentGame->m_Providers2.begin( ), (int(*)(int))toupper );

				if (m_CurrentGame->m_ProviderCheck)				
					m_CurrentGame->SendAllChat( "Provider check enabled, allowed providers: "+m_CurrentGame->m_Providers);
				if (m_CurrentGame->m_ProviderCheck2)				
					m_CurrentGame->SendAllChat( "Provider check enabled, denied providers: "+m_CurrentGame->m_Providers2);

				for( vector<CGamePlayer *> :: iterator i = m_CurrentGame->m_Players.begin( ); i != m_CurrentGame->m_Players.end( ); i++ )
				{
					// we reverse the byte order on the IP because it's stored in network byte order

					//						From =m_GHost->UDPChatWhoIs((*i)->GetExternalIPString( ));
					From =(*i)->GetProvider( );
					transform( From.begin( ), From.end( ), From.begin( ), (int(*)(int))toupper );						

					isAdmin = false;
					allowed = false;

					for( vector<CBNET *> :: iterator j = m_BNETs.begin( ); j != m_BNETs.end( ); j++ )
					{
						if( (*j)->IsAdmin( (*i)->GetName( ) ) || (*j)->IsRootAdmin( (*i)->GetName( ) ) )
						{
							isAdmin = true;
							break;
						}
					}

					if (m_CurrentGame->IsReserved ((*i)->GetName()))
						isAdmin=true;

					if (m_CurrentGame->m_ProviderCheck)
					{
						stringstream SS;
						SS << m_CurrentGame->m_Providers;

						while( !SS.eof( ) )
						{
							SS >> s;
							if (From.find(s)!=string :: npos)
								allowed=true;
						}
					}

					if (m_CurrentGame->m_ProviderCheck2)
					{
						stringstream SS;
						SS << m_CurrentGame->m_Providers2;

						while( !SS.eof( ) )
						{
							SS >> s;
							if (From.find(s)!=string :: npos)
								allowed=false;
						}
					}

					if (!allowed)
// 						if ((*i)->GetName( )!=RootAdmin)
							if (isAdmin==false)
							{
								m_CurrentGame->SendAllChat( m_Language->AutokickingPlayerForDeniedProvider( (*i)->GetName( ), From ) );
								(*i)->SetDeleteMe( true );
								(*i)->SetLeftReason( "was autokicked," + From + " provider "+From+" not allowed");
								(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
								m_CurrentGame->OpenSlot( m_CurrentGame->GetSIDFromPID( (*i)->GetPID( ) ), false );
							}
				}

			}	

		}
	}
	
	if (Command == "rootadmin" && !Payload.empty())
	{
		AddRootAdmin(Payload);
	}

	if (Command == "getfroms")
	{
		string froms;
		string Froms;
		string CN;
		string PR;
		string PG;
		string SL;
		string IP;
//		string SC;
		string sc = "0";
		string ra = "0";
		bool SendLobby = true;
		uint32_t ig = 0;
		uint32_t c;
		uint32_t t;

		if (SendLobby)
		if (m_CurrentGame)
		{
			c = m_CurrentGame->m_Players.size();
			t = m_CurrentGame->m_Slots.size();
			bool dota = m_Map->GetMapType() == "dota";

			Froms = "L "+UTIL_ToString(dota) +" "+UTIL_ToString(c)+" "+UTIL_ToString(t)+" ";
			if (t>0)
			for( vector<CGamePlayer *> :: iterator i = m_CurrentGame->m_Players.begin( ); i != m_CurrentGame->m_Players.end( ); i++ )
			{
				if (!(*i)->GetScoreSet())
				{
					sc = "0";
					ra = "0";
				} else
				{
					sc = (*i)->GetScoreS();
					ra = (*i)->GetRanksS();
				}
				Froms += (*i)->GetName( );
				Froms += "|";
				CN = (*i)->GetCountry();
//				PR = (*i)->GetProvider();
				sc = (*i)->GetScoreS();
				ra = (*i)->GetRanksS();
				IP = (*i)->GetExternalIPString();
				PG = UTIL_ToString( (*i)->GetPing( m_LCPings ) );
				SL = UTIL_ToString( m_CurrentGame->GetSIDFromPID((*i)->GetPID()));
				Froms += SL+"|"+PG+"|"+CN+"|"+IP+"|"+sc+ "|"+ra;
				Froms += "|";
			}
			UDPChatSendBack("|froms "+Froms);
			return;
		}
	}

	if (Command == "getgame")
	{
		string froms;
		string Froms;
		string PG;
		string SL;
		string CN;
		uint32_t ig = 0;
		uint32_t c;
		uint32_t t;
		string dk = "0";
		string dd = "0";
		string sc = "0";
		string ra = "0";
		string ac = "0";

		if (!Payload.empty())
			{
				stringstream SS;
				SS << Payload;
				SS >> ig;
			}

		if (m_Games.size()==0) return;
		int j=0;
		CBaseGame *g;
		for( vector<CBaseGame *> :: iterator gg = m_Games.begin( ); gg != m_Games.end( ); gg++)
		{
			g = (*gg);
			if (ig == -1 || ig == j)
			{
				c = g->m_Players.size();
				t = g->m_Slots.size();
				Froms = UTIL_ToString(j)+" "+UTIL_ToString(c)+" "+UTIL_ToString(t)+" ";
				for( vector<CGamePlayer *> :: iterator jj = g->m_Players.begin( ); jj != g->m_Players.end( ); jj++ )
				{
					sc = "0";
					ra = "0";
					CGamePlayer *i = (*jj);
					ac = "1";
					if (!i->GetScoreSet())
					{
						sc = "0";
						ra = "0";
					} else
					{
						sc = i->GetScoreS();
						ra = i->GetRanksS();
					}
					Froms += i->GetName( );
					Froms += "|";
					CN = i->GetCountry();
					PG = UTIL_ToString( i->GetPing( m_LCPings ) );
					SL = UTIL_ToString( g->GetSIDFromPID(i->GetPID()));
					dk = UTIL_ToString(i->GetDOTAKills());
					dd = UTIL_ToString(i->GetDOTADeaths());

					Froms += SL+"|"+ac+"|"+PG+"|"+sc+"|"+ra+ "|"+dk+ "|"+dd+ "|"+CN;
					if( (jj != g->m_Players.end( ) - 1) ||
						(g->m_DBGamePlayers.size()>0) )
					{
						Froms += "|";
					}					
				}
				for( vector<CDBGamePlayer *> :: iterator ii = g->m_DBGamePlayers.begin( ); ii != g->m_DBGamePlayers.end( ); ii++ )
				{
					sc = "0";
					ra = "0";
//					CGamePlayer *i = g->GetPlayerFromName((*ii)->GetName(), false);
//					if (!i)
					{
						sc = "0";
						ra = "0";
						ac = "0";
						dd = "0";
						dk = "0";
						SL = UTIL_ToString((*ii)->GetSlot());
						PG = "0";
						CN = (*ii)->GetCountry();
						Froms += (*ii)->GetName( );
						Froms += "|";
					}
					Froms += SL+"|"+ac+"|"+PG+"|"+sc+"|"+ra+ "|"+dk+ "|"+dd+ "|"+CN;
					if( ii != g->m_DBGamePlayers.end( ) - 1 )
					{
						Froms += "|";
					}
				}
				UDPChatSendBack("|game "+Froms);
			}
			j++;
		}
	}
}

void CGHost :: ReloadConfig ()
{
	CConfig CFGF;
	CConfig *CFG;
	CFGF.Read( "ghost.cfg");
	CFG = &CFGF;

	stringstream SS;
	string istr = string();
	m_LanguageFile = CFG->GetString( "bot_language", "language.cfg" );
	if (m_Language)
		delete m_Language;
	m_Language = new CLanguage( m_LanguageFile );
#ifdef WIN32
	sPathEnd = "\\";
#else
	sPathEnd = "/";
#endif
	m_Warcraft3Path = CFG->GetString( "bot_war3path", "C:\\Program Files\\Warcraft III\\" );
	m_Warcraft3Path = FixPath(m_Warcraft3Path, sPathEnd);
	m_wtvPath = CFG->GetString( "wtv_path", "C:\\Program Files\\WaaaghTV Recorder\\" );
	m_wtvPath = FixPath(m_wtvPath, sPathEnd);
	m_wtvPlayerName = CFG->GetString( "wtv_playername", "Waaagh!TV" );
	m_wtv = CFG->GetInt( "wtv_enabled", 0 ) == 0 ? false : true;
#ifndef WIN32
	m_wtv = false;
#endif
	if (m_wtv)
		CONSOLE_Print("[WTV] WaaaghTV is enabled.");
	else
		CONSOLE_Print("[WTV] WaaaghTV is not enabled.");
	m_BindAddress = CFG->GetString( "bot_bindaddress", string( ) );
	m_HostPort = CFG->GetInt( "bot_hostport", 6112 );
	m_MaxGames = CFG->GetInt( "bot_maxgames", 60 );
	string BotCommandTrigger = CFG->GetString( "bot_commandtrigger", "!" );

	if( BotCommandTrigger.empty( ) )
		BotCommandTrigger = "!";

	m_CommandTrigger = BotCommandTrigger[0];
	m_MapCFGPath = CFG->GetString( "bot_mapcfgpath", string( ) );
	m_MapCFGPath = FixPath(m_MapCFGPath, sPathEnd);
	m_SaveGamePath = CFG->GetString( "bot_savegamepath", string( ) );
	m_SaveGamePath = FixPath(m_SaveGamePath, sPathEnd);
	m_MapPath = CFG->GetString( "bot_mappath", string( ) );
	m_MapPath = FixPath(m_MapPath, sPathEnd);
	m_SaveReplays = CFG->GetInt( "bot_savereplays", 0 ) == 0 ? false : true;
	m_ReplayPath = CFG->GetString( "bot_replaypath", string( ) );
	m_ReplayPath = FixPath(m_ReplayPath, sPathEnd);
	m_HideIPAddresses = CFG->GetInt( "bot_hideipaddresses", 0 ) == 0 ? false : true;
	m_RefreshMessages = CFG->GetInt( "bot_refreshmessages", 0 ) == 0 ? false : true;
	m_SpoofChecks = CFG->GetInt( "bot_spoofchecks", 2 );
	m_RequireSpoofChecks = CFG->GetInt( "bot_requirespoofchecks", 0 ) == 0 ? false : true;
	m_AutoLock = CFG->GetInt( "bot_autolock", 0 ) == 0 ? false : true;
	m_AutoSave = CFG->GetInt( "bot_autosave", 0 ) == 0 ? false : true;
	m_AllowDownloads = CFG->GetInt( "bot_allowdownloads", 0 );
	m_PingDuringDownloads = CFG->GetInt( "bot_pingduringdownloads", 0 ) == 0 ? false : true;
	m_LCPings = CFG->GetInt( "bot_lcpings", 1 ) == 0 ? false : true;
	m_AutoKickPing = CFG->GetInt( "bot_autokickping", 1450 );
	m_IPBlackListFile = CFG->GetString( "bot_ipblacklistfile", "ipblacklist.txt" );
	m_ReconnectWaitTime = CFG->GetInt( "bot_reconnectwaittime", 3 );
	m_ReplaceBanWithWarn = CFG->GetInt( "bot_replacebanwithwarn", 0 ) == 0 ? false : true;
	m_BlueCanHCL = CFG->GetInt( "bot_bluecanhcl", 0 ) == 0 ? false : true;
	m_BlueIsOwner = CFG->GetInt( "bot_blueisowner", 0 ) == 0 ? false : true;
	m_NormalCountdown = CFG->GetInt( "bot_normalcountdown", 0 ) == 0 ? false : true;
	m_DetourAllMessagesToAdmins = false;
	m_UnbanRemovesChannelBans = CFG->GetInt( "bot_unbanremoveschannelban", 0 ) == 0 ? false : true;
	m_WhisperAllMessages = CFG->GetInt( "bot_whisperallmessages", 1 ) == 0 ? false : true;
	m_Latency = CFG->GetInt( "bot_latency", 100 );
	m_UseDynamicLatency = CFG->GetInt( "bot_usedynamiclatency", 1 ) == 0 ? false : true;
	m_DynamicLatency2xPingMax = CFG->GetInt( "bot_dynamiclatency2.2xhighestpingmax", 1 ) == 0 ? false : true;
	m_DynamicLatencyMaxToAdd = CFG->GetInt( "bot_dynamiclatencymaxtoadd", 30 );
	m_DynamicLatencyAddedToPing = CFG->GetInt( "bot_dynamiclatencyaddedtoping", 25 );
	m_DynamicLatencyIncreasewhenLobby = CFG->GetInt( "bot_dynamiclatencyincreasewhenlobby", 1 ) == 0 ? false : true;
	m_SyncLimit = CFG->GetInt( "bot_synclimit", 50 );
	m_VoteKickAllowed = CFG->GetInt( "bot_votekickallowed", 1 ) == 0 ? false : true;
	m_VoteKickPercentage = CFG->GetInt( "bot_votekickpercentage", 100 );
	m_LanAdmins = CFG->GetInt( "bot_lanadmins", 0 ) == 0 ? false : true;
	m_LanRootAdmins = CFG->GetInt( "bot_lanrootadmins", 0 ) == 0 ? false : true;
	m_LocalAdmins = CFG->GetInt( "bot_localadmins", 0 ) == 0 ? false : true;
	m_ForceLoadInGame = CFG->GetInt( "bot_forceloadingame", 0 ) == 0 ? false : true;
	m_ShowRealSlotCount = CFG->GetInt( "lan_showrealslotcount", 0 ) == 0 ? false : true;
	m_UpdateDotaEloAfterGame = CFG->GetInt( "bot_updatedotaeloaftergame", 0 ) == 0 ? false : true;
	m_UpdateDotaScoreAfterGame = CFG->GetInt( "bot_updatedotascoreaftergame", 1 ) == 0 ? false : true;	
	m_AutoHostAutoStartPlayers = CFG->GetInt( "bot_autohostautostartplayers", 0 );
	m_AutoHostAllowStart = CFG->GetInt( "bot_autohostallowstart", 0 ) == 0 ? false : true;
	m_AutoHostLocal = CFG->GetInt( "bot_autohostlocal", 0 ) == 0 ? false : true;
	m_AutoHostMaximumGames = CFG->GetInt( "bot_autohostmaximumgames", 0 );
	m_AutoHostCountries2 = CFG->GetString( "bot_autohostdeniedcountries", string( ) );
	if (m_AutoHostCountries2.length()>0)
		m_AutoHostCountryCheck2 = true;
	m_AutoHostCountries = CFG->GetString( "bot_autohostallowedcountries", string( ) );
	if (m_AutoHostCountries.length()>0)
		m_AutoHostCountryCheck = true;
	m_AutoHostGameName = CFG->GetString( "bot_autohostgamename", string( ) );
	m_AutoHostOwner = CFG->GetString( "bot_autohostowner", string( ) );
	m_AutoHostMapCFG = CFG->GetString( "bot_autohostmapcfg", string( ) );
	if ((m_AutoHostMapCFG.find("\\")==string::npos) && (m_AutoHostMapCFG.find("/")==string::npos))
		m_AutoHostMapCFG = m_MapCFGPath + m_AutoHostMapCFG;
	
	m_GUIPort = CFG->GetInt( "udp_guiport", 5868 );
	m_AutoBan = CFG->GetInt( "bot_autoban", 0 ) == 0 ? false : true;
	m_AutoBanTeamDiffMax = CFG->GetInt( "bot_autobanteamdiffmax", 0 );
	m_AutoBanTimer = CFG->GetInt( "bot_autobantimer", 0 );
	m_AutoBanAll = CFG->GetInt( "bot_autobanall", 0 ) == 0 ? false : true;
	m_AutoBanFirstXLeavers = CFG->GetInt( "bot_autobanfirstxleavers", 0 );
	m_AutoBanGameLoading = CFG->GetInt( "bot_autobangameloading", 0 ) == 0 ? false : true;
	m_AutoBanCountDown = CFG->GetInt( "bot_autobancountdown", 0 ) == 0 ? false : true;
	m_AutoBanGameEndMins = CFG->GetInt( "bot_autobangameendmins", 3 );	
	if (m_AutoBanGameEndMins<1)
		m_AutoBanGameEndMins = 1;
	m_gamestateinhouse = CFG->GetInt( "bot_gamestateinhouse", 999 );
	m_AdminsLimitedUnban = CFG->GetInt( "bot_adminslimitedunban", 0 ) == 0 ? false : true;
	m_AdminsCantUnbanRootadminBans = CFG->GetInt( "bot_adminscantunbanrootadminbans", 0 ) == 0 ? false : true;
	m_LobbyAnnounceUnoccupied = CFG->GetInt( "bot_lobbyannounceunoccupied", 1 ) == 0 ? false : true;
	m_detectwtf = CFG->GetInt( "bot_detectwtf", 0 ) == 0 ? false : true;
	m_CensorMute = CFG->GetInt( "bot_censormute", 0 ) == 0 ? false : true;
	m_CensorMuteAdmins = CFG->GetInt( "bot_censormuteadmins", 0 ) == 0 ? false : true;
	m_CensorMuteFirstSeconds = CFG->GetInt( "bot_censormutefirstseconds", 60 );
	m_CensorMuteSecondSeconds = CFG->GetInt( "bot_censormutesecondseconds", 180 );
	m_CensorMuteExcessiveSeconds = CFG->GetInt( "bot_censormuteexcessiveseconds", 360 );
	m_CensorWords = CFG->GetString( "bot_censorwords", "xxx" );
	ParseCensoredWords();
	m_EndReq2ndTeamAccept = CFG->GetInt( "bot_endreq2ndteamaccept", 0 ) == 0 ? false : true;
	m_HideBotFromNormalUsersInGArena = CFG->GetInt( "bot_hidebotfromnormalusersingarena", 1 ) == 0 ? false : true;
	m_AutoRehostDelay = CFG->GetInt( "bot_autorehostdelay", 30 );
	LoadHostCounter();
	m_AllowedCountries = CFG->GetString( "bot_allowedcountries", string( ) );
	transform( m_AllowedCountries.begin( ), m_AllowedCountries.end( ), m_AllowedCountries.begin( ), (int(*)(int))toupper );
	m_DeniedCountries = CFG->GetString( "bot_deniedcountries", string( ) );
	transform( m_DeniedCountries.begin( ), m_DeniedCountries.end( ), m_DeniedCountries.begin( ), (int(*)(int))toupper );
	m_OwnerAccess = CFG->GetLong( "bot_owneraccess", 3965 );
	m_AdminAccess = CFG->GetLong( "bot_adminaccess", 4294963199 );
	m_DB->SetAdminAccess(m_AdminAccess);
	m_AdminsAndSafeCanDownload = CFG->GetInt( "bot_adminsandsafecandownload", 1 ) == 0 ? false : true;
	m_channeljoingreets = CFG->GetInt( "bot_channeljoingreets", 1 ) == 0 ? false : true;
	m_channeljoinmessage = CFG->GetInt( "bot_channeljoinmessage", 0 ) == 0 ? false : true;
	m_channeljoinexceptions = CFG->GetString( "bot_channeljoinexceptions", string() );
	UTIL_ExtractStrings(m_channeljoinexceptions, m_channeljoinex);
	m_broadcastinlan = CFG->GetInt( "bot_broadcastlan", 1 ) == 0 ? false : true;
	m_sendrealslotcounttogproxy = CFG->GetInt( "bot_sendrealslotcounttogproxy", 0 ) == 0 ? false : true;
	string m_IPUsersString = CFG->GetString( "bot_ipusers", string() );
	vector<string> m_IPs;
	UTIL_ExtractStrings(m_IPUsersString, m_IPs);
	UTIL_AddStrings(m_IPUsers, m_IPs);
	m_onlyownerscanstart = CFG->GetInt( "bot_onlyownerscanstart", 1 ) == 0 ? false : true;
	m_forceautohclindota = CFG->GetInt( "bot_forceautohclindota", 1 ) == 0 ? false : true;
	m_PlaceAdminsHigherOnlyInDota = CFG->GetInt( "bot_placeadminshigheronlyindota", 0 ) == 0 ? false : true;
	m_MaxHostCounter = CFG->GetInt( "bot_maxhostcounter", 30 );
	m_ScoreFormula = CFG->GetString( "bot_scoreformula", "(((wins-losses)/totgames)+(kills-deaths+assists/2)+(creepkills/100+creepdenies/10+neutralkills/50)+(raxkills/6)+(towerkills/11))" );
	m_ScoreMinGames = CFG->GetString( "bot_scoremingames", "3" );
	m_ReplayTimeShift = CFG->GetInt( "bot_replaytimeshift", 0 );
	if (m_ReplayTimeShift!=0)
	{
		char Time[17];
		time_t Now = time( NULL );
		memset( Time, 0, sizeof( char ) * 17 );
		time_t times = Now+ m_ReplayTimeShift;
		strftime( Time, sizeof( char ) * 17, "%Y-%m-%d %H-%M", localtime( &times ) );
		CONSOLE_Print("[GHOST] shifted replay time: "+ (string)Time);
	}
	m_DB->SetFormula(m_ScoreFormula);
	m_DB->SetMinGames(UTIL_ToUInt32(m_ScoreMinGames));

	m_bnetpacketdelaymedium = CFG->GetString( "bot_bnetpacketdelaymedium", "3200" );
	m_bnetpacketdelaybig = CFG->GetString( "bot_bnetpacketdelaybig", "4000" );
	m_bnetpacketdelaymediumpvpgn = CFG->GetString( "bot_bnetpacketdelaymediumpvpgn", "2000" );
	m_bnetpacketdelaybigpvpgn = CFG->GetString( "bot_bnetpacketdelaybigpvpgn", "2500" );

	if( m_VoteKickPercentage > 100 )
	{
		m_VoteKickPercentage = 100;
		CONSOLE_Print( "[GHOST] warning - bot_votekickpercentage is greater than 100, using 100 instead" );
	}

	m_DefaultMap = CFG->GetString( "bot_defaultmap", "map" );
	m_AdminGameCreate = CFG->GetInt( "admingame_create", 0 ) == 0 ? false : true;
	m_AdminGamePort = CFG->GetInt( "admingame_port", 6113 );
	m_AdminGamePassword = CFG->GetString( "admingame_password", string( ) );
	m_AdminGameMap = CFG->GetString( "admingame_map", string( ) );

	m_LobbyTimeLimit = CFG->GetInt( "bot_lobbytimelimit", 111 );
	m_LobbyTimeLimitMax = CFG->GetInt( "bot_lobbytimelimitmax", 150 );
	m_LANWar3Version = CFG->GetInt( "lan_war3version", 24 );
	m_ReplayBuildNumber = CFG->GetInt( "replay_buildnumber", 6059 );
	if (m_LANWar3Version == 23)
	{
		m_ReplayWar3Version = 23;
		m_ReplayBuildNumber = 6058;
	}
	if (m_LANWar3Version == 24)
	{
		m_ReplayWar3Version = 24;
		m_ReplayBuildNumber = 6059;
	}
	if (m_LANWar3Version == 26)
	{
		m_ReplayWar3Version = 26;
		m_ReplayBuildNumber = 6060;
	}
	m_AutoStartDotaGames = CFG->GetInt( "bot_autostartdotagames", 0 ) == 0 ? false : true;
	m_AllowedScores = CFG->GetInt( "bot_allowedscores", 0 );
	m_AutoHostAllowedScores = CFG->GetInt( "bot_autohostallowedscores", 0 );
	m_AllowNullScoredPlayers = CFG->GetInt( "bot_allownullscoredplayers", 1 ) == 0 ? false : true;
	m_newLatency = CFG->GetInt( "bot_newLatency", 0 ) == 0 ? false : true;
	m_newTimer = CFG->GetInt( "bot_newTimer", 1 ) == 0 ? false : true;
	m_newTimerResolution = CFG->GetInt( "bot_newTimerResolution", 5 );
	EndTimer();

	if (m_newTimer)
	{
		SetTimerResolution();
	}
	m_UsersCanHost = CFG->GetInt( "bot_userscanhost", 0 ) == 0 ? false : true;
	m_SafeCanHost = CFG->GetInt( "bot_safecanhost", 0 ) == 0 ? false : true;
	m_AdminMessages = CFG->GetInt( "bot_adminmessages", 0 ) == 0 ? false : true;
	m_LocalAdminMessages = CFG->GetInt( "bot_localadminmessages", 0 ) == 0 ? false : true;
	m_TCPNoDelay = CFG->GetInt( "tcp_nodelay", 0 ) == 0 ? false : true;
	m_AltFindIP = CFG->GetInt("bot_altfindip", 0 ) == 0 ? false : true;
	m_FindExternalIP = CFG->GetInt( "bot_findexternalip", 1 ) == 0 ? false : true;
	m_ExternalIP = CFG->GetString( "bot_externalip", string () );
	m_Verbose = CFG->GetInt( "bot_verbose", 1 ) == 0 ? false : true;
	m_RelayChatCommands = CFG->GetInt( "bot_relaychatcommands", 1 ) == 0 ? false : true;
	m_NonAdminCommands = CFG->GetInt( "bot_nonadmincommands", 1 ) == 0 ? false : true;
	m_NotifyBannedPlayers = CFG->GetInt( "bot_notifybannedplayers", 0 ) == 0 ? false : true;
	m_RootAdminsSpoofCheck = CFG->GetInt( "bot_rootadminsspoofcheck", 1 ) == 0 ? false : true;
	m_SafelistedBanImmunity = CFG->GetInt( "bot_safelistedbanimmunity", 1 ) == 0 ? false : true;
	m_SafeLobbyImmunity = CFG->GetInt( "bot_safelistedlobbyimmunity", 0 ) == 0 ? false : true;
	m_TBanLastTime = CFG->GetInt( "bot_tbanlasttime", 30 );
	m_BanLastTime = CFG->GetInt( "bot_banlasttime", 180 );
	m_BanTime = CFG->GetInt( "bot_bantime", 180 );
	m_BanTheWarnedPlayerQuota = CFG->GetInt( "bot_banthewarnedplayerquota", 3 );
	m_BanTimeOfWarnedPlayer = CFG->GetInt( "bot_bantimeofwarnedplayer", 14 );
	m_WarnTimeOfWarnedPlayer = CFG->GetInt( "bot_warntimeofwarnedplayer", 14 );
	m_GameNumToForgetAWarn = CFG->GetInt( "bot_gamenumtoforgetawarn", 7);
	m_AutoWarnEarlyLeavers = CFG->GetInt( "bot_autowarnearlyleavers", 0 ) == 0 ? false : true;
	m_GameLoadedPrintout = CFG->GetInt( "bot_gameloadedprintout", 10 );
	m_InformAboutWarnsPrintout = CFG->GetInt( "bot_informaboutwarnsprintout", 60 );
	m_KickUnknownFromChannel = CFG->GetInt( "bot_kickunknownfromchannel", 0 ) == 0 ? false : true;
	m_KickBannedFromChannel = CFG->GetInt( "bot_kickbannedfromchannel", 0 ) == 0 ? false : true;
	m_BanBannedFromChannel = CFG->GetInt( "bot_banbannedfromchannel", 0) == 0 ? false :true;
	m_AdminsSpoofCheck = CFG->GetInt( "bot_adminsspoofcheck", 1 ) == 0 ? false : true;
	m_TwoLinesBanAnnouncement = CFG->GetInt( "bot_twolinesbanannouncement", 1 ) == 0 ? false : true;
	m_CustomVersionText = CFG->GetString( "bot_customversiontext", string( ) );
	m_autoinsultlobby = CFG->GetInt( "bot_autoinsultlobby", 0 ) == 0 ? false : true;
	m_Version += m_CustomVersionText;
	m_norank = CFG->GetInt( "bot_norank", 0 ) == 0 ? false : true;
	m_nostatsdota = CFG->GetInt( "bot_nostatsdota", 0 ) == 0 ? false : true;
	m_addcreatorasfriendonhost = CFG->GetInt( "bot_addcreatorasfriendonhost", 0 ) == 0 ? false : true;
	m_autohclfromgamename = CFG->GetInt( "bot_autohclfromgamename", 1 ) == 0 ? false : true;
	m_gameoverbasefallen = CFG->GetInt( "bot_gameoverbasefallen", 20 );
	m_gameoverminpercent = CFG->GetInt( "bot_gameoverminpercent", 0 );
	m_gameoverminplayers = CFG->GetInt( "bot_gameoverminplayers", 2 );
	m_gameovermaxteamdifference = CFG->GetInt( "bot_gameovermaxteamdifference", 0 );
	m_totaldownloadspeed = CFG->GetInt( "bot_totaldownloadspeed", 1536 );
	m_clientdownloadspeed = CFG->GetInt( "bot_clientdownloadspeed", 1024 );
	m_maxdownloaders = CFG->GetInt( "bot_maxdownloaders", 3 );
	m_ShowDownloadsInfoTime = CFG->GetInt( "bot_showdownloadsinfotime", 9 );
	m_ShowDownloadsInfo = CFG->GetInt( "bot_showdownloadsinfo", 1 ) == 0 ? false : true;
	m_ShowScoresOnJoin = CFG->GetInt( "bot_showscoresonjoin", 0 ) == 0 ? false : true;
	m_ShowNotesOnJoin = CFG->GetInt( "bot_shownotesonjoin", 0 ) == 0 ? false : true;
	m_ShuffleSlotsOnStart = CFG->GetInt( "bot_shuffleslotsonstart", 0 ) == 0 ? false : true;
	m_ShowCountryNotAllowed = CFG->GetInt( "bot_showcountrynotallowed", 1 ) == 0 ? false : true;
	m_RehostIfNameTaken = CFG->GetInt( "bot_rehostifnametaken", 1 ) == 0 ? false : true;
	m_RootAdmins = CFG->GetString( "bot_rootadmins", string () );
	transform( m_RootAdmins.begin( ), m_RootAdmins.end( ), m_RootAdmins.begin( ), (int(*)(int))tolower );
	m_FakePings = CFG->GetString( "bot_fakepings", string () );
	transform( m_FakePings.begin( ), m_FakePings.end( ), m_FakePings.begin( ), (int(*)(int))tolower );
	m_patch23 = CFG->GetInt( "bot_patch23ornewer", 1 ) == 0 ? false : true;
	m_patch21 = CFG->GetInt( "bot_patch21", 0 ) == 0 ? false : true;
	m_HoldPlayersForRMK = CFG->GetInt( "bot_holdplayersforrmk", 1 ) == 0 ? false : true;
	m_onlyownerscanswapadmins = CFG->GetInt( "bot_onlyownerscanswapadmins", 1 ) == 0 ? false : true;
	m_PlayersfromRMK = string();
	m_dropifdesync = CFG->GetInt( "bot_dropifdesync", 1 ) == 0 ? false : true; //Metal_Koola
	m_UDPPassword = CFG->GetString( "bot_udppassword", string () );
	m_VirtualHostName = CFG->GetString( "bot_virtualhostname", "|cFF483D8BOne" );
	if (m_VirtualHostName.length()>15)
		m_VirtualHostName=m_VirtualHostName.substr(0,15);
	m_DropVoteTime = CFG->GetInt( "bot_dropvotetime", 30 );
	m_IPBanning = CFG->GetInt( "bot_ipbanning", 2 );
	m_Banning = CFG->GetInt( "bot_banning", 1 );
	ReadProviders();
	size_t f;
	providersn.clear();
	providers.clear();
	for(uint32_t i=0; i<m_Providers.size(); i++)
	{
		f = m_Providers[i].find("=");
		if (f!=string :: npos)
		{
			providersn.push_back(m_Providers[i].substr(0,f));
			providers.push_back(m_Providers[i].substr(f+1,500));
		}
	}
	ReadWelcome();
	ReadChannelWelcome();
	ReadMars();
}

void CGHost :: ReadChannelWelcome ()
{
	string file = "channelwelcome.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_ChannelWelcome.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_ChannelWelcome.push_back(Line);
		}
	}
	in.close( );
}

void CGHost :: ReadProviders ()
{
	string file = "providers.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_Providers.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_Providers.push_back(Line);
		}
	}
	in.close( );
}

void CGHost :: ReadWelcome ()
{
	string file = "welcome.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_Welcome.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_Welcome.push_back(Line);
		}
	}
	in.close( );
}

string CGHost :: GetMars ()
{
	if (m_Mars.size()==0)
		return string();
	bool ok = true;
	// delete the oldest message
	if (m_MarsLast.size()>=m_Mars.size() || m_MarsLast.size()>15)
	if (m_MarsLast.size()>0)
		m_MarsLast.erase(m_MarsLast.begin());
	do
	{
		ok = true;
		random_shuffle( m_Mars.begin( ), m_Mars.end( ) );
		for (uint32_t i = 0; i<m_MarsLast.size(); i++)
		{
			if (m_MarsLast[i]==m_Mars[0])
			{
				ok = false;
				break;
			}
		}
	} while (!ok);

	m_MarsLast.push_back(m_Mars[0]);
	return m_Mars[0];
}

void CGHost :: ReadMars ()
{
	string file = "mars.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_Mars.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_Mars.push_back(Line);
		}
	}
	in.close( );
	srand((unsigned)time(0));
}

void CGHost :: UDPChatDel(string ip)
{
	for( vector<string> :: iterator i = gGHost->m_UDPUsers.begin( ); i != gGHost->m_UDPUsers.end( ); i++ )
	{
		if( (*i) == ip )
		{
			CONSOLE_Print("[UDPCMDSOCK] User "+ ip + " removed from UDP user list");
			gGHost->m_UDPUsers.erase(i);
			return;
		}
	}
}

bool CGHost :: UDPChatAuth(string ip)
{
	if (ip=="127.0.0.1")
		return true;

	for( vector<string> :: iterator i = gGHost->m_UDPUsers.begin( ); i != gGHost->m_UDPUsers.end( ); i++ )
	{
		if( (*i) == ip )
		{
			return true;
		}
	}
	return false;
}

void CGHost :: UDPChatAdd(string ip)
{
	// check that the ip is not already in the list
	for( vector<string> :: iterator i = gGHost->m_UDPUsers.begin( ); i != gGHost->m_UDPUsers.end( ); i++ )
	{
		if( (*i) == ip )
		{
			return;
		}
	}
	CONSOLE_Print("[UDPCMDSOCK] User "+ ip + " added to UDP user list");
	gGHost->m_UDPUsers.push_back( ip );
}

void CGHost :: UDPChatSend(BYTEARRAY b)
{
	if (m_UDPUsers.size()>0)
	{
		string ip;
		for( vector<string> :: iterator i = gGHost->m_UDPUsers.begin( ); i != gGHost->m_UDPUsers.end( ); i++ )
		{
			ip = (*i);			
			m_UDPSocket->SendTo(ip,6112,b);
		}
	}

	if (m_IPUsers.size()>0)
	{
		string ip;
		for( vector<string> :: iterator i = gGHost->m_IPUsers.begin( ); i != gGHost->m_IPUsers.end( ); i++ )
		{
			ip = (*i);			
			m_UDPSocket->SendTo(ip,6112,b);
		}
	}
}

void CGHost :: UDPChatSend(string s)
{
	m_inconsole = true;
//	CONSOLE_Print("[UDP] "+s);
	m_inconsole = false;
	string ip;
//	u_short port;
	BYTEARRAY b;
	char *c = new char[s.length()+2];
	strncpy(c,s.c_str(), s.length());
	c[s.length()]=0;
	b=UTIL_CreateByteArray(c,s.length());
	if (m_UDPUsers.size()>0)
	{
		for( vector<string> :: iterator i = gGHost->m_UDPUsers.begin( ); i != gGHost->m_UDPUsers.end( ); i++ )
		{
			ip = (*i);			
			m_UDPSocket->SendTo(ip,m_GUIPort,b);
		}
	}
	m_UDPSocket->SendTo("127.0.0.1",m_GUIPort,b);
}

void CGHost :: UDPChatSendBack(string s)
{
	m_inconsole = true;
//	CONSOLE_Print("[UDP] "+s);
	m_inconsole = false;
	if (m_LastIp=="") return;
	BYTEARRAY b;
	char *c = new char[s.length()+2];
	strncpy(c,s.c_str(), s.length());
	c[s.length()]=0;
	b=UTIL_CreateByteArray(c,s.length());
	m_UDPSocket->SendTo(m_LastIp,m_GUIPort,b);
}

string CGHost :: UDPChatWhoIs(string c, string s)
{
	string descr= string();
#ifdef WIN32
	try
	{
		PCHAR msg;
		msg = new char[s.length()+1];
		strcpy(msg,s.c_str());
		PCHAR cn;
		cn = new char[c.length()+1];
		strcpy(cn,c.c_str());
		descr=myudpwhois(cn, msg);
	}
	catch (...)
	{
	}
#endif
	return descr;
}

string CGHost :: Commands(unsigned int idx)
{
	string Result = string();
	if (idx<=m_Commands.size())
		Result = m_Commands[idx];
	return Result;
}

bool CGHost :: CommandAllowedToShow( string c)
{
	bool allowed = false;
	if (CMD_stringshow.find(c)!= string ::npos)
		allowed = true;
	return allowed;
}

void CGHost :: ParseCensoredWords( )
{
	m_CensoredWords.clear();
	transform( m_CensorWords.begin( ), m_CensorWords.end( ), m_CensorWords.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string s;
	SS << m_CensorWords;

	while( !SS.eof( ) )
	{
		SS >> s;
		m_CensoredWords.push_back(s);
	}
}

string CGHost :: Censor ( string msg)
{
	uint32_t idx = 1;

/* uncomment if you want the first two letter to be seen (instead of only one letter)
	if (msg.length()>2)
		idx = 2;
*/
	string Result = msg.substr(0,idx);

	for (uint32_t i = idx; i<=msg.length()-idx; i++)
	{
		Result+="*";
	}
	return Result;
}

string CGHost :: CensorRemoveDots( string msg)
{
	string msgnew = msg.substr(0,2);
	if (msg.length()<=4)
		return msg;
	for (uint32_t i = 2; i != msg.length()-2; i++)
	{
		if (msg.substr(i,1)!=".")
			msgnew += msg.substr(i,1);/* else
		{
			if (msg.substr(i-1,1)!=" " && msg.substr(i+1,1)!=" ")
		}*/
	}
	msgnew += msg.substr(msg.length()-2,2);
	return msgnew;
}

string CGHost :: CensorMessage( string msg)
{
	if (msg.length()<1)
		return msg;
	string Msg = msg;
	for( vector<string> :: iterator i = m_CensoredWords.begin( ); i != m_CensoredWords.end( ); i++ )
	{
		boost :: ireplace_all( Msg, (*i), Censor((*i)) );
//		Replace( Msg, (*i), Censor((*i)) );
	}
	return Msg;
}

bool CGHost :: ShouldFakePing(string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string s;
	SS << m_FakePings;

	while( !SS.eof( ) )
	{
		SS >> s;
		if (name == s)
		{
			return true;
		}
	}
	return false;
}

bool CGHost :: IsRootAdmin(string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string s;
	SS << m_RootAdmins;

	while( !SS.eof( ) )
	{
		SS >> s;
		if (name == s)
		{
			return true;
		}
	}
	return false;
}

void CGHost :: DelRootAdmin( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string tusers="";
	string luser;
	SS << m_RootAdmins;
	while( !SS.eof( ) )
	{
		SS >> luser;
		if (luser != name)
		if (tusers.length()==0)
			tusers = luser;
		else
			tusers +=" "+luser;
	}
	m_RootAdmins = tusers;
}


void CGHost :: AddRootAdmin(string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	if (m_RootAdmins.length()==0)
		m_RootAdmins = name;
	else
	if (!IsRootAdmin(name))
		m_RootAdmins +=" "+name;
}

uint32_t CGHost :: CMDAccessAddOwner (uint32_t acc)
{
	return acc | m_OwnerAccess;
}

uint32_t CGHost :: CMDAccessDel (uint32_t access, uint32_t cmd)
{
	uint32_t taccess;
	taccess = access;
	uint32_t Mask = 1;
	uint32_t scmd = cmd;
	uint32_t son = 0;
	uint32_t sacc = 0;
	if (scmd != 0)
		for (unsigned int k=1;k<=scmd;k++)
			Mask = Mask * 2;
	if (Mask > access)
		son = 0;
	else
		son = access & Mask;

	if (son==sacc || son==Mask)
	{
		if (sacc == 0)
		{
//			QueueChatCommand("Admin "+suser+ " already doesn't have access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		else
		{
//			QueueChatCommand("Admin "+suser+ " already has access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		return access;
	}
	if (sacc == 1)
		taccess+= Mask;
	else
		taccess -= Mask;

	return taccess;
}

uint32_t CGHost :: CMDAccessAdd (uint32_t access, uint32_t cmd)
{
	uint32_t taccess;
	taccess = access;
	uint32_t Mask = 1;
	uint32_t scmd = cmd;
	uint32_t son = 0;
	uint32_t sacc = 1;
	if (scmd != 0)
		for (unsigned int k=1;k<=scmd;k++)
			Mask = Mask * 2;
	if (Mask > access)
		son = 0;
	else
		son = access & Mask;

	if (son==sacc || son==Mask)
	{
		if (sacc == 0)
		{
			//			QueueChatCommand("Admin "+suser+ " already doesn't have access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		else
		{
			//			QueueChatCommand("Admin "+suser+ " already has access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		return access;
	}
	if (sacc == 1)
		taccess+= Mask;
	else
		taccess -= Mask;

	return taccess;
}

uint32_t CGHost :: CMDAccessAllOwner ()
{
	return m_OwnerAccess;
}

void CGHost :: SaveHostCounter()
{
	string File = "hostcounter.cfg";
	ofstream tmpcfg;
	tmpcfg.open( File.c_str( ), ios :: trunc );
	tmpcfg << "hostcounter = " << UTIL_ToString(m_HostCounter) << endl;
	tmpcfg.close( );
}

void CGHost :: LoadHostCounter()
{
	string File = "hostcounter.cfg";
	CConfig CFGH;
	CFGH.Read( File );
	m_HostCounter = CFGH.GetInt( "hostcounter", 1 );
}

void CGHost :: AddSpoofedIP(string name, string ip)
{
	m_CachedSpoofedIPs.push_back( ip );
	m_CachedSpoofedNames.push_back( name );
}

bool CGHost :: IsChannelException(string name)
{
	for( uint32_t i = 0; i != m_channeljoinex.size(); i++ )
	{
		if( m_channeljoinex[i] == name )
		{
			return true;
		}
	}
	return false;
}

bool CGHost :: IsSpoofedIP(string name, string ip)
{
	for( uint32_t i = 0; i != m_CachedSpoofedIPs.size(); i++ )
	{
		if( m_CachedSpoofedIPs[i] == ip && m_CachedSpoofedNames[i]== name)
		{
			return true;
		}
	}
	return false;
}

string CGHost :: IncGameNr ( string name)
{
	string GameName = name;
	string GameNr = string();
	bool found = false;
	uint32_t idx = 0;
	uint32_t id;
	uint32_t Nr = 0;

	idx = GameName.length()-1;
	for (id = 7; id >=1; id-- )
	{
		if (idx>=id)
			if (GameName.at(idx-id)=='#')
			{
				idx = idx-id+1;
				found = true;
				break;
			}
	}
	if (!found)
		idx = 0;

	// idx = 0, no Game Nr found in gamename
	if (idx == 0)
	{
		GameNr = "0";
		GameName = name + " #";
	}
	else
	{
		GameNr = GameName.substr(idx,GameName.length()-idx);
		GameName = GameName.substr(0,idx);
	}
	stringstream SS;
	SS << GameNr;
	SS >> Nr;
	Nr ++;
	if (m_MaxHostCounter!=0)
	if (Nr>m_MaxHostCounter)
		Nr = 1;
	GameNr = UTIL_ToString(Nr);
	GameName = GameName + GameNr;
	return GameName;
}

void CGHost :: CalculateScoresCount()
{
	m_ScoresCount = m_DB->ScoresCount(string());
	m_ScoresCountSet = true;
}

uint32_t CGHost :: ScoresCount()
{
	if (!m_ScoresCountSet)
		CalculateScoresCount();
	return m_ScoresCount;
}

void CGHost :: SetTimerResolution()
{
#ifdef WIN32
	// initialize timer resolution
	// attempt to set the resolution as low as possible from 1ms to 5ms

	unsigned int ii = 1;
	if (m_newTimerResolution>0)
		ii = m_newTimerResolution;
	if (ii>5)
		ii=5;

	for( unsigned int i = ii; i <= 5; i++ )
	{
		if( timeBeginPeriod( i ) == TIMERR_NOERROR )
		{
			m_newTimerResolution = i;
			break;
		}
		else if( i < 5 )
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution to " + UTIL_ToString( i ) + " milliseconds, trying a higher resolution" );
		else
		{
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution, going back to old timer function" );
			m_newTimer = false;
			return;
		}
	}

	CONSOLE_Print( "[GHOST] using Windows timer with resolution " + UTIL_ToString( m_newTimerResolution ) + " milliseconds" );
	m_newTimerStarted = true;
#endif
}

void CGHost :: EndTimer( )
{
	if (m_newTimerStarted)
	{
#ifdef WIN32
		timeEndPeriod( m_newTimerResolution );
#endif
		m_newTimerStarted = false;
	}
}

void CMyCallableDownloadFile :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = 0;

	// Create socket object
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(sock == SOCKET_ERROR)	// bad socket?
	{
		CONSOLE_Print("[GHOST] failed making socket");
		Close();
		return;
	}

	sockaddr_in sa;	
	sa.sin_family	= AF_INET;
	sa.sin_port		= htons(80);	// HTTP service uses port 80

	int port;
	char protocol[20],host[128],request[2048];

	/* Parse the URL */
	char * url;
	url = new char[m_File.length()+1];
	strcpy(url, m_File.c_str());	
	ParseURL(url,protocol,sizeof(protocol)-1,host,sizeof(host)-1,request,sizeof(request)-1,&port);
	string file = request;
	size_t fpos = file.rfind("/");
	if (fpos == string :: npos)
	{
		CONSOLE_Print("[GHOST] url not ok");
		Close();
		return;
	}
	file = file.substr(fpos+1, file.size()-fpos-1);

	// Time to get the hostname

	hostent *h = gethostbyname(host);
	if(!h)
	{
		CONSOLE_Print("[GHOST] failed to get host");
		Close();
		return;
	}

	memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], 4);

	if( connect(sock, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR )
	{
		CONSOLE_Print("[GHOST] failed to connect");
		Close();
		return;
	}

	// This is our packet that we are going to send to the HTTP server.
	// 

	string sshost = string(host);
	transform( sshost.begin( ), sshost.end( ), sshost.begin( ), (int(*)(int))tolower );	
	
	string shost;
	shost = "GET "+string(request)+" HTTP/1.1\r\n"+"Host: "+sshost+"\r\n"+"Referrer: \r\n"+"\r\n";
/*
	PCHAR Packet;
	Packet = new char[shost.length()+2];
	strcpy(Packet, shost.c_str());
*/

	int rtn = 0;
	int iOptVal = 3;
	int iOptLen = sizeof(int);
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&iOptVal, iOptLen);

//	rtn = send(sock, Packet, sizeof(Packet)-1, 0);
	rtn = send(sock, shost.c_str(), shost.size(), 0);
	if(rtn <= 0)
	{
		CONSOLE_Print("[GHOST] failed to send packet");
		Close();
		return;
	}

	char Buffer[16384] = {0};

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&iOptVal, iOptLen);

	rtn = recv(sock, Buffer, sizeof(Buffer), 0);
//	closesocket(sock);
	if(rtn <= 0) 
	{
		CONSOLE_Print("[GHOST] failed to receive packet");
		Close();
		return;
	}

	// Did we get a valid reply back from the server?
	bool redirect = !( _strnicmp(Buffer, "HTTP/1.1 302 Fo", 15));
	bool ok = !( _strnicmp(Buffer, "HTTP/1.1 200 OK", 15));

	if( !redirect && !ok)
	{
		CONSOLE_Print("[GHOST] invalid response");
		Close();
		return;
	}

	if (redirect)
	{
		string sbuf2 = Buffer;

		size_t pos3 = sbuf2.find("Location: ");
		if (pos3 == string :: npos)
		{
			CONSOLE_Print("[GHOST] content not OK!");
			Close();
			return;
		}
		pos3 += 10;
		size_t pos4 = sbuf2.find("\x0D\x0A", pos3);

		string clocation = sbuf2.substr(pos3, pos4-pos3);
		closesocket(sock);

		url = new char[clocation.length()+1];
		strcpy(url, clocation.c_str());	
		ParseURL(url,protocol,sizeof(protocol)-1,host,sizeof(host)-1,request,sizeof(request)-1,&port);
		file = request;
		fpos = file.rfind("/");
		if (fpos == string :: npos)
		{
			CONSOLE_Print("[GHOST] url not ok");
			Close();
			return;
		}
		file = file.substr(fpos+1, file.size()-fpos-1);
		// Time to get the hostname

		h = gethostbyname(host);
		if(!h)
		{
			CONSOLE_Print("[GHOST] failed to get host");
			Close();
			return;
		}

		memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], 4);
		// Create socket object
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if(sock == SOCKET_ERROR)	// bad socket?
		{
			CONSOLE_Print("[GHOST] failed making socket");
			Close();
			return;
		}
		if( connect(sock, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR )
		{
			CONSOLE_Print("[GHOST] failed to connect");
			Close();
			return;
		}
		string sshost2 = string(clocation);
		transform( sshost2.begin( ), sshost2.end( ), sshost2.begin( ), (int(*)(int))tolower );	
		string shost2;
		shost2 = "GET "+string(request)+" HTTP/1.1\r\n"+"Host: "+host+"\r\n"+"Referrer: \r\n"+"\r\n";
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&iOptVal, iOptLen);

		//	rtn = send(sock, Packet, sizeof(Packet)-1, 0);
		rtn = send(sock, shost2.c_str(), shost2.size(), 0);
		if(rtn <= 0)
		{
			CONSOLE_Print("[GHOST] failed to send packet");
			Close();
			return;
		}
		rtn = recv(sock, Buffer, sizeof(Buffer), 0);
		//	closesocket(sock);
		if(rtn <= 0) 
		{
			CONSOLE_Print("[GHOST] failed to receive packet");
			Close();
			return;
		}

		// Did we get a valid reply back from the server?
		ok = !( _strnicmp(Buffer, "HTTP/1.1 200 OK", 15));
		if( !ok)
		{
			CONSOLE_Print("[GHOST] invalid response");
			Close();
			return;
		}
	}

	string sbuf = Buffer;

	size_t pos1 = sbuf.find("Content-Length: ");
	if (pos1 == string :: npos)
	{
		CONSOLE_Print("[GHOST] content not OK!");
		Close();
		return;
	}
	pos1 += 16;
	size_t pos2 = sbuf.find("\x0D\x0A", pos1);
	
	string clength = sbuf.substr(pos1, pos2-pos1);

	size_t pos = sbuf.find("\x0D\x0A\x0D\x0A");
	if (pos == string :: npos)
	{
		CONSOLE_Print("[GHOST] content not OK!");
		Close();
		return;
	}
	else
		pos = pos+4;

	Replace( file, "%20", " ");
	uint32_t totalsize=0;
	ofstream myfile;
	string path = m_Path;
	string pfile = path + file;
	myfile.open (pfile.c_str(), ios_base::binary);
	myfile.write(&Buffer[pos], rtn-pos);
	totalsize += rtn-pos;
	while (rtn>0)
	{
		rtn = recv(sock, Buffer, sizeof(Buffer), 0);
		if (rtn>0)
		{
			myfile.write(Buffer, rtn);
			totalsize+=rtn;
		}
	}

	if (totalsize!=UTIL_ToUInt32(clength))
	{
		CONSOLE_Print("[GHOST] Error downloading file: "+UTIL_ToString(totalsize)+" out of "+clength+" received!");
		Close();
		return;
	}
	m_Result = totalsize;
	closesocket(sock);
	myfile.close();		

	Close( );
}

CMyCallableDownloadFile *CGHost :: ThreadedDownloadFile( string url, string path )
{
	CMyCallableDownloadFile *Callable = new CMyCallableDownloadFile( url, path);
	m_DB->CreateThreadReal( Callable );
	return Callable;
}
