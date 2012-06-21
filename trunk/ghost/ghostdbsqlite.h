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

#ifndef GHOSTDBSQLITE_H
#define GHOSTDBSQLITE_H

/**************
 *** SCHEMA ***
 **************

CREATE TABLE admins (
	id INTEGER PRIMARY KEY,
	name TEXT NOT NULL,
	server TEXT NOT NULL DEFAULT "",
	access INT default 4294963199
)

CREATE TABLE bans (
	id INTEGER PRIMARY KEY,
	server TEXT NOT NULL,
	name TEXT NOT NULL,
	ip TEXT,
	date TEXT NOT NULL,
	gamename TEXT,
	admin TEXT NOT NULL,
	reason TEXT
)

CREATE TABLE games (
	id INTEGER PRIMARY KEY,
	server TEXT NOT NULL,
	map TEXT NOT NULL,
	datetime TEXT NOT NULL,
	gamename TEXT NOT NULL,
	ownername TEXT NOT NULL,
	duration INTEGER NOT NULL,
	gamestate INTEGER NOT NULL DEFAULT 0,
	creatorname TEXT NOT NULL DEFAULT "",
	creatorserver TEXT NOT NULL DEFAULT ""
)

CREATE TABLE gameplayers (
	id INTEGER PRIMARY KEY,
	gameid INTEGER NOT NULL,
	name TEXT NOT NULL,
	ip TEXT NOT NULL,
	spoofed INTEGER NOT NULL,
	reserved INTEGER NOT NULL,
	loadingtime INTEGER NOT NULL,
	left INTEGER NOT NULL,
	leftreason TEXT NOT NULL,
	team INTEGER NOT NULL,
	colour INTEGER NOT NULL,
	spoofedrealm TEXT NOT NULL DEFAULT ""
)

CREATE TABLE dotagames (
	id INTEGER PRIMARY KEY,
	gameid INTEGER NOT NULL,
	winner INTEGER NOT NULL,
	min INTEGER NOT NULL DEFAULT 0,
	sec INTEGER NOT NULL DEFAULT 0
)

CREATE TABLE dotaplayers (
	id INTEGER PRIMARY KEY,
	gameid INTEGER NOT NULL,
	colour INTEGER NOT NULL,
	kills INTEGER NOT NULL,
	deaths INTEGER NOT NULL,
	creepkills INTEGER NOT NULL,
	creepdenies INTEGER NOT NULL,
	assists INTEGER NOT NULL,
	gold INTEGER NOT NULL,
	neutralkills INTEGER NOT NULL,
	item1 TEXT NOT NULL,
	item2 TEXT NOT NULL,
	item3 TEXT NOT NULL,
	item4 TEXT NOT NULL,
	item5 TEXT NOT NULL,
	item6 TEXT NOT NULL,
	hero TEXT NOT NULL DEFAULT "",
	newcolour NOT NULL DEFAULT 0,
	towerkills NOT NULL DEFAULT 0,
	raxkills NOT NULL DEFAULT 0,
	courierkills NOT NULL DEFAULT 0
)

CREATE TABLE config (
	name TEXT NOT NULL PRIMARY KEY,
	value TEXT NOT NULL
)

CREATE TABLE downloads (
	id INTEGER PRIMARY KEY,
	map TEXT NOT NULL,
	mapsize INTEGER NOT NULL,
	datetime TEXT NOT NULL,
	name TEXT NOT NULL,
	ip TEXT NOT NULL,
	spoofed INTEGER NOT NULL,
	spoofedrealm TEXT NOT NULL,
	downloadtime INTEGER NOT NULL
)

CREATE TABLE w3mmdplayers (
	id INTEGER PRIMARY KEY,
	category TEXT NOT NULL,
	gameid INTEGER NOT NULL,
	pid INTEGER NOT NULL,
	name TEXT NOT NULL,
	flag TEXT NOT NULL,
	leaver INTEGER NOT NULL,
	practicing INTEGER NOT NULL
)

CREATE TABLE w3mmdvars (
	id INTEGER PRIMARY KEY,
	gameid INTEGER NOT NULL,
	pid INTEGER NOT NULL,
	varname TEXT NOT NULL,
	value_int INTEGER DEFAULT NULL,
	value_real REAL DEFAULT NULL,
	value_string TEXT DEFAULT NULL
)

CREATE TEMPORARY TABLE iptocountry (
	ip1 INTEGER NOT NULL,
	ip2 INTEGER NOT NULL,
	country TEXT NOT NULL,
	PRIMARY KEY ( ip1, ip2 )
)

CREATE INDEX idx_gameid ON gameplayers ( gameid )
CREATE INDEX idx_gameid_colour ON dotaplayers ( gameid, colour )

 **************
 *** SCHEMA ***
 **************/

