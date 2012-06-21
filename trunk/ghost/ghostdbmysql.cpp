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

#ifdef GHOST_MYSQL

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "ghostdb.h"
#include "ghostdbmysql.h"

#include <signal.h>

#ifdef WIN32
 #include <winsock.h>
#endif

#include <mysql/mysql.h>
#include <boost/thread.hpp>

//
// CGHostDBMySQL
//

CGHostDBMySQL :: CGHostDBMySQL( CConfig *CFG ) : CGHostDB( CFG )
{
	m_Server = CFG->GetString( "db_mysql_server", string( ) );
	m_Database = CFG->GetString( "db_mysql_database", "ghost" );
	m_User = CFG->GetString( "db_mysql_user", string( ) );
	m_Password = CFG->GetString( "db_mysql_password", string( ) );
	m_Port = CFG->GetInt( "db_mysql_port", 3306 );
	m_BotID = CFG->GetInt( "db_mysql_botid", 0 );
	m_MaxConnections = 30;
	m_NumConnections = 1;
	m_OutstandingCallables = 0;

	mysql_library_init( 0, NULL, NULL );

	// create the first connection

	CONSOLE_Print( "[MYSQL] connecting to database server" );
	MYSQL *Connection = NULL;

	if( !( Connection = mysql_init( NULL ) ) )
	{
		CONSOLE_Print( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		m_Error = "error initializing MySQL connection";
		return;
	}

	my_bool Reconnect = true;
	mysql_options( Connection, MYSQL_OPT_RECONNECT, &Reconnect );

	if( !( mysql_real_connect( Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
	{
		CONSOLE_Print( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		m_Error = "error connecting to MySQL server";
		return;
	}

	m_IdleConnections.push( Connection );
}

CGHostDBMySQL :: ~CGHostDBMySQL( )
{
	CONSOLE_Print( "[MYSQL] closing " + UTIL_ToString( m_IdleConnections.size( ) ) + "/" + UTIL_ToString( m_NumConnections ) + " idle MySQL connections" );

	while( !m_IdleConnections.empty( ) )
	{
		mysql_close( (MYSQL *)m_IdleConnections.front( ) );
		m_IdleConnections.pop( );
	}

	if( m_OutstandingCallables > 0 )
		CONSOLE_Print( "[MYSQL] " + UTIL_ToString( m_OutstandingCallables ) + " outstanding callables were never recovered" );

	mysql_library_end( );
}

string CGHostDBMySQL :: GetStatus( )
{
	return "DB STATUS --- Connections: " + UTIL_ToString( m_IdleConnections.size( ) ) + "/" + UTIL_ToString( m_NumConnections ) + " idle. Outstanding callables: " + UTIL_ToString( m_OutstandingCallables ) + ".";
}

void CGHostDBMySQL :: RecoverCallable( CBaseCallable *callable )
{
	CMySQLCallable *MySQLCallable = dynamic_cast<CMySQLCallable *>( callable );

	if( MySQLCallable )
	{
		if( m_IdleConnections.size( ) > m_MaxConnections )
		{
			mysql_close( (MYSQL *)MySQLCallable->GetConnection( ) );
			m_NumConnections--;
		}
		else
			m_IdleConnections.push( MySQLCallable->GetConnection( ) );

		if( m_OutstandingCallables == 0 )
			CONSOLE_Print( "[MYSQL] recovered a mysql callable with zero outstanding" );
		else
			m_OutstandingCallables--;

		if( !MySQLCallable->GetError( ).empty( ) )
			CONSOLE_Print( "[MYSQL] error --- " + MySQLCallable->GetError( ) );
	}
	else
		CONSOLE_Print( "[MYSQL] tried to recover a non-mysql callable" );
}

void CGHostDBMySQL :: CreateThread( CBaseCallable *callable )
{
	try
	{
		boost :: thread Thread( boost :: ref( *callable ) );
	}
	catch( boost :: thread_resource_error tre )
	{
		CONSOLE_Print( "[MYSQL] error spawning thread on attempt #1 [" + string( tre.what( ) ) + "], pausing execution and trying again in 50ms" );
		MILLISLEEP( 50 );

		try
		{
			boost :: thread Thread( boost :: ref( *callable ) );
		}
		catch( boost :: thread_resource_error tre2 )
		{
			CONSOLE_Print( "[MYSQL] error spawning thread on attempt #2 [" + string( tre2.what( ) ) + "], giving up" );
			callable->SetReady( true );
		}
	}
}

CCallableAdminCount *CGHostDBMySQL :: ThreadedAdminCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminCount *Callable = new CMySQLCallableAdminCount( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminCheck *CGHostDBMySQL :: ThreadedAdminCheck( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminCheck *Callable = new CMySQLCallableAdminCheck( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminAdd *CGHostDBMySQL :: ThreadedAdminAdd( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminAdd *Callable = new CMySQLCallableAdminAdd( server, user, m_AdminAccess, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminRemove *CGHostDBMySQL :: ThreadedAdminRemove( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminRemove *Callable = new CMySQLCallableAdminRemove( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminList *CGHostDBMySQL :: ThreadedAdminList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminList *Callable = new CMySQLCallableAdminList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAccessesList *CGHostDBMySQL :: ThreadedAccessesList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAccessesList *Callable = new CMySQLCallableAccessesList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableSafeList *CGHostDBMySQL :: ThreadedSafeList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableSafeList *Callable = new CMySQLCallableSafeList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableSafeList *CGHostDBMySQL :: ThreadedSafeListV( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableSafeList *Callable = new CMySQLCallableSafeListV( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableSafeList *CGHostDBMySQL :: ThreadedNotes( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableSafeList *Callable = new CMySQLCallableNotes( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableSafeList *CGHostDBMySQL :: ThreadedNotesN( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableSafeList *Callable = new CMySQLCallableNotesN( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableRanks *CGHostDBMySQL :: ThreadedRanks( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableRanks *Callable = new CMySQLCallableRanks( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableSafeCheck *CGHostDBMySQL :: ThreadedSafeCheck( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableSafeCheck *Callable = new CMySQLCallableSafeCheck( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableSafeAdd *CGHostDBMySQL :: ThreadedSafeAdd( string server, string user, string voucher )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableSafeAdd *Callable = new CMySQLCallableSafeAdd( server, user, voucher, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableSafeRemove *CGHostDBMySQL :: ThreadedSafeRemove( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableSafeRemove *Callable = new CMySQLCallableSafeRemove( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableNoteAdd *CGHostDBMySQL :: ThreadedNoteAdd( string server, string user, string note )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableNoteAdd *Callable = new CMySQLCallableNoteAdd( server, user, note, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableNoteAdd *CGHostDBMySQL :: ThreadedNoteUpdate( string server, string user, string note )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableNoteAdd *Callable = new CMySQLCallableNoteUpdate( server, user, note, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableNoteRemove *CGHostDBMySQL :: ThreadedNoteRemove( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableNoteRemove *Callable = new CMySQLCallableNoteRemove( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableCalculateScores *CGHostDBMySQL :: ThreadedCalculateScores( string formula, string mingames )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableCalculateScores *Callable = new CMySQLCallableCalculateScores( formula, mingames, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableWarnUpdate *CGHostDBMySQL :: ThreadedWarnUpdate( string user, uint32_t before, uint32_t after )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableWarnUpdate *Callable = new CMySQLCallableWarnUpdate( user, before, after, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableWarnForget *CGHostDBMySQL :: ThreadedWarnForget( string user, uint32_t gamethreshold )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableWarnForget *Callable = new CMySQLCallableWarnForget( user, gamethreshold, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableTodayGamesCount *CGHostDBMySQL :: ThreadedTodayGamesCount( )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableTodayGamesCount *Callable = new CMySQLCallableTodayGamesCount( Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanCount *CGHostDBMySQL :: ThreadedBanCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanCount *Callable = new CMySQLCallableBanCount( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanCheck *CGHostDBMySQL :: ThreadedBanCheck( string server, string user, string ip, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanCheck *Callable = new CMySQLCallableBanCheck( server, user, ip, warn, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanAdd *CGHostDBMySQL :: ThreadedBanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanAdd *Callable = new CMySQLCallableBanAdd( server, user, ip, gamename, admin, reason, expiredaytime, warn, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanRemove *CGHostDBMySQL :: ThreadedBanRemove( string server, string user, string admin, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanRemove *Callable = new CMySQLCallableBanRemove( server, user, admin, warn, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanRemove *CGHostDBMySQL :: ThreadedBanRemove( string user, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanRemove *Callable = new CMySQLCallableBanRemove( string( ), user, string ( ), warn, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanList *CGHostDBMySQL :: ThreadedBanList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanList *Callable = new CMySQLCallableBanList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGameAdd *CGHostDBMySQL :: ThreadedGameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGameAdd *Callable = new CMySQLCallableGameAdd( server, map, gamename, ownername, duration, gamestate, creatorname, creatorserver, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGamePlayerAdd *CGHostDBMySQL :: ThreadedGamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGamePlayerAdd *Callable = new CMySQLCallableGamePlayerAdd( gameid, name, ip, spoofed, spoofedrealm, reserved, loadingtime, left, leftreason, team, colour, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGamePlayerSummaryCheck *CGHostDBMySQL :: ThreadedGamePlayerSummaryCheck( string name )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGamePlayerSummaryCheck *Callable = new CMySQLCallableGamePlayerSummaryCheck( name, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAGameAdd *CGHostDBMySQL :: ThreadedDotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAGameAdd *Callable = new CMySQLCallableDotAGameAdd( gameid, winner, min, sec, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAPlayerAdd *CGHostDBMySQL :: ThreadedDotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAPlayerAdd *Callable = new CMySQLCallableDotAPlayerAdd( gameid, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolour, towerkills, raxkills, courierkills, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAPlayerSummaryCheck *CGHostDBMySQL :: ThreadedDotAPlayerSummaryCheck( string name, string formula, string mingames, string gamestate )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAPlayerSummaryCheck *Callable = new CMySQLCallableDotAPlayerSummaryCheck( name, GetFormula(), UTIL_ToString(GetMinGames()), gamestate, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDownloadAdd *CGHostDBMySQL :: ThreadedDownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDownloadAdd *Callable = new CMySQLCallableDownloadAdd( map, mapsize, name, ip, spoofed, spoofedrealm, downloadtime, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableWarnCount *CGHostDBMySQL :: ThreadedWarnCount( string name, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableWarnCount *Callable = new CMySQLCallableWarnCount( name, warn, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanUpdate *CGHostDBMySQL :: ThreadedBanUpdate( string name, string ip )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanUpdate *Callable = new CMySQLCallableBanUpdate( name, ip, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableRunQuery *CGHostDBMySQL :: ThreadedRunQuery( string query )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableRunQuery *Callable = new CMySQLCallableRunQuery( query, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableScoreCheck *CGHostDBMySQL :: ThreadedScoreCheck( string category, string name, string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableScoreCheck *Callable = new CMySQLCallableScoreCheck( category, name, server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDPlayerAdd *CGHostDBMySQL :: ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDPlayerAdd *Callable = new CMySQLCallableW3MMDPlayerAdd( category, gameid, pid, name, flag, leaver, practicing, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_ints, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_reals, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_strings, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

void *CGHostDBMySQL :: GetIdleConnection( )
{
	void *Connection = NULL;

	if (m_IdleConnections.size()>m_NumConnections+10)
		CONSOLE_Print( "[MYSQL] closing " + UTIL_ToString( m_IdleConnections.size( )-m_NumConnections ) + "/" + UTIL_ToString( m_IdleConnections.size( )) + " idle MySQL connections" );

	while( m_IdleConnections.size()>m_NumConnections )
	{
		mysql_close( (MYSQL *)m_IdleConnections.front( ) );
		m_IdleConnections.pop( );
	}

	if( !m_IdleConnections.empty( ) )
	{
		Connection = m_IdleConnections.front( );
		m_IdleConnections.pop( );
	}

	return Connection;
}

//
// unprototyped global helper functions
//

string MySQLEscapeString( void *conn, string str )
{
	if (str.length()==0)
		return str;
	char *to = new char[str.size( ) * 2 + 1];
	unsigned long size = mysql_real_escape_string( (MYSQL *)conn, to, str.c_str( ), str.size( ) );
	string result( to, size );
	delete [] to;
	return result;
}

vector<string> MySQLFetchRow( MYSQL_RES *res )
{
	vector<string> Result;

	MYSQL_ROW Row = mysql_fetch_row( res );

	if( Row )
	{
		unsigned long *Lengths;
		Lengths = mysql_fetch_lengths( res );

		for( unsigned int i = 0; i < mysql_num_fields( res ); i++ )
		{
			if( Row[i] )
				Result.push_back( string( Row[i], Lengths[i] ) );
			else
				Result.push_back( string( ) );
		}
	}

	return Result;
}

//
// global helper functions
//

uint32_t MySQLTodayGamesCount( void *conn, string *error, uint32_t botid)
{
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM games WHERE TO_DAYS(datetime)=TO_DAYS(CURDATE())";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting games - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

uint32_t MySQLAdminCount( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM admins WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting admins [" + server + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

bool MySQLAdminCheck( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	uint32_t Access = 0;
	bool IsAdmin = false;
	string Query = "SELECT access FROM admins WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
			{
				Access = UTIL_ToUInt32((Row)[0]);
				IsAdmin = true;
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	
	return IsAdmin;
}

bool MySQLAdminAdd( void *conn, string *error, uint32_t botid, string server, string user, uint32_t defaccess )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	uint32_t acc = defaccess;
	string access = UTIL_ToString(acc);

	bool Success = false;
	string Query = "INSERT INTO admins ( botid, server, name, access ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscServer + "', '" + EscUser + "', " + access + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLAdminRemove( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM admins WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		uint32_t rows = (uint32_t) mysql_affected_rows((MYSQL *)conn);
		if (rows>0)
			Success = true;
	}

	return Success;
}

vector<string> MySQLRemoveBanListDay( void *conn, string *error, uint32_t botid, string server, uint32_t day)
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> RemoveTempBanList;
	string Query = "SELECT name FROM bans WHERE server ='"+ EscServer +"' AND date=DATE_SUB(CURDATE(),INTERVAL '"+UTIL_ToString(day)+"' DAY)";
	string Query2 = "DELETE FROM bans WHERE server ='"+ EscServer +"' AND date=DATE_SUB(CURDATE(),INTERVAL '"+UTIL_ToString(day)+"' DAY)";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				RemoveTempBanList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	if( mysql_real_query( (MYSQL *)conn, Query2.c_str( ), Query2.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{

	}
	return RemoveTempBanList;
}

vector<string> MySQLRemoveTempBanList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> RemoveTempBanList;
	string Query = "SELECT name FROM bans WHERE server ='"+ EscServer +"' AND expiredate=CURDATE()";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				RemoveTempBanList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return RemoveTempBanList;
}

vector<string> MySQLAdminList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> AdminList;
	string Query = "SELECT name FROM admins WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				AdminList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return AdminList;
}

uint32_t MySQLScoresCount( void *conn, string *error, string server )
{
//	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
//	string Query = "SELECT COUNT(*) FROM scores WHERE server='" + EscServer + "'";
	string Query = "SELECT COUNT(*) FROM scores";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting scores [" + server + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

uint32_t MySQLBanCount( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM bans WHERE server='" + EscServer + "' AND warn=0 AND (expiredate = '' OR TO_DAYS(expiredate)> TO_DAYS(CURDATE()))";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting bans [" + server + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

uint32_t MySQLBanCount( void *conn, string *error, uint32_t botid, string name, uint32_t warn )
{
	string EscName = MySQLEscapeString( conn, name );
	string EscWarn = MySQLEscapeString( conn, UTIL_ToString(warn));

	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM bans WHERE name='" + EscName + "' AND warn='"+ EscWarn +"' AND (expiredate = '' OR TO_DAYS(expiredate)> TO_DAYS(CURDATE()))";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting bans for [" + name + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

uint32_t MySQLBanCount( void *conn, string *error, uint32_t botid, string server, string name, uint32_t warn )
{
	string EscServer = MySQLEscapeString( conn, server );
	string EscName = MySQLEscapeString( conn, name );
	string EscWarn = MySQLEscapeString( conn, UTIL_ToString(warn));

	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM bans WHERE server='"+ EscServer+"' AND name='" + EscName + "' AND warn='"+ EscWarn +"' AND (expiredate = '' OR TO_DAYS(expiredate)> TO_DAYS(CURDATE()))";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting bans for [" + name + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

CDBBan *MySQLBanCheck( void *conn, string *error, uint32_t botid, string server, string user, string ip, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscWarn = MySQLEscapeString( conn, UTIL_ToString(warn));
	CDBBan *Ban = NULL;
	string Query;

	if( ip.empty( ) )
		Query = "SELECT name, ip, DATE(date), gamename, admin, reason, expiredate FROM bans WHERE server='" + EscServer + "' AND name='" + EscUser + "' AND warn='" + EscWarn + "' AND (TO_DAYS(expiredate)> TO_DAYS(CURDATE()) OR expiredate = \"\")";
	else
		Query = "SELECT name, ip, DATE(date), gamename, admin, reason, expiredate FROM bans WHERE ((server='" + EscServer + "' AND name='" + EscUser + "') OR ip='" + EscIP + "')  AND warn='" + EscWarn + "' AND (TO_DAYS(expiredate)> TO_DAYS(CURDATE()) OR expiredate = \"\")";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 7 )
				Ban = new CDBBan( server, Row[0], Row[1], Row[2], Row[3], Row[4], Row[5], Row[6] );
			/* else
				*error = "error checking ban [" + server + " : " + user + "] - row doesn't have 6 columns"; */

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Ban;
}

bool MySQLBanAdd( void *conn, string *error, uint32_t botid, string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscUser = MySQLEscapeString( conn, user );

	uint32_t NumOfGames = 0;
	string Query3 = "SELECT COUNT(*) FROM gameplayers WHERE name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query3.c_str( ), Query3.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				NumOfGames = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting player games [" + user + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}
	string EscServer = MySQLEscapeString( conn, server );
	string EscIP = MySQLEscapeString( conn, ip );
// don't ban people on GArena by ip
	if (ip == "127.0.0.1")
		EscIP = MySQLEscapeString( conn, string() );
	string EscGameName = MySQLEscapeString( conn, gamename );
	string EscAdmin = MySQLEscapeString( conn, admin );
	string EscReason = MySQLEscapeString( conn, reason );
	string EscWarn = MySQLEscapeString( conn, UTIL_ToString(warn));
//	string EscExpireDayTime = MySQLEscapeString( conn, UTIL_ToString(expiredaytime));
	string EscExpireDayTime = UTIL_ToString(expiredaytime);
	string EscGameCount = MySQLEscapeString( conn, UTIL_ToString(NumOfGames));
	bool Success = false;
	vector<string> ips;

// if we don't know the ip, search for this name in gameplayers
	if (false)
	if (ip==string())
	{
		string Query2 = "SELECT distinct ip from gameplayers where name='"+EscUser+"' AND spoofedrealm='"+EscServer+"'";
		if( mysql_real_query( (MYSQL *)conn, Query2.c_str( ), Query2.size( ) ) != 0 )
			*error = mysql_error( (MYSQL *)conn );
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				while( Row.size( ) == 1 )
				{
					ips.push_back( Row[0] );
					Row = MySQLFetchRow( Result );
				}
				mysql_free_result( Result );
			}
			else
				*error = mysql_error( (MYSQL *)conn );
		}				
	}
	string Query = string();
	if (ips.size()!=0 && warn!=0)
		EscIP = MySQLEscapeString( conn, ips[0] );
	if (ips.size()==0 || warn!=0)	
	{
		Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, warn, gamecount, expiredate  ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', CURDATE( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "', '" + EscWarn + "', '" + EscGameCount + "','' )";
		if (expiredaytime>0)
			Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, warn, gamecount, expiredate  ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', CURDATE( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "', '" + EscWarn + "', '" + EscGameCount + "', DATE_ADD(CURDATE(), INTERVAL "+EscExpireDayTime +" DAY))";

		if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
			*error = mysql_error( (MYSQL *)conn );
		else
			Success = true;
	} else
	{
		for (uint32_t i=0;i<ips.size();i++)
		{
			EscIP = MySQLEscapeString( conn, ips[i] );
//			Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, warn, gamecount ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', CURDATE( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "' )";
			Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, warn, gamecount, expiredate  ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', CURDATE( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "', '" + EscWarn + "', '" + EscGameCount + "','' )";
			if (expiredaytime>0)
				Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, warn, gamecount, expiredate  ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', CURDATE( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "', '" + EscWarn + "', '" + EscGameCount + "', DATE_ADD(CURDATE(), INTERVAL "+EscExpireDayTime +" DAY))";
			if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
				*error = mysql_error( (MYSQL *)conn );
			else
				Success = true;			
		}
	}
	return Success;
}

bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string server, string user, string admin, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscAdmin = MySQLEscapeString( conn, admin );
	string EscWarn = MySQLEscapeString( conn, UTIL_ToString(warn));
	bool Success = false;
	string Query = "DELETE FROM bans WHERE server='" + EscServer + "' AND name='" + EscUser + "'"+ "' AND warn='" + EscWarn + "'";
	if (!admin.empty())
		Query = "DELETE FROM bans WHERE server='" + EscServer + "' AND name='" + EscUser + "'" + "' AND warn='" + EscWarn + "'" + " AND admin='" + EscAdmin + "'";
	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		uint32_t rows = (uint32_t) mysql_affected_rows((MYSQL *)conn);
		if (rows>0)
			Success = true;
	}

	return Success;
}

bool MySQLBanUpdate( void *conn, string *error, string user, string ip  )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscUser = MySQLEscapeString( conn, user );
	string EscIP = MySQLEscapeString( conn, ip );
	bool Success = false;
	string Query = "UPDATE bans SET ip='"+EscIP+"' WHERE name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

string MySQLRunQuery( void *conn, string *error, string query)
{
	string Result = "OK ";

	if( mysql_real_query( (MYSQL *)conn, query.c_str( ), query.size( ) ) != 0 )
	{
		*error = mysql_error( (MYSQL *)conn );
		Result = *error;
	} else
	{
		MYSQL_RES *sqlResult = mysql_store_result( (MYSQL *)conn );

		if( sqlResult )
		{
			vector<string> Row = MySQLFetchRow( sqlResult );
			for (uint32_t i=0; i<Row.size(); i++)
			{
				Result += Row[i];
			}

			mysql_free_result( sqlResult );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Result;
}

bool MySQLWarnUpdate( void *conn, string *error, uint32_t botid, string user, uint32_t before, uint32_t after )
{
	// warns are set to 1 if active
	// 2 if inactive
	// 3 if inactive, but consider the ban of the player
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscUser = MySQLEscapeString( conn, user );
	string EscBefore = MySQLEscapeString( conn, UTIL_ToString(before) );
	string EscAfter = MySQLEscapeString( conn, UTIL_ToString(after) );
	bool Success = false;
	string Query = "UPDATE bans SET warn='"+ EscAfter +"' WHERE name='"+ EscUser +"' AND warn='"+ EscBefore  +"'";
	if(before <= 1) // either active warns or bans
		Query = "UPDATE bans SET warn='"+ EscAfter +"' WHERE name='"+ EscUser +"' AND warn='"+ EscBefore  +"' AND TO_DAYS(expiredate) > TO_DAYS(CURDATE())";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLWarnForget( void *conn, string *error, uint32_t botid, string name, uint32_t gamethreshold )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscUser = MySQLEscapeString( conn, name );
	string EscThreshold = MySQLEscapeString( conn, UTIL_ToString(gamethreshold) );

	uint32_t NumOfGames = 0;
	string Query3 = "SELECT COUNT(*) FROM gameplayers WHERE name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query3.c_str( ), Query3.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				NumOfGames = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting player games [" + name + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	uint32_t LastWarn = 9999;

	bool Success = false;

	if(NumOfGames >= gamethreshold)
	{
		string Query1 = "SELECT MAX(gamecount) FROM bans WHERE name='"+EscUser +"' AND warn='1' AND (TO_DAYS(expiredate) > TO_DAYS(CURDATE()) OR expiredate=\"\")";

		if( mysql_real_query( (MYSQL *)conn, Query1.c_str( ), Query1.size( ) ) != 0 )
			*error = mysql_error( (MYSQL *)conn );
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				if( Row.size( ) == 1 )
					LastWarn = UTIL_ToUInt32( Row[0] );
				else
					*error = "error checking game number of the gameplayer - [" + name + "] - row doesn't have 1 column";

				mysql_free_result( Result );
			}
			else
				*error = mysql_error( (MYSQL *)conn );
		}

	}

	if(LastWarn != 9999 && (NumOfGames - LastWarn) % gamethreshold == 0)
	{
		// delete the oldest of the player's active warns
		string Query4 = "SELECT id FROM bans WHERE name='"+ EscUser+"' AND warn = 1 AND (TO_DAYS(expiredate)>TO_DAYS(CURDATE()) OR expiredate=\"\") ORDER BY id ASC LIMIT 1";

		uint32_t ID = 0;
		if( mysql_real_query( (MYSQL *)conn, Query4.c_str( ), Query4.size( ) ) != 0 )
			*error = mysql_error( (MYSQL *)conn );
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				if( Row.size( ) == 1 )
					ID = UTIL_ToUInt32( Row[0] );

				mysql_free_result( Result );
			}
			else
				*error = mysql_error( (MYSQL *)conn );

			if (ID!=0)
			{
				Success = false;
				string EscID = MySQLEscapeString( conn, UTIL_ToString(ID) );
				string Query5 = "DELETE FROM bans WHERE id='"+EscID+"'";

				if( mysql_real_query( (MYSQL *)conn, Query5.c_str( ), Query5.size( ) ) != 0 )
					*error = mysql_error( (MYSQL *)conn );
				else
					Success = true;
			}
		}

		Success = true;
	}
	return Success;
}

bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string user, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscUser = MySQLEscapeString( conn, user );
	string EscWarn = MySQLEscapeString( conn, UTIL_ToString(warn));
	bool Success = false;
	string Query = "DELETE FROM bans WHERE name='" + EscUser + "'" + " AND warn='" + EscWarn + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		uint32_t rows = (uint32_t) mysql_affected_rows((MYSQL *)conn);
		if (rows>0)
			Success = true;
	}

	return Success;
}

uint32_t MySQLRemoveTempBans( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
	string Query = "DELETE FROM bans WHERE server='"+EscServer+"' AND expiredate=CURDATE()";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		uint32_t rows = (uint32_t) mysql_affected_rows((MYSQL *)conn);
		Count = rows;
	}

	return Count;
}

vector<CDBBan *> MySQLBanList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<CDBBan *> BanList;
	string Query = "SELECT name, ip, DATE(date), gamename, admin, reason, expiredate FROM bans WHERE warn='0' AND server='" + EscServer + "' ORDER BY name";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( Row.size( ) == 7 )
			{
				BanList.push_back( new CDBBan( server, Row[0], Row[1], Row[2], Row[3], Row[4], Row[5], Row[6] ) );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return BanList;
}

uint32_t MySQLGameAdd( void *conn, string *error, uint32_t botid, string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	uint32_t RowID = 0;
	string EscServer = MySQLEscapeString( conn, server );
	string EscMap = MySQLEscapeString( conn, map );
	string EscGameName = MySQLEscapeString( conn, gamename );
	string EscOwnerName = MySQLEscapeString( conn, ownername );
	string EscCreatorName = MySQLEscapeString( conn, creatorname );
	string EscCreatorServer = MySQLEscapeString( conn, creatorserver );
	string Query = "INSERT INTO games ( botid, server, map, datetime, gamename, ownername, duration, gamestate, creatorname, creatorserver ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscServer + "', '" + EscMap + "', NOW( ), '" + EscGameName + "', '" + EscOwnerName + "', " + UTIL_ToString( duration ) + ", " + UTIL_ToString( gamestate ) + ", '" + EscCreatorName + "', '" + EscCreatorServer + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = (uint32_t)mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

uint32_t MySQLGamePlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	uint32_t RowID = 0;
	string EscName = MySQLEscapeString( conn, name );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscSpoofedRealm = MySQLEscapeString( conn, spoofedrealm );
	string EscLeftReason = MySQLEscapeString( conn, leftreason );
	string Query = "INSERT INTO gameplayers ( botid, gameid, name, ip, spoofed, reserved, loadingtime, `left`, leftreason, team, colour, spoofedrealm ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", '" + EscName + "', '" + EscIP + "', " + UTIL_ToString( spoofed ) + ", " + UTIL_ToString( reserved ) + ", " + UTIL_ToString( loadingtime ) + ", " + UTIL_ToString( left ) + ", '" + EscLeftReason + "', " + UTIL_ToString( team ) + ", " + UTIL_ToString( colour ) + ", '" + EscSpoofedRealm + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = (uint32_t)mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

CDBGamePlayerSummary *MySQLGamePlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscName = MySQLEscapeString( conn, name );
	CDBGamePlayerSummary *GamePlayerSummary = NULL;
	string Query = "SELECT MIN(DATE(datetime)), MAX(DATE(datetime)), COUNT(*), MIN(loadingtime), AVG(loadingtime), MAX(loadingtime), MIN(`left`/duration)*100, AVG(`left`/duration)*100, MAX(`left`/duration)*100, MIN(duration), AVG(duration), MAX(duration) FROM gameplayers LEFT JOIN games ON games.id=gameid WHERE name='" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 12 )
			{
				string FirstGameDateTime = Row[0];
				string LastGameDateTime = Row[1];
				uint32_t TotalGames = UTIL_ToUInt32( Row[2] );
				uint32_t MinLoadingTime = UTIL_ToUInt32( Row[3] );
				uint32_t AvgLoadingTime = UTIL_ToUInt32( Row[4] );
				uint32_t MaxLoadingTime = UTIL_ToUInt32( Row[5] );
				uint32_t MinLeftPercent = UTIL_ToUInt32( Row[6] );
				uint32_t AvgLeftPercent = UTIL_ToUInt32( Row[7] );
				uint32_t MaxLeftPercent = UTIL_ToUInt32( Row[8] );
				uint32_t MinDuration = UTIL_ToUInt32( Row[9] );
				uint32_t AvgDuration = UTIL_ToUInt32( Row[10] );
				uint32_t MaxDuration = UTIL_ToUInt32( Row[11] );
				GamePlayerSummary = new CDBGamePlayerSummary( string( ), name, FirstGameDateTime, LastGameDateTime, TotalGames, MinLoadingTime, AvgLoadingTime, MaxLoadingTime, MinLeftPercent, AvgLeftPercent, MaxLeftPercent, MinDuration, AvgDuration, MaxDuration );
			}
			else
				*error = "error checking gameplayersummary [" + name + "] - row doesn't have 12 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return GamePlayerSummary;
}

uint32_t MySQLDotAGameAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	uint32_t RowID = 0;
	string Query = "INSERT INTO dotagames ( botid, gameid, winner, min, sec ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( winner ) + ", " + UTIL_ToString( min ) + ", " + UTIL_ToString( sec ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = (uint32_t)mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

uint32_t MySQLDotAPlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills )
{
	uint32_t RowID = 0;
	string EscItem1 = MySQLEscapeString( conn, item1 );
	string EscItem2 = MySQLEscapeString( conn, item2 );
	string EscItem3 = MySQLEscapeString( conn, item3 );
	string EscItem4 = MySQLEscapeString( conn, item4 );
	string EscItem5 = MySQLEscapeString( conn, item5 );
	string EscItem6 = MySQLEscapeString( conn, item6 );
	string EscHero = MySQLEscapeString( conn, hero );
	string Query = "INSERT INTO dotaplayers ( botid, gameid, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolour, towerkills, raxkills, courierkills ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( colour ) + ", " + UTIL_ToString( kills ) + ", " + UTIL_ToString( deaths ) + ", " + UTIL_ToString( creepkills ) + ", " + UTIL_ToString( creepdenies ) + ", " + UTIL_ToString( assists ) + ", " + UTIL_ToString( gold ) + ", " + UTIL_ToString( neutralkills ) + ", '" + EscItem1 + "', '" + EscItem2 + "', '" + EscItem3 + "', '" + EscItem4 + "', '" + EscItem5 + "', '" + EscItem6 + "', '" + EscHero + "', " + UTIL_ToString( newcolour ) + ", " + UTIL_ToString( towerkills ) + ", " + UTIL_ToString( raxkills ) + ", " + UTIL_ToString( courierkills ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = (uint32_t)mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

CDBDotAPlayerSummary *MySQLDotAPlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name, string formula, string mingames, string gamestate )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscName = MySQLEscapeString( conn, name );
	uint32_t Rank = 0;
	double Score = -10000;
	string server = string();
	CDBDotAPlayerSummary *DotAPlayerSummary = NULL;
//	string Query = "select totgames,wins,losses,killstotal,deathstotal,creepkillstotal,creepdeniestotal,assiststotal,neutralkillstotal,towerkillstotal,raxkillstotal,courierkillstotal,kills,deaths,creepkills,creepdenies,assists,neutralkills,towerkills,raxkills,courierkills, ("+formula+") as totalscore,server from(select *, (kills/deaths) as killdeathratio, (totgames-wins) as losses from (select gp.name as name,ga.server as server,gp.gameid as gameid, gp.colour as colour, avg(dp.courierkills) as courierkills, sum(dp.raxkills) as raxkillstotal, sum(dp.towerkills) as towerkillstotal, sum(dp.assists) as assiststotal,sum(dp.courierkills) as courierkillstotal, sum(dp.creepdenies) as creepdeniestotal, sum(dp.creepkills) as creepkillstotal,sum(dp.neutralkills) as neutralkillstotal, sum(dp.deaths) as deathstotal, sum(dp.kills) as killstotal,avg(dp.raxkills) as raxkills,avg(dp.towerkills) as towerkills, avg(dp.assists) as assists, avg(dp.creepdenies) as creepdenies, avg(dp.creepkills) as creepkills,avg(dp.neutralkills) as neutralkills, avg(dp.deaths) as deaths, avg(dp.kills) as kills,count(*) as totgames, SUM(case when((dg.winner = 1 and dp.newcolour < 6) or (dg.winner = 2 and dp.newcolour > 6)) then 1 else 0 end) as wins from gameplayers as gp, dotagames as dg, games as ga,dotaplayers as dp where dg.winner <> 0 and dp.gameid = gp.gameid and dg.gameid = dp.gameid and dp.gameid = ga.id and gp.gameid = dg.gameid and gp.colour = dp.colour and gp.name='"+name+"' group by gp.name) as h) as i";
	string Query = "select totgames,wins,losses,killstotal,deathstotal,creepkillstotal,creepdeniestotal,assiststotal,neutralkillstotal,towerkillstotal,raxkillstotal,courierkillstotal,kills,deaths,creepkills,creepdenies,assists,neutralkills,towerkills,raxkills,courierkills, server from(select *, (kills/deaths) as killdeathratio, (totgames-wins) as losses from (select gp.name as name,ga.server as server,gp.gameid as gameid, gp.colour as colour, avg(dp.courierkills) as courierkills, sum(dp.raxkills) as raxkillstotal, sum(dp.towerkills) as towerkillstotal, sum(dp.assists) as assiststotal,sum(dp.courierkills) as courierkillstotal, sum(dp.creepdenies) as creepdeniestotal, sum(dp.creepkills) as creepkillstotal,sum(dp.neutralkills) as neutralkillstotal, sum(dp.deaths) as deathstotal, sum(dp.kills) as killstotal,avg(dp.raxkills) as raxkills,avg(dp.towerkills) as towerkills, avg(dp.assists) as assists, avg(dp.creepdenies) as creepdenies, avg(dp.creepkills) as creepkills,avg(dp.neutralkills) as neutralkills, avg(dp.deaths) as deaths, avg(dp.kills) as kills,count(*) as totgames, SUM(case when((dg.winner = 1 and dp.newcolour < 6) or (dg.winner = 2 and dp.newcolour > 6)) then 1 else 0 end) as wins from gameplayers as gp, dotagames as dg, games as ga,dotaplayers as dp where dg.winner <> 0 and dp.gameid = gp.gameid and dg.gameid = dp.gameid and dp.gameid = ga.id and gp.gameid = dg.gameid and gp.colour = dp.colour and gp.name='"+name+"' group by gp.name) as h) as i";
	if (gamestate!="")
		Query = "select totgames,wins,losses,killstotal,deathstotal,creepkillstotal,creepdeniestotal,assiststotal,neutralkillstotal,towerkillstotal,raxkillstotal,courierkillstotal,kills,deaths,creepkills,creepdenies,assists,neutralkills,towerkills,raxkills,courierkills, server from(select *, (kills/deaths) as killdeathratio, (totgames-wins) as losses from (select gp.name as name,ga.server as server,gp.gameid as gameid, gp.colour as colour, avg(dp.courierkills) as courierkills, sum(dp.raxkills) as raxkillstotal, sum(dp.towerkills) as towerkillstotal, sum(dp.assists) as assiststotal,sum(dp.courierkills) as courierkillstotal, sum(dp.creepdenies) as creepdeniestotal, sum(dp.creepkills) as creepkillstotal,sum(dp.neutralkills) as neutralkillstotal, sum(dp.deaths) as deathstotal, sum(dp.kills) as killstotal,avg(dp.raxkills) as raxkills,avg(dp.towerkills) as towerkills, avg(dp.assists) as assists, avg(dp.creepdenies) as creepdenies, avg(dp.creepkills) as creepkills,avg(dp.neutralkills) as neutralkills, avg(dp.deaths) as deaths, avg(dp.kills) as kills,count(*) as totgames, SUM(case when((dg.winner = 1 and dp.newcolour < 6) or (dg.winner = 2 and dp.newcolour > 6)) then 1 else 0 end) as wins from gameplayers as gp, dotagames as dg, games as ga,dotaplayers as dp where dg.winner <> 0 and dp.gameid = gp.gameid and dg.gameid = dp.gameid and dp.gameid = ga.id and gp.gameid = dg.gameid and gp.colour = dp.colour and gp.name='"+name+"' and ga.gamestate='"+ gamestate +"' group by gp.name) as h) as i";
	
	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 22 )
			{
				uint32_t TotalGames = UTIL_ToUInt32( Row[0] );

				if( TotalGames > 0 )
				{
					uint32_t TotalWins = UTIL_ToUInt32( Row[1] );
					uint32_t TotalLosses = UTIL_ToUInt32( Row[2] );
					uint32_t TotalKills = UTIL_ToUInt32( Row[3] );
					uint32_t TotalDeaths = UTIL_ToUInt32( Row[4] );
					uint32_t TotalCreepKills = UTIL_ToUInt32( Row[5] );
					uint32_t TotalCreepDenies = UTIL_ToUInt32( Row[6] );
					uint32_t TotalAssists = UTIL_ToUInt32( Row[7] );
					uint32_t TotalNeutralKills = UTIL_ToUInt32( Row[8] );
					uint32_t TotalTowerKills = UTIL_ToUInt32( Row[9] );
					uint32_t TotalRaxKills = UTIL_ToUInt32( Row[10] );
					uint32_t TotalCourierKills = UTIL_ToUInt32( Row[11] );

					double wpg = 0;
					double lpg = 0;
					double kpg = (double)TotalKills/TotalGames;
					double dpg = (double)TotalDeaths/TotalGames;
					double ckpg = (double)TotalCreepKills/TotalGames;
					double cdpg = (double)TotalCreepDenies/TotalGames;
					double apg = (double)TotalAssists/TotalGames;
					double nkpg = (double)TotalNeutralKills/TotalGames;
					double tkpg = (double)TotalTowerKills/TotalGames;
					double rkpg = (double)TotalRaxKills/TotalGames;
					double coukpg = (double)TotalCourierKills/TotalGames;
					server = Row[21];
//					double Score = UTIL_ToDouble( Row[21] );
					uint32_t Rank = 0;
					wpg = (double)TotalWins/TotalGames;
					lpg = (double)TotalLosses/TotalGames;
					wpg = wpg * 100;
					lpg = lpg * 100;
/*
					if (TotalGames>=UTIL_ToUInt32(mingames))
					{
						string Query4 = "DELETE FROM scores WHERE name='"+name+"'";
						mysql_real_query( (MYSQL *)conn, Query4.c_str( ), Query4.size( ) );

						
						string Query3 = "INSERT INTO scores (category,server,name,score) VALUES ('dota_elo','"+server+"','"+name+"',"+UTIL_ToString(Score)+ ")";
						if( mysql_real_query( (MYSQL *)conn, Query3.c_str( ), Query3.size( ) ) != 0 )
							*error = mysql_error( (MYSQL *)conn );
						else
						{
						}

						CDBScoreSummary *DotAScoreSummary = ScoreCheck((MYSQL *)conn, error, name);
						if (DotAScoreSummary)
						{
							Rank = DotAScoreSummary->GetRank();
						}
						delete DotAScoreSummary;
					}
*/
					CDBScoreSummary *DotAScoreSummary = ScoreCheck((MYSQL *)conn, error, botid, name);
					if (DotAScoreSummary)
					{
						if (TotalGames>=UTIL_ToUInt32(mingames))
							Rank = DotAScoreSummary->GetRank();
						Score = DotAScoreSummary->GetScore();
					}
					delete DotAScoreSummary;
					DotAPlayerSummary = new CDBDotAPlayerSummary( string( ), name, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills, wpg,lpg, kpg, dpg, ckpg, cdpg, apg, nkpg, Score, tkpg, rkpg, coukpg, Rank );
				}
			}
			else;
//				*error = "error checking dotaplayersummary [" + name + "] - row doesn't have 23 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}


/*
	string Query = "SELECT COUNT(dotaplayers.id), SUM(kills), SUM(deaths), SUM(creepkills), SUM(creepdenies), SUM(assists), SUM(neutralkills), SUM(towerkills), SUM(raxkills), SUM(courierkills) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON dotagames.gameid=games.id WHERE name='" + EscName + "' AND winner!=0";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 10 )
			{
				uint32_t TotalGames = UTIL_ToUInt32( Row[0] );

				if( TotalGames > 0 )
				{
					uint32_t TotalWins = 0;
					uint32_t TotalLosses = 0;
					uint32_t TotalKills = UTIL_ToUInt32( Row[1] );
					uint32_t TotalDeaths = UTIL_ToUInt32( Row[2] );
					uint32_t TotalCreepKills = UTIL_ToUInt32( Row[3] );
					uint32_t TotalCreepDenies = UTIL_ToUInt32( Row[4] );
					uint32_t TotalAssists = UTIL_ToUInt32( Row[5] );
					uint32_t TotalNeutralKills = UTIL_ToUInt32( Row[6] );
					uint32_t TotalTowerKills = UTIL_ToUInt32( Row[7] );
					uint32_t TotalRaxKills = UTIL_ToUInt32( Row[8] );
					uint32_t TotalCourierKills = UTIL_ToUInt32( Row[9] );

					// calculate total wins

					string Query2 = "SELECT COUNT(*) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON games.id=dotagames.gameid WHERE name='" + EscName + "' AND ((winner=1 AND dotaplayers.newcolour>=1 AND dotaplayers.newcolour<=5) OR (winner=2 AND dotaplayers.newcolour>=7 AND dotaplayers.newcolour<=11))";

					if( mysql_real_query( (MYSQL *)conn, Query2.c_str( ), Query2.size( ) ) != 0 )
						*error = mysql_error( (MYSQL *)conn );
					else
					{
						MYSQL_RES *Result2 = mysql_store_result( (MYSQL *)conn );

						if( Result2 )
						{
							vector<string> Row2 = MySQLFetchRow( Result2 );

							if( Row2.size( ) == 1 )
								TotalWins = UTIL_ToUInt32( Row2[0] );
							else
								*error = "error checking dotaplayersummary wins [" + name + "] - row doesn't have 1 column";

							mysql_free_result( Result2 );
						}
						else
							*error = mysql_error( (MYSQL *)conn );
					}

					// calculate total losses

					string Query3 = "SELECT COUNT(*) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON games.id=dotagames.gameid WHERE name='" + EscName + "' AND ((winner=2 AND dotaplayers.newcolour>=1 AND dotaplayers.newcolour<=5) OR (winner=1 AND dotaplayers.newcolour>=7 AND dotaplayers.newcolour<=11))";

					if( mysql_real_query( (MYSQL *)conn, Query3.c_str( ), Query3.size( ) ) != 0 )
						*error = mysql_error( (MYSQL *)conn );
					else
					{
						MYSQL_RES *Result3 = mysql_store_result( (MYSQL *)conn );

						if( Result3 )
						{
							vector<string> Row3 = MySQLFetchRow( Result3 );

							if( Row3.size( ) == 1 )
								TotalLosses = UTIL_ToUInt32( Row3[0] );
							else
								*error = "error checking dotaplayersummary losses [" + name + "] - row doesn't have 1 column";

							mysql_free_result( Result3 );
						}
						else
							*error = mysql_error( (MYSQL *)conn );
					}

					TotalGames = TotalWins + TotalLosses;

					if (TotalGames !=0)
					{
						double Score = 0;
						double Score1 = 0;
						double Score2 = 0;
						double Score3 = 0;
						double wpg = 0;
						double lpg = 0;
						double kpg = (double)TotalKills/TotalGames;
						double dpg = (double)TotalDeaths/TotalGames;
						double ckpg = (double)TotalCreepKills/TotalGames;
						double cdpg = (double)TotalCreepDenies/TotalGames;
						double apg = (double)TotalAssists/TotalGames;
						double nkpg = (double)TotalNeutralKills/TotalGames;
						double tkpg = (double)TotalTowerKills/TotalGames;
						double rkpg = (double)TotalRaxKills/TotalGames;
						double coukpg = (double)TotalCourierKills/TotalGames;


						// done

						wpg = (double)TotalWins/TotalGames;
						lpg = (double)TotalLosses/TotalGames;

						//[[W-L] : played games]+kpg-dpg+[apg:10]+[ckpg:100]+[cdpg:10]+[nkpg:50]
//						Score1 = (double)(TotalWins - TotalLosses)/TotalGames*5;
//						Score2 = ((double)((double)(TotalKills-TotalDeaths+TotalAssists)/4)/TotalGames);
//						Score3 = ((double)((double)(TotalCreepKills/50)+(double)(TotalCreepDenies/5)+(double)(TotalNeutralKills/30))/TotalGames);
						Score1 = TotalWins;
						Score1 = Score1 - (double)TotalLosses;
						Score1 = Score1/TotalGames;
						Score2 = kpg-dpg+apg/10;
						Score3 = ckpg/100+cdpg/10+nkpg/50;

						Score = Score1+Score2+Score3;

						wpg = wpg * 100;
						lpg = lpg * 100;

						DotAPlayerSummary = new CDBDotAPlayerSummary( string( ), name, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills, wpg,lpg, kpg, dpg, ckpg, cdpg, apg, nkpg, Score, tkpg, rkpg, coukpg, Rank );
					}

					// done

//					DotAPlayerSummary = new CDBDotAPlayerSummary( string( ), name, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills );
				}
			}
			else
				*error = "error checking dotaplayersummary [" + name + "] - row doesn't have 10 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}
*/
	return DotAPlayerSummary;
}

bool MySQLCalculateScores( void *conn, string *error, uint32_t botid, string formula, string mingames )
{
	bool Success = false;
	
	string Query = "select name,server, (" + formula + ") as totalscore from(select *, (kills/deaths) as killdeathratio, (totgames-wins) as losses from (select gp.name as name,ga.server as server,gp.gameid as gameid, gp.colour as colour, avg(dp.courierkills) as courierkills, sum(dp.raxkills) as raxkillstotal, sum(dp.towerkills) as towerkillstotal, sum(dp.assists) as assiststotal, sum(dp.courierkills) as courierkillstotal, sum(dp.creepdenies) as creepdeniestotal, sum(dp.creepkills) as creepkillstotal, sum(dp.neutralkills) as neutralkillstotal, sum(dp.deaths) as deathstotal, sum(dp.kills) as killstotal, avg(dp.raxkills) as raxkills,avg(dp.towerkills) as towerkills, avg(dp.assists) as assists, avg(dp.creepdenies) as creepdenies, avg(dp.creepkills) as creepkills,avg(dp.neutralkills) as neutralkills, avg(dp.deaths) as deaths, avg(dp.kills) as kills,count(*) as totgames, SUM(case when((dg.winner = 1 and dp.newcolour < 6) or (dg.winner = 2 and dp.newcolour > 6)) then 1 else 0 end) as wins from gameplayers as gp, dotagames as dg, games as ga,dotaplayers as dp where dg.winner <> 0 and dp.gameid = gp.gameid and dg.gameid = dp.gameid and dp.gameid = ga.id and gp.gameid = dg.gameid and gp.colour = dp.colour";
	Query+=" group by gp.name having totgames >= " + mingames + ") as h) as i ORDER BY totalscore desc, name asc";
	string Query2;
	string Query3 = "DELETE from scores";
	string name;
	string server;
	double Score = -100000.0;

	vector<string> names;
	vector<double> scores;
	vector<string> servers;

	if( mysql_real_query( (MYSQL *)conn, Query3.c_str( ), Query3.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		Success = true;
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( Row.size( ) == 3 )
			{
				names.push_back(MySQLEscapeString( conn, Row[0] ));
				servers.push_back(MySQLEscapeString( conn, Row[1]));
				scores.push_back(UTIL_ToDouble( Row[2] ));
				Row = MySQLFetchRow( Result );
			}
			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}
	if (Success)
	for (uint32_t i = 0; i<names.size();i++)
	{
		Query2 = "INSERT into scores (category,name,server,score) VALUES ('dota_elo','"+names[i]+"','"+servers[i]+"',"+UTIL_ToString(scores[i],6)+")";
		if( mysql_real_query( (MYSQL *)conn, Query2.c_str( ), Query2.size( ) ) != 0 )
			*error = mysql_error( (MYSQL *)conn );
	}
	names.clear();
	servers.clear();
	scores.clear();
	return Success;
}

bool MySQLDownloadAdd( void *conn, string *error, uint32_t botid, string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	bool Success = false;
	string EscMap = MySQLEscapeString( conn, map );
	string EscName = MySQLEscapeString( conn, name );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscSpoofedRealm = MySQLEscapeString( conn, spoofedrealm );
	string Query = "INSERT INTO downloads ( botid, map, mapsize, datetime, name, ip, spoofed, spoofedrealm, downloadtime ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscMap + "', " + UTIL_ToString( mapsize ) + ", NOW( ), '" + EscName + "', '" + EscIP + "', " + UTIL_ToString( spoofed ) + ", '" + EscSpoofedRealm + "', " + UTIL_ToString( downloadtime ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

double MySQLScoreCheck( void *conn, string *error, uint32_t botid, string category, string name, string server )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscCategory = MySQLEscapeString( conn, category );
	string EscName = MySQLEscapeString( conn, name );
	string EscServer = MySQLEscapeString( conn, server );
	double Score = -100000.0;
	string Query = "SELECT score FROM scores WHERE category='" + EscCategory + "' AND name='" + EscName + "' AND server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Score = UTIL_ToDouble( Row[0] );
			/* else
				*error = "error checking score [" + category + " : " + name + " : " + server + "] - row doesn't have 1 column"; */

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Score;
}

uint32_t MySQLW3MMDPlayerAdd( void *conn, string *error, uint32_t botid, string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t RowID = 0;
	string EscCategory = MySQLEscapeString( conn, category );
	string EscName = MySQLEscapeString( conn, name );
	string EscFlag = MySQLEscapeString( conn, flag );
	string Query = "INSERT INTO w3mmdplayers ( botid, category, gameid, pid, name, flag, leaver, practicing ) VALUES ( " + UTIL_ToString( botid ) +", '" + EscCategory + "', " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( pid ) + ", '" + EscName + "', '" + EscFlag + "', " + UTIL_ToString( leaver ) + ", " + UTIL_ToString( practicing ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = (uint32_t)mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,int32_t> var_ints )
{
	if( var_ints.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,int32_t> :: iterator i = var_ints.begin( ); i != var_ints.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );

		if( Query.empty( ) )
			Query = "INSERT INTO w3mmdvars ( botid, gameid, pid, varname, value_int ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second ) + " )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second ) + " )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,double> var_reals )
{
	if( var_reals.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,double> :: iterator i = var_reals.begin( ); i != var_reals.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );

		if( Query.empty( ) )
			Query = "INSERT INTO w3mmdvars ( botid, gameid, pid, varname, value_real ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second, 10 ) + " )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second, 10 ) + " )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,string> var_strings )
{
	if( var_strings.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,string> :: iterator i = var_strings.begin( ); i != var_strings.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );
		string EscValueString = MySQLEscapeString( conn, i->second );

		if( Query.empty( ) )
			Query = "INSERT INTO w3mmdvars ( botid, gameid, pid, varname, value_string ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', '" + EscValueString + "' )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', '" + EscValueString + "' )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

//
// MySQL Callables
//

void CMySQLCallable :: Init( )
{
	CBaseCallable :: Init( );

#ifndef WIN32
	// disable SIGPIPE since this is (or should be) a new thread and it doesn't inherit the spawning thread's signal handlers
	// MySQL should automatically disable SIGPIPE when we initialize it but we do so anyway here

	signal( SIGPIPE, SIG_IGN );
#endif

	mysql_thread_init( );

	if( !m_Connection )
	{
		if( !( m_Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)m_Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)m_Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)m_Connection, m_SQLServer.c_str( ), m_SQLUser.c_str( ), m_SQLPassword.c_str( ), m_SQLDatabase.c_str( ), m_SQLPort, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)m_Connection );
	}
	else if( mysql_ping( (MYSQL *)m_Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)m_Connection );
}

void CMySQLCallable :: Close( )
{
	mysql_thread_end( );

	CBaseCallable :: Close( );
}

void CMySQLCallableAdminCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminCount( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableAdminCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminCheck( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_AdminAccess );

	Close( );
}

void CMySQLCallableAdminRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableAccessesList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAccessesList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableSafeList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLSafeList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableSafeListV :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLSafeListV( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableNotes :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLNotes( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableNotesN :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLNotesN( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableRanks :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLRanks( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableSafeCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLSafeCheck( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User);

	Close( );
}

void CMySQLCallableSafeAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLSafeAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_Voucher );

	Close( );
}

void CMySQLCallableSafeRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLSafeRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableNoteAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLNoteAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_Note );

	Close( );
}

void CMySQLCallableNoteUpdate :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLNoteUpdate( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_Note );

	Close( );
}

void CMySQLCallableNoteRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLNoteRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableCalculateScores :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLCalculateScores( m_Connection, &m_Error, m_SQLBotID, m_Formula, m_MinGames );

	Close( );
}

void CMySQLCallableWarnUpdate :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLWarnUpdate( m_Connection, &m_Error, m_SQLBotID, m_User, m_Before, m_After );

	Close( );
}

void CMySQLCallableWarnForget :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLWarnForget( m_Connection, &m_Error, m_SQLBotID, m_User, m_GameThreshold );

	Close( );
}

void CMySQLCallableTodayGamesCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLTodayGamesCount( m_Connection, &m_Error, m_SQLBotID );

	Close( );
}

void CMySQLCallableBanCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanCount( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableBanCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanCheck( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_IP, m_Warn );

	Close( );
}

void CMySQLCallableBanAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_IP, m_GameName, m_Admin, m_Reason, m_ExpireDayTime, m_Warn );

	Close( );
}

void CMySQLCallableBanRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		string Admin = string();
		if (!m_Admin.empty())
			Admin = m_Admin;

		if( m_Server.empty( ) && m_Admin.empty() )
			m_Result = MySQLBanRemove( m_Connection, &m_Error, m_SQLBotID, m_User, m_Warn );
		else
			m_Result = MySQLBanRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, Admin, m_Warn );
	}

	Close( );
}

void CMySQLCallableBanList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableGameAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGameAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_Map, m_GameName, m_OwnerName, m_Duration, m_GameState, m_CreatorName, m_CreatorServer );

	Close( );
}

void CMySQLCallableGamePlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGamePlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Name, m_IP, m_Spoofed, m_SpoofedRealm, m_Reserved, m_LoadingTime, m_Left, m_LeftReason, m_Team, m_Colour );

	Close( );
}

void CMySQLCallableGamePlayerSummaryCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGamePlayerSummaryCheck( m_Connection, &m_Error, m_SQLBotID, m_Name );

	Close( );
}

void CMySQLCallableDotAGameAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAGameAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Winner, m_Min, m_Sec );

	Close( );
}

void CMySQLCallableDotAPlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAPlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Colour, m_Kills, m_Deaths, m_CreepKills, m_CreepDenies, m_Assists, m_Gold, m_NeutralKills, m_Item1, m_Item2, m_Item3, m_Item4, m_Item5, m_Item6, m_Hero, m_NewColour, m_TowerKills, m_RaxKills, m_CourierKills );

	Close( );
}

void CMySQLCallableDotAPlayerSummaryCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAPlayerSummaryCheck( m_Connection, &m_Error, m_SQLBotID, m_Name, m_Formula, m_MinGames, m_GameState );

	Close( );
}

void CMySQLCallableDownloadAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDownloadAdd( m_Connection, &m_Error, m_SQLBotID, m_Map, m_MapSize, m_Name, m_IP, m_Spoofed, m_SpoofedRealm, m_DownloadTime );

	Close( );
}

void CMySQLCallableWarnCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanCount( m_Connection, &m_Error, m_SQLBotID, m_Name, m_Warn );

	Close( );
}

void CMySQLCallableBanUpdate :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanUpdate( m_Connection, &m_Error, m_Name, m_IP );

	Close( );
}

void CMySQLCallableRunQuery :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLRunQuery( m_Connection, &m_Error, m_Query );

	Close( );
}

void CMySQLCallableScoreCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		CDBScoreSummary *DotAScoreSummary = ScoreCheck(m_Connection, &m_Error, m_SQLBotID, m_Category, m_Name, m_Server);

		m_Rank = 0;
		m_Result = 0;
		if (DotAScoreSummary)
		{
			m_Rank = DotAScoreSummary->GetRank();
			m_Result = DotAScoreSummary->GetScore();
		}
		delete DotAScoreSummary;
//		m_Result = MySQLScoreCheck( m_Connection, &m_Error, m_SQLBotID, m_Category, m_Name, m_Server );
	}
	Close( );
}

void CMySQLCallableW3MMDPlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLW3MMDPlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_Category, m_GameID, m_PID, m_Name, m_Flag, m_Leaver, m_Practicing );

	Close( );
}

void CMySQLCallableW3MMDVarAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		if( m_ValueType == VALUETYPE_INT )
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarInts );
		else if( m_ValueType == VALUETYPE_REAL )
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarReals );
		else
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarStrings );
	}

	Close( );
}

string MySQLWarnReasonsCheck( void *conn, string *error, uint32_t botid, string user, uint32_t warn )
{
	string Res = string();
//	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscWarn = MySQLEscapeString( conn, UTIL_ToString(warn));

//	string Query = "SELECT (reason) FROM bans WHERE name='"+ EscUser+"' AND warn='"+ EscWarn+"' AND server='"+EscServer+"' ";
	string Query = "SELECT (reason) FROM bans WHERE name='"+ EscUser+"' AND warn='"+ EscWarn+"' AND (expiredate=\"\" OR (TO_DAYS(expiredate)> TO_DAYS(CURDATE())))";
	if (warn>1)
		Query = "SELECT (reason) FROM bans WHERE name='"+ EscUser+"' AND warn='"+ EscWarn+"'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );
			uint32_t i = 1;
			while( !Row.empty( ) )
			{
				if (!Res.empty())
					Res +=" | ";
				Res += Row[0];

				Row = MySQLFetchRow( Result );

				i++;
			}
			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Res;
}

string MySQLRanks( void *conn, string *error, uint32_t botid, string server )
{
	string Res = string();
	string EscServer = MySQLEscapeString( conn, server );
	string Query = "SELECT name,score FROM scores WHERE server='"+EscServer+"' ORDER BY score desc LIMIT 0,15";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );
			uint32_t i = 1;
			while( !Row.empty( ) )
			{
				if (!Res.empty())
					Res +=" ";
				Res += "["+UTIL_ToString(i)+"]"+ (Row)[0]+"=" + UTIL_ToString(strtod( ((Row)[1]).c_str(),NULL ));
				
				Row = MySQLFetchRow( Result );

				i++;
			}
			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Res;
}

bool MySQLSafeCheck( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool IsSafe = false;
	string Query = "SELECT * FROM safelist WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
				IsSafe = true;

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return IsSafe;
}

bool MySQLSafeAdd( void *conn, string *error, uint32_t botid, string server, string user, string voucher )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	transform( voucher.begin( ), voucher.end( ), voucher.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscVoucher = MySQLEscapeString( conn, voucher );
	bool Success = false;
	string Query = "INSERT INTO safelist ( server, name, voucher ) VALUES ( '" + EscServer + "', '" + EscUser + "', '" + EscVoucher +"' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

string MySQLNoteCheck( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string Note = string();
	string Query = "SELECT note FROM notes WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
				Note = (Row)[0];

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Note;
}

bool MySQLNoteAdd( void *conn, string *error, uint32_t botid, string server, string user, string note )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscNote = MySQLEscapeString( conn, note );
	bool Success = false;
	string Query = "INSERT INTO notes ( server, name, note ) VALUES ( '" + EscServer + "', '" + EscUser + "', '" + EscNote +"' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLNoteUpdate( void *conn, string *error, uint32_t botid, string server, string user, string note )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscNote = MySQLEscapeString( conn, note );
	bool Success = false;
	string Query = "UPDATE notes SET note='" + EscNote + "' WHERE name='" + EscUser + "' AND server='"+ EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLSafeRemove( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM safelist WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		uint32_t rows = (uint32_t) mysql_affected_rows((MYSQL *)conn);
		if (rows>0)
			Success = true;
	}

	return Success;
}

bool MySQLNoteRemove( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM notes WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		uint32_t rows = (uint32_t) mysql_affected_rows((MYSQL *)conn);
		if (rows>0)
			Success = true;
	}

	return Success;
}

bool MySQLAdminSetAccess( void *conn, string *error, uint32_t botid, string server, string user, uint32_t access )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string acc = UTIL_ToString(access);
	bool ok = false;
	string Query = "UPDATE admins SET access=" + acc + " WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
				ok = true;

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return ok;
}

uint32_t MySQLAdminAccess( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	uint32_t Access = 0;
	bool ok = false;
	string Query = "SELECT access FROM admins WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
			{
				Access = UTIL_ToUInt32((Row)[0]);
//				m_LastAccess = Access;
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Access;
}

vector<uint32_t> MySQLAccessesList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<uint32_t> AccessesList;
	string Query = "SELECT access FROM admins WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				AccessesList.push_back(UTIL_ToUInt32( Row[0] ));
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return AccessesList;
}

vector<string> MySQLSafeList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> SafeList;
	string Query = "SELECT name FROM safelist WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				SafeList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return SafeList;
}

vector<string> MySQLSafeListV( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> SafeList;
	string Query = "SELECT voucher FROM safelist WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				SafeList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return SafeList;
}

vector<string> MySQLNotes( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> NotesList;
	string Query = "SELECT name FROM notes WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				NotesList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return NotesList;
}

vector<string> MySQLNotesN( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> NotesList;
	string Query = "SELECT note FROM notes WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				NotesList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return NotesList;
}

uint32_t MySQLCountPlayerGames( void *conn, string *error, uint32_t botid, string name)
{
	string EscName = MySQLEscapeString( conn, name );
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM gameplayers WHERE name='" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting player games [" + name + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

CDBScoreSummary *ScoreCheck( void *conn, string *error, uint32_t botid, string user)
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );

	CDBScoreSummary *Score = NULL;

	string EscUser = MySQLEscapeString( conn, user );
	bool ok = false;
	double sc = -10000.00;
	uint32_t rank = 0;
	string Query = "SELECT score FROM scores WHERE name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
			{
				sc = strtod( ((Row)[0]).c_str(), NULL);
			}

			char buffer[75];
			#ifdef WIN32
				sprintf_s(buffer, "%.7f",sc);
			#else
				sprintf(buffer, "%.7f",sc);
			#endif
				
				string scString = buffer;

			mysql_free_result( Result );
			if (sc!= -10000)
			{
				Query = "SELECT count(1) as rank FROM scores WHERE score>" + scString;

				if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
					*error = mysql_error( (MYSQL *)conn );
				else
				{
					MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

					if( Result )
					{
						Row = MySQLFetchRow( Result );

						if( !Row.empty( ) )
						{
							rank =  UTIL_ToUInt32((Row)[0]);
							rank = rank + 1;
						}

						mysql_free_result( Result );
					}
					else
						*error = mysql_error( (MYSQL *)conn );
				}


			}

			Score = new CDBScoreSummary( user, sc, rank);
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Score;
}

CDBScoreSummary *ScoreCheck( void *conn, string *error, uint32_t botid, string category, string user, string server)
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );

	CDBScoreSummary *Score = NULL;

	string EscUser = MySQLEscapeString( conn, user );
	string EscCategory = MySQLEscapeString( conn, category );
	string EscServer = MySQLEscapeString( conn, server );
	bool ok = false;
	double sc = -10000.00;
	uint32_t rank = 0;
	string Query = "SELECT score FROM scores WHERE name='" + EscUser + "' AND category='"+ EscCategory + "' AND server='"+ EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
			{
				sc = strtod( ((Row)[0]).c_str(), NULL);
			}

			char buffer[75];
