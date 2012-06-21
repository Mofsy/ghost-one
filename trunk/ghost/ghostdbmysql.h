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

#ifndef GHOSTDBMYSQL_H
#define GHOSTDBMYSQL_H

/**************
 *** SCHEMA ***
 **************

CREATE TABLE admins (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	name VARCHAR(15) NOT NULL,
	server VARCHAR(100) NOT NULL,
	access INT default 4294963199
)

CREATE TABLE bans (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	server VARCHAR(100) NOT NULL,
	name VARCHAR(15) NOT NULL,
	ip VARCHAR(15) NOT NULL,
	date DATETIME NOT NULL,
	gamename VARCHAR(31) NOT NULL,
	admin VARCHAR(15) NOT NULL,
	reason VARCHAR(255) NOT NULL
)

CREATE TABLE games (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	server VARCHAR(100) NOT NULL,
	map VARCHAR(100) NOT NULL,
	datetime DATETIME NOT NULL,
	gamename VARCHAR(31) NOT NULL,
	ownername VARCHAR(15) NOT NULL,
	duration INT NOT NULL,
	gamestate INT NOT NULL,
	creatorname VARCHAR(15) NOT NULL,
	creatorserver VARCHAR(100) NOT NULL
)

CREATE TABLE gameplayers (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	gameid INT NOT NULL,
	name VARCHAR(15) NOT NULL,
	ip VARCHAR(15) NOT NULL,
	spoofed INT NOT NULL,
	reserved INT NOT NULL,
	loadingtime INT NOT NULL,
	`left` INT NOT NULL,
	leftreason VARCHAR(100) NOT NULL,
	team INT NOT NULL,
	colour INT NOT NULL,
	spoofedrealm VARCHAR(100) NOT NULL,
	INDEX( gameid )
)

CREATE TABLE dotagames (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	gameid INT NOT NULL,
	winner INT NOT NULL,
	min INT NOT NULL,
	sec INT NOT NULL
)

CREATE TABLE dotaplayers (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	gameid INT NOT NULL,
	colour INT NOT NULL,
	kills INT NOT NULL,
	deaths INT NOT NULL,
	creepkills INT NOT NULL,
	creepdenies INT NOT NULL,
	assists INT NOT NULL,
	gold INT NOT NULL,
	neutralkills INT NOT NULL,
	item1 CHAR(4) NOT NULL,
	item2 CHAR(4) NOT NULL,
	item3 CHAR(4) NOT NULL,
	item4 CHAR(4) NOT NULL,
	item5 CHAR(4) NOT NULL,
	item6 CHAR(4) NOT NULL,
	hero CHAR(4) NOT NULL,
	newcolour INT NOT NULL,
	towerkills INT NOT NULL,
	raxkills INT NOT NULL,
	courierkills INT NOT NULL,
	INDEX( gameid, colour )
)

CREATE TABLE downloads (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	map VARCHAR(100) NOT NULL,
	mapsize INT NOT NULL,
	datetime DATETIME NOT NULL,
	name VARCHAR(15) NOT NULL,
	ip VARCHAR(15) NOT NULL,
	spoofed INT NOT NULL,
	spoofedrealm VARCHAR(100) NOT NULL,
	downloadtime INT NOT NULL
)

CREATE TABLE scores (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	category VARCHAR(25) NOT NULL,
	name VARCHAR(15) NOT NULL,
	server VARCHAR(100) NOT NULL,
	score REAL NOT NULL
)

CREATE TABLE w3mmdplayers (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	category VARCHAR(25) NOT NULL,
	gameid INT NOT NULL,
	pid INT NOT NULL,
	name VARCHAR(15) NOT NULL,
	flag VARCHAR(32) NOT NULL,
	leaver INT NOT NULL,
	practicing INT NOT NULL
)

CREATE TABLE w3mmdvars (
	id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
	botid INT NOT NULL,
	gameid INT NOT NULL,
	pid INT NOT NULL,
	varname VARCHAR(25) NOT NULL,
	value_int INT DEFAULT NULL,
	value_real REAL DEFAULT NULL,
	value_string VARCHAR(100) DEFAULT NULL
)

 **************
 *** SCHEMA ***
 **************/

//
// CGHostDBMySQL
//