//
// CSQLITE3 (wrapper class)
//

class CSQLITE3
{
private:
	void *m_DB;
	bool m_Ready;
	vector<string> m_Row;

public:
	CSQLITE3( string filename );
	~CSQLITE3( );

	bool GetReady( )			{ return m_Ready; }
	vector<string> *GetRow( )	{ return &m_Row; }
	string GetError( );

	int Changes( );
	int Prepare( string query, void **Statement );
	int Step( void *Statement );
	int Finalize( void *Statement );
	int Reset( void *Statement );
	int ClearBindings( void *Statement );
	int Exec( string query );
	uint32_t LastRowID( );
};

//
// CGHostDBSQLite
//

class CGHostDBSQLite : public CGHostDB
{
private:
	string m_File;
	uint32_t m_LastAccess;
	CSQLITE3 *m_DB;

	// we keep some prepared statements in memory rather than recreating them each function call
	// this is an optimization because preparing statements takes time
	// however it only pays off if you're going to be using the statement extremely often

	void *FromAddStmt;

public:
	CGHostDBSQLite( CConfig *CFG );
	virtual ~CGHostDBSQLite( );

	virtual void Upgrade1_2( );
	virtual void Upgrade2_3( );
	virtual void Upgrade3_4( );
	virtual void Upgrade4_5( );
	virtual void Upgrade5_6( );
	virtual void Upgrade6_7( );
	virtual void Upgrade7_8( );
	virtual void Upgrade_Tempban( );

	virtual void UpgradeAdminTable( );
	virtual void UpgradeScoresTable( );
	virtual void UpgradeScoresTable2( );
	virtual void CreateSafeListTable( );
	virtual void UpgradeSafeListTable( );
	virtual void CreateNotesTable( );