#ifdef WIN32
			sprintf_s(buffer, "%.7f",sc);
#else
			sprintf(buffer, "%.7f",sc);
#endif

			string scString = buffer;

			mysql_free_result( Result );
			if (sc!= -10000)
			{
				Query = "SELECT count(1) as rank FROM scores WHERE score>" + scString;

				if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
					*error = mysql_error( (MYSQL *)conn );
				else
				{
					MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

					if( Result )
					{
						Row = MySQLFetchRow( Result );

						if( !Row.empty( ) )
						{
							rank =  UTIL_ToUInt32((Row)[0]);
							rank = rank + 1;
						}

						mysql_free_result( Result );
					}
					else
						*error = mysql_error( (MYSQL *)conn );
				}


			}

			Score = new CDBScoreSummary( user, sc, rank);
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Score;
}

uint32_t CGHostDBMySQL :: LastAccess( )
{
	return m_LastAccess;
}

bool CGHostDBMySQL :: AdminSetAccess( string server, string user, uint32_t access )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;

	string error;
	bool result = MySQLAdminSetAccess(Connection, &error, m_BotID, server, user, access);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

uint32_t CGHostDBMySQL :: TodayGamesCount( )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return 0;
	string error;
	uint32_t result = MySQLTodayGamesCount(Connection, &error, m_BotID);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