class CGHostDBMySQL : public CGHostDB
{
private:
	string m_Server;
	string m_Database;
	string m_User;
	string m_Password;
	uint16_t m_Port;
	uint32_t m_BotID;
	queue<void *> m_IdleConnections;
	uint32_t m_NumConnections;
	uint32_t m_MaxConnections;
	uint32_t m_OutstandingCallables;
	uint32_t m_LastAccess;
	vector<uint32_t> m_LastAccesses;

public:
	CGHostDBMySQL( CConfig *CFG );
	virtual ~CGHostDBMySQL( );

	virtual string GetStatus( );

	virtual void RecoverCallable( CBaseCallable *callable );

	// threaded database functions

	virtual void CreateThread( CBaseCallable *callable );
	virtual CCallableAdminCount *ThreadedAdminCount( string server );
	virtual CCallableAdminCheck *ThreadedAdminCheck( string server, string user );
	virtual CCallableAdminAdd *ThreadedAdminAdd( string server, string user );
	virtual CCallableAdminRemove *ThreadedAdminRemove( string server, string user );
	virtual CCallableAdminList *ThreadedAdminList( string server );
	virtual CCallableAccessesList *ThreadedAccessesList( string server );
	virtual CCallableSafeCheck *ThreadedSafeCheck( string server, string user );
	virtual CCallableRanks *ThreadedRanks( string server);
	virtual CCallableSafeAdd *ThreadedSafeAdd( string server, string user, string voucher );
	virtual CCallableSafeRemove *ThreadedSafeRemove( string server, string user );
	virtual CCallableNoteAdd *ThreadedNoteUpdate( string server, string user, string note );
	virtual CCallableNoteAdd *ThreadedNoteAdd( string server, string user, string note );
	virtual CCallableNoteRemove *ThreadedNoteRemove( string server, string user );
	virtual CCallableSafeList *ThreadedSafeList( string server );
	virtual CCallableSafeList *ThreadedSafeListV( string server );
	virtual CCallableSafeList *ThreadedNotes( string server );
	virtual CCallableSafeList *ThreadedNotesN( string server );
	virtual CCallableCalculateScores *ThreadedCalculateScores( string formula, string mingames );
	virtual CCallableWarnUpdate *ThreadedWarnUpdate( string name, uint32_t before, uint32_t after );
	virtual CCallableWarnForget *ThreadedWarnForget( string name, uint32_t gamethreshold );
	virtual CCallableTodayGamesCount *ThreadedTodayGamesCount( );
	virtual CCallableBanCount *ThreadedBanCount( string server );
	virtual CCallableBanCheck *ThreadedBanCheck( string server, string user, string ip, uint32_t warn );
	virtual CCallableBanAdd *ThreadedBanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn );
	virtual CCallableBanRemove *ThreadedBanRemove( string server, string user, string admin, uint32_t warn );
	virtual CCallableBanRemove *ThreadedBanRemove( string user, uint32_t warn );
	virtual CCallableBanList *ThreadedBanList( string server );
	virtual CCallableGameAdd *ThreadedGameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver );
	virtual CCallableGamePlayerAdd *ThreadedGamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour );
	virtual CCallableGamePlayerSummaryCheck *ThreadedGamePlayerSummaryCheck( string name );
	virtual CCallableDotAGameAdd *ThreadedDotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec );
	virtual CCallableDotAPlayerAdd *ThreadedDotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills );
	virtual CCallableDotAPlayerSummaryCheck *ThreadedDotAPlayerSummaryCheck( string name, string formula, string mingames, string gamestate );
	virtual CCallableDownloadAdd *ThreadedDownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime );
	virtual CCallableWarnCount *ThreadedWarnCount( string name, uint32_t warn );
	virtual CCallableBanUpdate *ThreadedBanUpdate( string name, string ip );
	virtual CCallableRunQuery *ThreadedRunQuery( string query );
	virtual CCallableScoreCheck *ThreadedScoreCheck( string category, string name, string server );
	virtual CCallableW3MMDPlayerAdd *ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings );

	// other database functions

	virtual void *GetIdleConnection( );

	// ghost one functions