	virtual bool Begin( );
	virtual bool Commit( );
	virtual uint32_t AdminCount( string server );
	virtual uint32_t CountPlayerGames( string name );
	virtual bool AdminCheck( string server, string user );
	virtual bool SafeAdd( string server, string user, string voucher );
	virtual bool SafeRemove( string server, string user );
	virtual bool NoteAdd( string server, string user, string note);
	virtual bool NoteUpdate( string server, string user, string note);
	virtual bool NoteRemove( string server, string user );
	virtual bool SafeCheck( string server, string user );
	virtual bool CalculateScores( string formula, string mingames );
	virtual bool AdminSetAccess( string server, string user, uint32_t access );
	virtual uint32_t AdminAccess( string server, string user );
	virtual bool AdminAdd( string server, string user );
	virtual bool AdminRemove( string server, string user );
	virtual vector<string> AdminList( string server );
	virtual vector<string> RemoveTempBanList( string server );
	virtual vector<string> RemoveBanListDay( string server, uint32_t day );
	virtual vector<uint32_t> AccessesList( string server );
	virtual vector<string> SafeList( string server );
	virtual vector<string> SafeListV( string server );
	virtual vector<string> Notes( string server );
	virtual vector<string> NotesN( string server );
	virtual uint32_t TodayGamesCount( );
	virtual uint32_t BanCount( string server );
	virtual uint32_t BanCount( string name, uint32_t warn );
	virtual uint32_t BanCount( string server, string name, uint32_t warn );
	virtual uint32_t ScoresCount( string server );
	virtual CDBScoreSummary *ScoreCheck( string name);
	virtual CDBScoreSummary *ScoreCheck( string category, string name, string server);
	virtual string Ranks ( string server );
	virtual CDBBan *BanCheck( string server, string user, string ip, uint32_t warn );
	virtual CDBBan *BanCheckIP( string server, string ip, uint32_t warn );
	virtual bool BanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn );
	virtual bool BanRemove( string server, string user, string admin, uint32_t warn );
	virtual bool BanRemove( string user, uint32_t warn );
	virtual uint32_t RemoveTempBans( string server);
	virtual bool BanUpdate( string user, string ip );
	virtual string RunQuery( string query );
	virtual bool WarnUpdate( string user, uint32_t before, uint32_t after );
	virtual bool WarnForget( string name, uint32_t gamethreshold );
	virtual vector<CDBBan *> BanList( string server );
	virtual uint32_t GameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver );
	virtual uint32_t GamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour );
	virtual uint32_t GamePlayerCount( string name );
	virtual CDBGamePlayerSummary *GamePlayerSummaryCheck( string name );
	virtual uint32_t DotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec );
	virtual uint32_t DotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills );
	virtual uint32_t DotAPlayerCount( string name );
	virtual CDBDotAPlayerSummary *DotAPlayerSummaryCheck( string name, string formula, string mingames, string gamestate );
	virtual string FromCheck( uint32_t ip );
	virtual string WarnReasonsCheck( string user, uint32_t warn );
	virtual bool FromAdd( uint32_t ip1, uint32_t ip2, string country );
	virtual bool DownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime );
	virtual uint32_t W3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing );
	virtual bool W3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints );
	virtual bool W3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals );
	virtual bool W3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings );

	// threaded database functions
	// note: these are not actually implemented with threads at the moment, they WILL block until the query is complete
	// todotodo: implement threads here

	virtual CCallableWarnCount *ThreadedWarnCount( string name, uint32_t warn );
	virtual CCallableAdminCount *ThreadedAdminCount( string server );
	virtual CCallableAdminCheck *ThreadedAdminCheck( string server, string user );
	virtual CCallableAdminAdd *ThreadedAdminAdd( string server, string user );
	virtual CCallableAdminRemove *ThreadedAdminRemove( string server, string user );
	virtual CCallableAdminList *ThreadedAdminList( string server );
	virtual CCallableAccessesList *ThreadedAccessesList( string server );
	virtual CCallableRanks *ThreadedRanks( string server);
	virtual CCallableScoreCheck *ThreadedScoreCheck( string category, string name, string server );
	virtual CCallableBanUpdate *ThreadedBanUpdate( string name, string ip );
	virtual CCallableRunQuery *ThreadedRunQuery( string query );
	virtual CCallableSafeCheck *ThreadedSafeCheck( string server, string user );
	virtual CCallableSafeAdd *ThreadedSafeAdd( string server, string user, string voucher );
	virtual CCallableSafeRemove *ThreadedSafeRemove( string server, string user );
	virtual CCallableNoteAdd *ThreadedNoteAdd( string server, string user, string note );
	virtual CCallableNoteAdd *ThreadedNoteUpdate( string server, string user, string note );
	virtual CCallableNoteRemove *ThreadedNoteRemove( string server, string user );
	virtual CCallableCalculateScores *ThreadedCalculateScores( string formula, string mingames );
	virtual CCallableWarnUpdate *ThreadedWarnUpdate( string user, uint32_t before, uint32_t after );
	virtual CCallableWarnForget *ThreadedWarnForget( string user, uint32_t gamethreshold );
	virtual CCallableSafeList *ThreadedSafeList( string server );
	virtual CCallableSafeList *ThreadedSafeListV( string server );
	virtual CCallableSafeList *ThreadedNotes( string server );
	virtual CCallableSafeList *ThreadedNotesN( string server );
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
	virtual CCallableW3MMDPlayerAdd *ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings );

	virtual uint32_t LastAccess( );
};

#endif