uint32_t CGHostDBMySQL :: BanCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return 0;
	string error;
	uint32_t result = MySQLBanCount(Connection, &error, m_BotID, server);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

uint32_t CGHostDBMySQL :: BanCount( string name, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return 0;
	string error;
	uint32_t result = MySQLBanCount(Connection, &error, m_BotID, name, warn );

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

uint32_t CGHostDBMySQL :: BanCount( string server, string name, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return 0;
	string error;
	uint32_t result = MySQLBanCount(Connection, &error, m_BotID, server, name, warn );

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

bool CGHostDBMySQL :: WarnForget( string name, uint32_t gamethreshold )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;
	bool result =  MySQLWarnForget(Connection, &error, m_BotID, name, gamethreshold);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

bool CGHostDBMySQL :: WarnUpdate( string user, uint32_t before, uint32_t after )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;
	bool result =  MySQLWarnUpdate(Connection, &error, m_BotID, user, before, after);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

bool CGHostDBMySQL :: BanUpdate( string user, string ip )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;
	bool result =  MySQLBanUpdate(Connection, &error, user, ip);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

string CGHostDBMySQL :: RunQuery( string query )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;
	string result =  MySQLRunQuery(Connection, &error, query);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

CDBBan * CGHostDBMySQL :: BanCheck( string server, string user, string ip, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;

	CDBBan *result = NULL;
	result = MySQLBanCheck( Connection, &error, m_BotID, server, user, ip, warn);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

bool CGHostDBMySQL :: BanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;

	bool result;
	result = MySQLBanAdd( Connection, &error, m_BotID, server, user, ip, gamename, admin, reason, expiredaytime, warn);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

bool CGHostDBMySQL :: BanRemove( string server, string user, string admin, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;

	bool result;

	string Admin = string();
	if (!admin.empty())
		Admin = admin;

	if( m_Server.empty( ) && admin.empty() )
		result = MySQLBanRemove( Connection, &error, m_BotID, user, warn );
	else
		result = MySQLBanRemove( Connection, &error, m_BotID, server, user, Admin, warn );

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

bool CGHostDBMySQL :: BanRemove( string user, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;

	bool result;

	result = MySQLBanRemove( Connection, &error, m_BotID, user, warn );

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

string CGHostDBMySQL :: WarnReasonsCheck( string user, uint32_t warn )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return false;
	string error;

	string result;

	result = MySQLWarnReasonsCheck( Connection, &error, m_BotID, user, warn );

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );
	return result;
}

uint32_t CGHostDBMySQL :: RemoveTempBans( string server)
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return 0;

	string error=string();

	uint32_t result = MySQLRemoveTempBans(Connection, &error, m_BotID, server);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );

	return result;
}

vector<string> CGHostDBMySQL :: RemoveTempBanList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return vector<string>( );

	string error=string();

	vector<string> result = MySQLRemoveTempBanList(Connection, &error, m_BotID, server);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );

	return result;
}