/*
	virtual bool SafeAdd( string server, string user );
	virtual bool SafeRemove( string server, string user );
	virtual bool SafeCheck( string server, string user );
*/

	uint32_t LastAccess( );
	bool AdminSetAccess(  string server, string user, uint32_t access );
	uint32_t TodayGamesCount( );
	uint32_t BanCount( string server);
	uint32_t BanCount( string name, uint32_t warn );
	uint32_t BanCount( string server, string name, uint32_t warn );
	CDBBan * BanCheck( string server, string user, string ip, uint32_t warn );
	bool BanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn );
	bool BanRemove( string server, string user, string admin, uint32_t warn);
	bool BanRemove( string user, uint32_t warn);
	string WarnReasonsCheck( string user, uint32_t warn );
	uint32_t RemoveTempBans( string server );
	vector<string> RemoveTempBanList( string server );
	vector<string> RemoveBanListDay( string server, uint32_t day );
	bool BanUpdate( string user, string ip);
	string RunQuery( string query);
	bool WarnUpdate( string user, uint32_t before, uint32_t after );
	bool WarnForget( string name, uint32_t gamethreshold );
	uint32_t ScoresCount(  string server);
	void CreateSafeListTable( void *conn, string *error );
	void CreateNotesTable( void *conn, string *error );
	void UpgradeAdminTable( void *conn, string *error );
};

//
// global helper functions
//

uint32_t MySQLAdminCount( void *conn, string *error, uint32_t botid, string server );
bool MySQLAdminCheck( void *conn, string *error, uint32_t botid, string server, string user );
bool MySQLAdminAdd( void *conn, string *error, uint32_t botid, string server, string user, uint32_t defaccess );
bool MySQLAdminRemove( void *conn, string *error, uint32_t botid, string server, string user );
vector<string> MySQLAdminList( void *conn, string *error, uint32_t botid, string server );
vector<string> MySQLRemoveTempBanList( void *conn, string *error, uint32_t botid, string server );
vector<string> MySQLRemoveBanListDay( void *conn, string *error, uint32_t botid, uint32_t day );
uint32_t MySQLTodayGamesCount( void *conn, string *error, uint32_t botid );
uint32_t MySQLBanCount( void *conn, string *error, uint32_t botid, string server );
uint32_t MySQLScoresCount( void *conn, string *error, uint32_t botid, string server );
CDBBan *MySQLBanCheck( void *conn, string *error, uint32_t botid, string server, string user, string ip, uint32_t warn );
bool MySQLBanAdd( void *conn, string *error, uint32_t botid, string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn );
bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string server, string user, string admin, uint32_t warn );
bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string user, uint32_t warn );
bool MySQLBanUpdate( void *conn, string *error, uint32_t botid, string user, string ip );
string MySQLRunQuery( void *conn, string *error, uint32_t botid, string query );
bool MySQLWarnUpdate( void *conn, string *error, uint32_t botid, string user, uint32_t before, uint32_t after );
bool MySQLWarnForget( void *conn, string *error, uint32_t botid, string name, uint32_t gamethreshold );
uint32_t MySQLRemoveTempBans( void *conn, string *error, uint32_t botid );
vector<CDBBan *> MySQLBanList( void *conn, string *error, uint32_t botid, string server );
uint32_t MySQLGameAdd( void *conn, string *error, uint32_t botid, string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver );
uint32_t MySQLGamePlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour );
CDBGamePlayerSummary *MySQLGamePlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name );
uint32_t MySQLDotAGameAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec );
uint32_t MySQLDotAPlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills );
CDBDotAPlayerSummary *MySQLDotAPlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name, string formula, string mingames, string gamestate );
bool MySQLDownloadAdd( void *conn, string *error, uint32_t botid, string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime );
bool MySQLCalculateScores( void *conn, string *error, uint32_t botid, string formula, string mingames );
double MySQLScoreCheck( void *conn, string *error, uint32_t botid, string category, string name, string server );
uint32_t MySQLW3MMDPlayerAdd( void *conn, string *error, uint32_t botid, string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing );
bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,int32_t> var_ints );
bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,double> var_reals );
bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,string> var_strings );

string MySQLRanks( void *conn, string *error, uint32_t botid, string server );
string MySQLWarnReasonsCheck( void *conn, string *error, uint32_t botid, string user, uint32_t warn );
bool MySQLSafeCheck( void *conn, string *error, uint32_t botid, string server, string user );
bool MySQLSafeAdd( void *conn, string *error, uint32_t botid, string server, string user, string voucher );
bool MySQLSafeRemove( void *conn, string *error, uint32_t botid, string server, string user );
bool MySQLNoteAdd( void *conn, string *error, uint32_t botid, string server, string user, string note );
bool MySQLNoteUpdate( void *conn, string *error, uint32_t botid, string server, string user, string note );
bool MySQLNoteRemove( void *conn, string *error, uint32_t botid, string server, string user );
uint32_t MySQLAdminAccess( void *conn, string *error, uint32_t botid, string server, string user );
vector<uint32_t> MySQLAccessesList( void *conn, string *error, uint32_t botid, string server );
vector<string> MySQLSafeList( void *conn, string *error, uint32_t botid, string server );
vector<string> MySQLSafeListV( void *conn, string *error, uint32_t botid, string server );
vector<string> MySQLNotes( void *conn, string *error, uint32_t botid, string server );
vector<string> MySQLNotesN( void *conn, string *error, uint32_t botid, string server );
CDBScoreSummary *ScoreCheck( void *conn, string *error, uint32_t botid, string user);
CDBScoreSummary *ScoreCheck( void *conn, string *error, uint32_t botid, string category, string user, string server);
uint32_t MySQLCountPlayerGames( void *conn, string *error, uint32_t botid, string name);


//
// MySQL Callables
//

class CMySQLCallable : virtual public CBaseCallable
{
protected:
	void *m_Connection;
	string m_SQLServer;
	string m_SQLDatabase;
	string m_SQLUser;
	string m_SQLPassword;
	uint16_t m_SQLPort;
	uint32_t m_SQLBotID;

public:
	CMySQLCallable( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), m_Connection( nConnection ), m_SQLBotID( nSQLBotID ), m_SQLServer( nSQLServer ), m_SQLDatabase( nSQLDatabase ), m_SQLUser( nSQLUser ), m_SQLPassword( nSQLPassword ), m_SQLPort( nSQLPort ) { }
	virtual ~CMySQLCallable( ) { }

	virtual void *GetConnection( )	{ return m_Connection; }

	virtual void Init( );
	virtual void Close( );
};