vector<string> CGHostDBMySQL :: RemoveBanListDay( string server, uint32_t day )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return vector<string>( );

	string error=string();

	vector<string> result = MySQLRemoveBanListDay(Connection, &error, m_BotID, server, day);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );

	return result;
}

uint32_t CGHostDBMySQL :: ScoresCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;	

	if( !Connection )
	{
		if( !( Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)Connection );
	}
	else if( mysql_ping( (MYSQL *)Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)Connection );
	if (m_Error!="")
		return 0;

	string error=string();

	uint32_t result = MySQLScoresCount(Connection, &error, server);

	if( !error.empty( ) )
		CONSOLE_Print( "[MYSQL] error --- " + error );

	if( m_IdleConnections.size( ) > m_MaxConnections )
	{
		mysql_close( (MYSQL *)Connection );
		m_NumConnections--;
	}
	else
		m_IdleConnections.push( Connection );

	return result;
}

void CGHostDBMySQL :: CreateSafeListTable( void *conn, string *error )
{
	bool Success = false;
	string Query = "CREATE TABLE safelist ( id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, server VARCHAR(100) NOT NULL, name VARCHAR(15) NOT NULL, voucher VARCHAR(15) DEFAULT '' NOT NULL)";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;
}

void CGHostDBMySQL :: CreateNotesTable( void *conn, string *error )
{
	bool Success = false;
	string Query = "CREATE TABLE notes ( id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, server VARCHAR(100) NOT NULL, name VARCHAR(15) NOT NULL, note VARCHAR(250) NOT NULL)";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;
}

void CGHostDBMySQL :: UpgradeAdminTable( void *conn, string *error )
{

	// add new columns to table admins
	//  + access INT64 DEFAULT 4294963199

	bool Success = false;
	string Query = "ALTER TABLE admins ADD access INT64 DEFAULT 4294963199";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;
}

#endif