class CMySQLCallableAdminCount : public CCallableAdminCount, public CMySQLCallable
{
public:
	CMySQLCallableAdminCount( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminCount( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminCount( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminCheck : public CCallableAdminCheck, public CMySQLCallable
{
public:
	CMySQLCallableAdminCheck( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminCheck( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminAdd : public CCallableAdminAdd, public CMySQLCallable
{
public:
	CMySQLCallableAdminAdd( string nServer, string nUser, uint32_t nAccess, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminAdd( nServer, nUser, nAccess ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminRemove : public CCallableAdminRemove, public CMySQLCallable
{
public:
	CMySQLCallableAdminRemove( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminRemove( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminRemove( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminList : public CCallableAdminList, public CMySQLCallable
{
public:
	CMySQLCallableAdminList( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminList( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAccessesList : public CCallableAccessesList, public CMySQLCallable
{
public:
	CMySQLCallableAccessesList( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAccessesList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAccessesList( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableSafeList : public CCallableSafeList, public CMySQLCallable
{
public:
	CMySQLCallableSafeList( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableSafeList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableSafeList( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableSafeListV : public CCallableSafeList, public CMySQLCallable
{
public:
	CMySQLCallableSafeListV( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableSafeList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableSafeListV( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableNotes : public CCallableSafeList, public CMySQLCallable
{
public:
	CMySQLCallableNotes( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableSafeList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableNotes( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableNotesN : public CCallableSafeList, public CMySQLCallable
{
public:
	CMySQLCallableNotesN( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableSafeList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableNotesN( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableRanks : public CCallableRanks, public CMySQLCallable
{
public:
	CMySQLCallableRanks( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableRanks( nServer), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableRanks( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableSafeCheck : public CCallableSafeCheck, public CMySQLCallable
{
public:
	CMySQLCallableSafeCheck( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableSafeCheck( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableSafeCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableSafeAdd : public CCallableSafeAdd, public CMySQLCallable
{
public:
	CMySQLCallableSafeAdd( string nServer, string nUser, string nVoucher, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableSafeAdd( nServer, nUser, nVoucher ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableSafeAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableSafeRemove : public CCallableSafeRemove, public CMySQLCallable
{
public:
	CMySQLCallableSafeRemove( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableSafeRemove( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableSafeRemove( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableNoteAdd : public CCallableNoteAdd, public CMySQLCallable
{
public:
	CMySQLCallableNoteAdd( string nServer, string nUser, string nNote, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableNoteAdd( nServer, nUser, nNote ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableNoteAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableNoteUpdate : public CCallableNoteAdd, public CMySQLCallable
{
public:
	CMySQLCallableNoteUpdate( string nServer, string nUser, string nNote, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableNoteAdd( nServer, nUser, nNote ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableNoteUpdate( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableNoteRemove : public CCallableNoteRemove, public CMySQLCallable
{
public:
	CMySQLCallableNoteRemove( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableNoteRemove( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableNoteRemove( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableCalculateScores : public CCallableCalculateScores, public CMySQLCallable
{
public:
	CMySQLCallableCalculateScores( string nFormula, string nMinGames, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableCalculateScores( nFormula, nMinGames ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableCalculateScores( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableWarnUpdate : public CCallableWarnUpdate, public CMySQLCallable
{
public:
	CMySQLCallableWarnUpdate( string nUser, uint32_t nBefore, uint32_t nAfter, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableWarnUpdate( nUser, nBefore, nAfter ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableWarnUpdate( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableWarnForget : public CCallableWarnForget, public CMySQLCallable
{
public:
	CMySQLCallableWarnForget( string nUser, uint32_t nGameThreshold, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableWarnForget( nUser, nGameThreshold ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableWarnForget( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableTodayGamesCount : public CCallableTodayGamesCount, public CMySQLCallable
{
public:
	CMySQLCallableTodayGamesCount( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableTodayGamesCount( ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableTodayGamesCount( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanCount : public CCallableBanCount, public CMySQLCallable
{
public:
	CMySQLCallableBanCount( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanCount( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanCount( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanCheck : public CCallableBanCheck, public CMySQLCallable
{
public:
	CMySQLCallableBanCheck( string nServer, string nUser, string nIP, uint32_t nWarn, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanCheck( nServer, nUser, nIP, nWarn ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanAdd : public CCallableBanAdd, public CMySQLCallable
{
public:
	CMySQLCallableBanAdd( string nServer, string nUser, string nIP, string nGameName, string nAdmin, string nReason, uint32_t nexpiredaytime, uint32_t nwarn, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanAdd( nServer, nUser, nIP, nGameName, nAdmin, nReason, nexpiredaytime, nwarn ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanRemove : public CCallableBanRemove, public CMySQLCallable
{
public:
	CMySQLCallableBanRemove( string nServer, string nUser, string nAdmin, uint32_t nWarn, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanRemove( nServer, nUser, nAdmin, nWarn ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanRemove( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanList : public CCallableBanList, public CMySQLCallable
{
public:
	CMySQLCallableBanList( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanList( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGameAdd : public CCallableGameAdd, public CMySQLCallable
{
public:
	CMySQLCallableGameAdd( string nServer, string nMap, string nGameName, string nOwnerName, uint32_t nDuration, uint32_t nGameState, string nCreatorName, string nCreatorServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGameAdd( nServer, nMap, nGameName, nOwnerName, nDuration, nGameState, nCreatorName, nCreatorServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableGameAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGamePlayerAdd : public CCallableGamePlayerAdd, public CMySQLCallable
{
public:
	CMySQLCallableGamePlayerAdd( uint32_t nGameID, string nName, string nIP, uint32_t nSpoofed, string nSpoofedRealm, uint32_t nReserved, uint32_t nLoadingTime, uint32_t nLeft, string nLeftReason, uint32_t nTeam, uint32_t nColour, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGamePlayerAdd( nGameID, nName, nIP, nSpoofed, nSpoofedRealm, nReserved, nLoadingTime, nLeft, nLeftReason, nTeam, nColour ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableGamePlayerAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGamePlayerSummaryCheck : public CCallableGamePlayerSummaryCheck, public CMySQLCallable
{
public:
	CMySQLCallableGamePlayerSummaryCheck( string nName, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGamePlayerSummaryCheck( nName ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableGamePlayerSummaryCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDotAGameAdd : public CCallableDotAGameAdd, public CMySQLCallable
{
public:
	CMySQLCallableDotAGameAdd( uint32_t nGameID, uint32_t nWinner, uint32_t nMin, uint32_t nSec, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDotAGameAdd( nGameID, nWinner, nMin, nSec ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDotAGameAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDotAPlayerAdd : public CCallableDotAPlayerAdd, public CMySQLCallable
{
public:
	CMySQLCallableDotAPlayerAdd( uint32_t nGameID, uint32_t nColour, uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nGold, uint32_t nNeutralKills, string nItem1, string nItem2, string nItem3, string nItem4, string nItem5, string nItem6, string nHero, uint32_t nNewColour, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDotAPlayerAdd( nGameID, nColour, nKills, nDeaths, nCreepKills, nCreepDenies, nAssists, nGold, nNeutralKills, nItem1, nItem2, nItem3, nItem4, nItem5, nItem6, nHero, nNewColour, nTowerKills, nRaxKills, nCourierKills ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDotAPlayerAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDotAPlayerSummaryCheck : public CCallableDotAPlayerSummaryCheck, public CMySQLCallable
{
public:
	CMySQLCallableDotAPlayerSummaryCheck( string nName, string nFormula, string nMinGames, string nGameState, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDotAPlayerSummaryCheck( nName, nFormula, nMinGames, nGameState ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDotAPlayerSummaryCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDownloadAdd : public CCallableDownloadAdd, public CMySQLCallable
{
public:
	CMySQLCallableDownloadAdd( string nMap, uint32_t nMapSize, string nName, string nIP, uint32_t nSpoofed, string nSpoofedRealm, uint32_t nDownloadTime, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDownloadAdd( nMap, nMapSize, nName, nIP, nSpoofed, nSpoofedRealm, nDownloadTime ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDownloadAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableWarnCount : public CCallableWarnCount, public CMySQLCallable
{
public:
	CMySQLCallableWarnCount( string nName, uint32_t nWarn, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableWarnCount( nName, nWarn ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableWarnCount( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanUpdate : public CCallableBanUpdate, public CMySQLCallable
{
public:
	CMySQLCallableBanUpdate( string nName, string nIp, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanUpdate( nName, nIp ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanUpdate( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableRunQuery: public CCallableRunQuery, public CMySQLCallable
{
public:
	CMySQLCallableRunQuery( string nQuery, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableRunQuery( nQuery ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableRunQuery( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableScoreCheck : public CCallableScoreCheck, public CMySQLCallable
{
public:
	CMySQLCallableScoreCheck( string nCategory, string nName, string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableScoreCheck( nCategory, nName, nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableScoreCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableW3MMDPlayerAdd : public CCallableW3MMDPlayerAdd, public CMySQLCallable
{
public:
	CMySQLCallableW3MMDPlayerAdd( string nCategory, uint32_t nGameID, uint32_t nPID, string nName, string nFlag, uint32_t nLeaver, uint32_t nPracticing, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDPlayerAdd( nCategory, nGameID, nPID, nName, nFlag, nLeaver, nPracticing ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableW3MMDPlayerAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableW3MMDVarAdd : public CCallableW3MMDVarAdd, public CMySQLCallable
{
public:
	CMySQLCallableW3MMDVarAdd( uint32_t nGameID, map<VarP,int32_t> nVarInts, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDVarAdd( nGameID, nVarInts ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	CMySQLCallableW3MMDVarAdd( uint32_t nGameID, map<VarP,double> nVarReals, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDVarAdd( nGameID, nVarReals ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	CMySQLCallableW3MMDVarAdd( uint32_t nGameID, map<VarP,string> nVarStrings, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDVarAdd( nGameID, nVarStrings ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableW3MMDVarAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

#endif

#endif
