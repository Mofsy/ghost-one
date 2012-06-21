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
#include "config.h"
#include "ghostdb.h"
#include "ghostdbsqlite.h"
#include "sqlite3.h"
#include <boost/filesystem.hpp>

using namespace boost :: filesystem;

//
// CQSLITE3 (wrapper class)
//

CSQLITE3 :: CSQLITE3( string filename )
{
	m_Ready = true;

//	if( sqlite3_open_v2( filename.c_str( ), (sqlite3 **)&m_DB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL ) != SQLITE_OK )
	if( sqlite3_open_v2( filename.c_str( ), (sqlite3 **)&m_DB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL ) != SQLITE_OK )
		m_Ready = false;
}

CSQLITE3 :: ~CSQLITE3( )
{
	sqlite3_close( (sqlite3 *)m_DB );
}

string CSQLITE3 :: GetError( )
{
	return sqlite3_errmsg( (sqlite3 *)m_DB );
}

int CSQLITE3 :: Changes( )
{
	return sqlite3_changes( (sqlite3 *)m_DB );
}

int CSQLITE3 :: Prepare( string query, void **Statement )
{
	return sqlite3_prepare_v2( (sqlite3 *)m_DB, query.c_str( ), -1, (sqlite3_stmt **)Statement, NULL );
}

int CSQLITE3 :: Step( void *Statement )
{
	int RC = sqlite3_step( (sqlite3_stmt *)Statement );

	if( RC == SQLITE_ROW )
	{
		m_Row.clear( );

		for( int i = 0; i < sqlite3_column_count( (sqlite3_stmt *)Statement ); i++ )
		{
			char *ColumnText = (char *)sqlite3_column_text( (sqlite3_stmt *)Statement, i );

			if( ColumnText )
				m_Row.push_back( ColumnText );
			else
				m_Row.push_back( string( ) );
		}
	}

	return RC;
}

int CSQLITE3 :: Finalize( void *Statement )
{
	return sqlite3_finalize( (sqlite3_stmt *)Statement );
}

int CSQLITE3 :: Reset( void *Statement )
{
	return sqlite3_reset( (sqlite3_stmt *)Statement );
}

int CSQLITE3 :: ClearBindings( void *Statement )
{
	return sqlite3_clear_bindings( (sqlite3_stmt *)Statement );
}

int CSQLITE3 :: Exec( string query )
{
	return sqlite3_exec( (sqlite3 *)m_DB, query.c_str( ), NULL, NULL, NULL );
}

uint32_t CSQLITE3 :: LastRowID( )
{
	return (uint32_t)sqlite3_last_insert_rowid( (sqlite3 *)m_DB );
}

//
// CGHostDBSQLite
//

CGHostDBSQLite :: CGHostDBSQLite( CConfig *CFG ) : CGHostDB( CFG )
{
	m_File = CFG->GetString( "db_sqlite3_file", "ghost.dbs" );
	CONSOLE_Print( "[SQLITE3] version " + string( SQLITE_VERSION ) );
	CONSOLE_Print( "[SQLITE3] opening database [" + m_File + "]" );
	
	string m_localf = "ips.dbs";
	CSQLITE3 * m_local;
	if (!exists(m_localf))
	{
		m_local = new CSQLITE3( m_localf);
		if( m_local->Exec( "CREATE TABLE iptocountry ( ip1 INTEGER NOT NULL, ip2 INTEGER NOT NULL, country TEXT NOT NULL, PRIMARY KEY ( ip1, ip2 ) )" ) != SQLITE_OK )
			CONSOLE_Print( "[SQLITE3] error creating iptocountry table - " + m_local->GetError( ) );
		delete m_local;
	}
	
	m_DB = new CSQLITE3( m_File );

	if( !m_DB->GetReady( ) )
	{
		// setting m_HasError to true indicates there's been a critical error and we want GHost to shutdown
		// this is okay here because we're in the constructor so we're not dropping any games or players

		CONSOLE_Print( string( "[SQLITE3] error opening database [" + m_File + "] - " ) + m_DB->GetError( ) );
		m_HasError = true;
		m_Error = "error opening database";
		return;
	}

	m_DB->Exec("PRAGMA cache_size=15000; PRAGMA synchronous=OFF; PRAGMA temp_store=2;");

	// find the schema number so we can determine whether we need to upgrade or not

	string SchemaNumber;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT value FROM config WHERE name=\"schema_number\"", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
				SchemaNumber = (*Row)[0];
			else
				CONSOLE_Print( "[SQLITE3] error getting schema number - row doesn't have 1 column" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error getting schema number - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error getting schema number - " + m_DB->GetError( ) );

	if( SchemaNumber.empty( ) )
	{
		// couldn't find the schema number
		// unfortunately the very first schema didn't have a config table
		// so we might actually be looking at schema version 1 rather than an empty database
		// try to confirm this by looking for the admins table

		CONSOLE_Print( "[SQLITE3] couldn't find schema number, looking for admins table" );
		bool AdminTable = false;
		m_DB->Prepare( "SELECT * FROM sqlite_master WHERE type=\"table\" AND name=\"admins\"", (void **)&Statement );

		if( Statement )
		{
			int RC = m_DB->Step( Statement );

			// we're just checking to see if the query returned a row, we don't need to check the row data itself

			if( RC == SQLITE_ROW )
				AdminTable = true;
			else if( RC == SQLITE_ERROR )
				CONSOLE_Print( "[SQLITE3] error looking for admins table - " + m_DB->GetError( ) );

			m_DB->Finalize( Statement );
		}
		else
			CONSOLE_Print( "[SQLITE3] prepare error looking for admins table - " + m_DB->GetError( ) );

		if( AdminTable )
		{
			// the admins table exists, assume we're looking at schema version 1

			CONSOLE_Print( "[SQLITE3] found admins table, assuming schema number [1]" );
			SchemaNumber = "1";
		}
		else
		{
			// the admins table doesn't exist, assume the database is empty
			// note to self: update the SchemaNumber and the database structure when making a new schema

			CONSOLE_Print( "[SQLITE3] couldn't find admins table, assuming database is empty" );
			SchemaNumber = "8";

			string squery = "CREATE TABLE admins ( id INTEGER PRIMARY KEY, name TEXT NOT NULL, server TEXT NOT NULL DEFAULT \"\", access INT64 default "+UTIL_ToString(m_AdminAccess)+")";
			if( m_DB->Exec( squery ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating admins table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE bans ( id INTEGER PRIMARY KEY, server TEXT NOT NULL, name TEXT NOT NULL, ip TEXT, date TEXT NOT NULL, gamename TEXT, admin TEXT NOT NULL, reason TEXT )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating bans table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE games ( id INTEGER PRIMARY KEY, server TEXT NOT NULL, map TEXT NOT NULL, datetime TEXT NOT NULL, gamename TEXT NOT NULL, ownername TEXT NOT NULL, duration INTEGER NOT NULL, gamestate INTEGER NOT NULL DEFAULT 0, creatorname TEXT NOT NULL DEFAULT \"\", creatorserver TEXT NOT NULL DEFAULT \"\" )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating games table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE gameplayers ( id INTEGER PRIMARY KEY, gameid INTEGER NOT NULL, name TEXT NOT NULL, ip TEXT NOT NULL, spoofed INTEGER NOT NULL, reserved INTEGER NOT NULL, loadingtime INTEGER NOT NULL, left INTEGER NOT NULL, leftreason TEXT NOT NULL, team INTEGER NOT NULL, colour INTEGER NOT NULL, spoofedrealm TEXT NOT NULL DEFAULT \"\" )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating gameplayers table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE dotagames ( id INTEGER PRIMARY KEY, gameid INTEGER NOT NULL, winner INTEGER NOT NULL, min INTEGER NOT NULL DEFAULT 0, sec INTEGER NOT NULL DEFAULT 0 )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating dotagames table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE dotaplayers ( id INTEGER PRIMARY KEY, gameid INTEGER NOT NULL, colour INTEGER NOT NULL, kills INTEGER NOT NULL, deaths INTEGER NOT NULL, creepkills INTEGER NOT NULL, creepdenies INTEGER NOT NULL, assists INTEGER NOT NULL, gold INTEGER NOT NULL, neutralkills INTEGER NOT NULL, item1 TEXT NOT NULL, item2 TEXT NOT NULL, item3 TEXT NOT NULL, item4 TEXT NOT NULL, item5 TEXT NOT NULL, item6 TEXT NOT NULL, hero TEXT NOT NULL DEFAULT \"\", newcolour NOT NULL DEFAULT 0, towerkills NOT NULL DEFAULT 0, raxkills NOT NULL DEFAULT 0, courierkills NOT NULL DEFAULT 0 )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating dotaplayers table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE config ( name TEXT NOT NULL PRIMARY KEY, value TEXT NOT NULL )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating config table - " + m_DB->GetError( ) );

			m_DB->Prepare( "INSERT INTO config VALUES ( \"schema_number\", ? )", (void **)&Statement );

			if( Statement )
			{
				sqlite3_bind_text( Statement, 1, SchemaNumber.c_str( ), -1, SQLITE_TRANSIENT );
				int RC = m_DB->Step( Statement );

				if( RC == SQLITE_ERROR )
					CONSOLE_Print( "[SQLITE3] error inserting schema number [" + SchemaNumber + "] - " + m_DB->GetError( ) );

				m_DB->Finalize( Statement );
			}
			else
				CONSOLE_Print( "[SQLITE3] prepare error inserting schema number [" + SchemaNumber + "] - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE downloads ( id INTEGER PRIMARY KEY, map TEXT NOT NULL, mapsize INTEGER NOT NULL, datetime TEXT NOT NULL, name TEXT NOT NULL, ip TEXT NOT NULL, spoofed INTEGER NOT NULL, spoofedrealm TEXT NOT NULL, downloadtime INTEGER NOT NULL )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating downloads table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE w3mmdplayers ( id INTEGER PRIMARY KEY, category TEXT NOT NULL, gameid INTEGER NOT NULL, pid INTEGER NOT NULL, name TEXT NOT NULL, flag TEXT NOT NULL, leaver INTEGER NOT NULL, practicing INTEGER NOT NULL )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating w3mmdplayers table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE TABLE w3mmdvars ( id INTEGER PRIMARY KEY, gameid INTEGER NOT NULL, pid INTEGER NOT NULL, varname TEXT NOT NULL, value_int INTEGER DEFAULT NULL, value_real REAL DEFAULT NULL, value_string TEXT DEFAULT NULL )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating w3mmdvars table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE INDEX idx_gameid ON gameplayers ( gameid )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating idx_gameid index on gameplayers table - " + m_DB->GetError( ) );

			if( m_DB->Exec( "CREATE INDEX idx_gameid_colour ON dotaplayers ( gameid, colour )" ) != SQLITE_OK )
				CONSOLE_Print( "[SQLITE3] error creating idx_gameid_colour index on dotaplayers table - " + m_DB->GetError( ) );
		}
	}
	else
		CONSOLE_Print( "[SQLITE3] found schema number [" + SchemaNumber + "]" );

	if( SchemaNumber == "1" )
	{
		Upgrade1_2( );
		SchemaNumber = "2";
	}

	if( SchemaNumber == "2" )
	{
		Upgrade2_3( );
		SchemaNumber = "3";
	}

	if( SchemaNumber == "3" )
	{
		Upgrade3_4( );
		SchemaNumber = "4";
	}

	if( SchemaNumber == "4" )
	{
		Upgrade4_5( );
		SchemaNumber = "5";
	}

	if( SchemaNumber == "5" )
	{
		Upgrade5_6( );
		SchemaNumber = "6";
	}

	if( SchemaNumber == "6" )
	{
		Upgrade6_7( );
		SchemaNumber = "7";
	}

	if( SchemaNumber == "7" )
	{
		Upgrade7_8( );
		SchemaNumber = "8";
	}

	CONSOLE_Print( "[SQLITE3] checking ban table" );
	bool TempbanTable = false;
	m_DB->Prepare( "SELECT gamecount FROM bans", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ERROR )
		{
			CONSOLE_Print( "[SQLITE3] no tempban" );
			TempbanTable = false;
		}
		else TempbanTable = true;

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error looking for bans table - " + m_DB->GetError( ) );

	if( TempbanTable )
	{
		// the tempban exists

		CONSOLE_Print( "[SQLITE3] found tempban additions" );
	} else Upgrade_Tempban();

	CONSOLE_Print( "[SQLITE3] checking admin table" );
	bool AdminTable2 = false;
	m_DB->Prepare( "SELECT access FROM admins", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ERROR )
		{
			CONSOLE_Print( "[SQLITE3] old admin table" );
			AdminTable2 = false;
		}
		else AdminTable2 = true;

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error looking for admins table - " + m_DB->GetError( ) );

	if( AdminTable2 )
	{
		// the improved admins table exists

		CONSOLE_Print( "[SQLITE3] found improved admins table" );
	} else UpgradeAdminTable();

	CONSOLE_Print( "[SQLITE3] checking score table" );
	bool ScoreTable = false;
	m_DB->Prepare( "SELECT category FROM scores", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself
		if (RC == SQLITE_ERROR)
		{
			CONSOLE_Print( "[SQLITE3] old scores table" );
			ScoreTable = false; 
		}
		else
			ScoreTable = true;

		m_DB->Finalize( Statement );
	}
	else;
//		CONSOLE_Print( "[SQLITE3] prepare error looking for scores table - " + m_DB->GetError( ) );

	if( ScoreTable )
	{
		// the improved score table exists

		CONSOLE_Print( "[SQLITE3] found normal score table" );
	} else
		UpgradeScoresTable();

	CONSOLE_Print( "[SQLITE3] checking score table 2" );
	bool ScoreTable2 = false;
	m_DB->Prepare( "SELECT rank FROM scores", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself
		if (RC == SQLITE_ERROR)
		{
			CONSOLE_Print( "[SQLITE3] new scores table" );
			ScoreTable2 = true; 
		}
		else
			ScoreTable2 = false;

		m_DB->Finalize( Statement );
	}
	else ScoreTable2 = true;
//		CONSOLE_Print( "[SQLITE3] prepare error looking for scores table - " + m_DB->GetError( ) );

	if( ScoreTable2 )
	{
		// the improved score table exists

		CONSOLE_Print( "[SQLITE3] found normal score table" );
	} else UpgradeScoresTable();

	CONSOLE_Print( "[SQLITE3] checking safelist table" );
	bool SLTable = false;
	m_DB->Prepare( "SELECT * FROM safelist", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ROW )
			SLTable = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] no safelist table" );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error looking for safelist table - " + m_DB->GetError( ) );

	if( SLTable )
	{
		// safelist table exists

		CONSOLE_Print( "[SQLITE3] found safelist table" );
	} else CreateSafeListTable();

	CONSOLE_Print( "[SQLITE3] checking improved safelist table" );
	bool SLTable2 = false;
	m_DB->Prepare( "SELECT voucher FROM safelist", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ROW )
			SLTable2 = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] no improved safelist table" );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error looking for safelist table - " + m_DB->GetError( ) );

	if( SLTable2 )
	{
		// improved safelist table exists

		CONSOLE_Print( "[SQLITE3] found improved safelist table" );
	} else UpgradeSafeListTable();

	CONSOLE_Print( "[SQLITE3] checking notes table" );
	bool NotesTable = false;
	m_DB->Prepare( "SELECT * FROM notes", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ROW )
			NotesTable = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] no notes table" );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error looking for notes table - " + m_DB->GetError( ) );

	if( NotesTable )
	{
		// notes table exists

		CONSOLE_Print( "[SQLITE3] found notes table" );
	} else CreateNotesTable();



	if( m_DB->Exec( "CREATE TEMPORARY TABLE iptocountry ( ip1 INTEGER NOT NULL, ip2 INTEGER NOT NULL, country TEXT NOT NULL, PRIMARY KEY ( ip1, ip2 ) )" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating temporary iptocountry table - " + m_DB->GetError( ) );

	FromAddStmt = NULL;
}

CGHostDBSQLite :: ~CGHostDBSQLite( )
{
	if( FromAddStmt )
		m_DB->Finalize( FromAddStmt );

	CONSOLE_Print( "[SQLITE3] closing database [" + m_File + "]" );
	delete m_DB;
}

void CGHostDBSQLite :: Upgrade1_2( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v1 to v2 started" );

	// add new column to table dotaplayers
	//  + hero TEXT NOT NULL DEFAULT ""

	if( m_DB->Exec( "ALTER TABLE dotaplayers ADD hero TEXT NOT NULL DEFAULT \"\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column hero to table dotaplayers - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column hero to table dotaplayers" );

	// add new columns to table dotagames
	//  + min INTEGER NOT NULL DEFAULT 0
	//  + sec INTEGER NOT NULL DEFAULT 0

	if( m_DB->Exec( "ALTER TABLE dotagames ADD min INTEGER NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column min to table dotagames - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column min to table dotagames" );

	if( m_DB->Exec( "ALTER TABLE dotagames ADD sec INTEGER NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column sec to table dotagames - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column sec to table dotagames" );

	// add new table config

	if( m_DB->Exec( "CREATE TABLE config ( name TEXT NOT NULL PRIMARY KEY, value TEXT NOT NULL )" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating config table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] created config table" );

	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO config VALUES ( \"schema_number\", \"2\" )", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			CONSOLE_Print( "[SQLITE3] inserted schema number [2]" );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error inserting schema number [2] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error inserting schema number [2] - " + m_DB->GetError( ) );

	CONSOLE_Print( "[SQLITE3] schema upgrade v1 to v2 finished" );
}

void CGHostDBSQLite :: Upgrade2_3( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v2 to v3 started" );

	// add new column to table admins
	//  + server TEXT NOT NULL DEFAULT ""

	if( m_DB->Exec( "ALTER TABLE admins ADD server TEXT NOT NULL DEFAULT \"\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column server to table admins - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column server to table admins" );

	// add new column to table games
	//  + gamestate INTEGER NOT NULL DEFAULT 0

	if( m_DB->Exec( "ALTER TABLE games ADD gamestate INTEGER NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column gamestate to table games - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column gamestate to table games" );

	// add new column to table gameplayers
	//  + spoofedrealm TEXT NOT NULL DEFAULT ""

	if( m_DB->Exec( "ALTER TABLE gameplayers ADD spoofedrealm TEXT NOT NULL DEFAULT \"\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column spoofedrealm to table gameplayers - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column spoofedrealm to table gameplayers" );

	// update schema number

	if( m_DB->Exec( "UPDATE config SET value=\"3\" where name=\"schema_number\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error updating schema number [3] - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] updated schema number [3]" );

	CONSOLE_Print( "[SQLITE3] schema upgrade v2 to v3 finished" );
}

void CGHostDBSQLite :: Upgrade3_4( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v3 to v4 started" );

	// add new columns to table games
	//  + creatorname TEXT NOT NULL DEFAULT ""
	//  + creatorserver TEXT NOT NULL DEFAULT ""

	if( m_DB->Exec( "ALTER TABLE games ADD creatorname TEXT NOT NULL DEFAULT \"\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column creatorname to table games - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column creatorname to table games" );

	if( m_DB->Exec( "ALTER TABLE games ADD creatorserver TEXT NOT NULL DEFAULT \"\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column creatorserver to table games - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column creatorserver to table games" );

	// add new table downloads

	if( m_DB->Exec( "CREATE TABLE downloads ( id INTEGER PRIMARY KEY, map TEXT NOT NULL, mapsize INTEGER NOT NULL, datetime TEXT NOT NULL, name TEXT NOT NULL, ip TEXT NOT NULL, spoofed INTEGER NOT NULL, spoofedrealm TEXT NOT NULL, downloadtime INTEGER NOT NULL )" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating downloads table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] created downloads table" );

	// update schema number

	if( m_DB->Exec( "UPDATE config SET value=\"4\" where name=\"schema_number\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error updating schema number [4] - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] updated schema number [4]" );

	CONSOLE_Print( "[SQLITE3] schema upgrade v3 to v4 finished" );
}

void CGHostDBSQLite :: Upgrade4_5( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v4 to v5 started" );

	// add new column to table dotaplayers
	//  + newcolour NOT NULL DEFAULT 0

	if( m_DB->Exec( "ALTER TABLE dotaplayers ADD newcolour NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column newcolour to table dotaplayers - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column newcolour to table dotaplayers" );

	// set newcolour = colour on all existing dotaplayers rows

	if( m_DB->Exec( "UPDATE dotaplayers SET newcolour=colour WHERE newcolour=0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error setting newcolour = colour on all existing dotaplayers rows - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] set newcolour = colour on all existing dotaplayers rows" );

	// update schema number

	if( m_DB->Exec( "UPDATE config SET value=\"5\" where name=\"schema_number\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error updating schema number [5] - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] updated schema number [5]" );

	CONSOLE_Print( "[SQLITE3] schema upgrade v4 to v5 finished" );
}

void CGHostDBSQLite :: Upgrade5_6( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v5 to v6 started" );

	// add new columns to table dotaplayers
	//  + towerkills NOT NULL DEFAULT 0
	//  + raxkills NOT NULL DEFAULT 0
	//  + courierkills NOT NULL DEFAULT 0

	if( m_DB->Exec( "ALTER TABLE dotaplayers ADD towerkills NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column towerkills to table dotaplayers - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column towerkills to table dotaplayers" );

	if( m_DB->Exec( "ALTER TABLE dotaplayers ADD raxkills NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column raxkills to table dotaplayers - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column raxkills to table dotaplayers" );

	if( m_DB->Exec( "ALTER TABLE dotaplayers ADD courierkills NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column courierkills to table dotaplayers - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column courierkills to table dotaplayers" );

	// update schema number

	if( m_DB->Exec( "UPDATE config SET value=\"6\" where name=\"schema_number\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error updating schema number [6] - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] updated schema number [6]" );

	CONSOLE_Print( "[SQLITE3] schema upgrade v5 to v6 finished" );
}

void CGHostDBSQLite :: Upgrade6_7( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v6 to v7 started" );

	// add new index to table gameplayers

	if( m_DB->Exec( "CREATE INDEX idx_gameid ON gameplayers ( gameid )" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating idx_gameid index on gameplayers table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new index idx_gameid to table gameplayers" );

	// add new index to table dotaplayers

	if( m_DB->Exec( "CREATE INDEX idx_gameid_colour ON dotaplayers ( gameid, colour )" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating idx_gameid_colour index on dotaplayers table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new index idx_gameid_colour to table dotaplayers" );

	// update schema number

	if( m_DB->Exec( "UPDATE config SET value=\"7\" where name=\"schema_number\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error updating schema number [7] - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] updated schema number [7]" );

	CONSOLE_Print( "[SQLITE3] schema upgrade v6 to v7 finished" );
}

void CGHostDBSQLite :: Upgrade7_8( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v7 to v8 started" );

	// create new tables

	if( m_DB->Exec( "CREATE TABLE w3mmdplayers ( id INTEGER PRIMARY KEY, category TEXT NOT NULL, gameid INTEGER NOT NULL, pid INTEGER NOT NULL, name TEXT NOT NULL, flag TEXT NOT NULL, leaver INTEGER NOT NULL, practicing INTEGER NOT NULL )" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating w3mmdplayers table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] created w3mmdplayers table" );

	if( m_DB->Exec( "CREATE TABLE w3mmdvars ( id INTEGER PRIMARY KEY, gameid INTEGER NOT NULL, pid INTEGER NOT NULL, varname TEXT NOT NULL, value_int INTEGER DEFAULT NULL, value_real REAL DEFAULT NULL, value_string TEXT DEFAULT NULL )" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating w3mmdvars table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] created w3mmdvars table" );

	// update schema number

	if( m_DB->Exec( "UPDATE config SET value=\"8\" where name=\"schema_number\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error updating schema number [8] - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] updated schema number [8]" );

	CONSOLE_Print( "[SQLITE3] schema upgrade v7 to v8 finished" );
}

void CGHostDBSQLite :: Upgrade_Tempban( )
{
	CONSOLE_Print( "[SQLITE3] schema upgrade v7 to v7_b started" );

	// add a gamenum (num of total games by player when this warn occured) to the bans
	if( m_DB->Exec( "ALTER TABLE bans ADD gamecount INTEGER NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding the gamenum column on bans table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column gamenum to table bans" );

	// add an expiredate column to the bans
	if( m_DB->Exec( "ALTER TABLE bans ADD expiredate TEXT NOT NULL DEFAULT \"\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding the expiredate column on bans table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column expiredate to table bans" );

	// add a warn column to the bans
	if( m_DB->Exec( "ALTER TABLE bans ADD warn INTEGER NOT NULL DEFAULT 0" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding the [warn] column on bans table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column [warn] to table bans" );
	
	// update schema number
	if( m_DB->Exec( "UPDATE config SET value=\"7_b\" where name=\"schema_number\"" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error updating schema number [7_b] - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] updated schema number [7_b]" );

	CONSOLE_Print( "[SQLITE3] schema upgrade v7 to v7_b finished" );
}

void CGHostDBSQLite :: CreateSafeListTable( )
{
	CONSOLE_Print( "[SQLITE3] creating safelist table" );

	if( m_DB->Exec( "CREATE TABLE safelist ( id INTEGER PRIMARY KEY, server TEXT NOT NULL, name TEXT NOT NULL)" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating table safelist - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] created new table safelist" );
}

void CGHostDBSQLite :: CreateNotesTable( )
{
	CONSOLE_Print( "[SQLITE3] creating notes table" );

	if( m_DB->Exec( "CREATE TABLE notes ( id INTEGER PRIMARY KEY, server TEXT NOT NULL, name TEXT NOT NULL, voucher TEXT NOT NULL DEFAULT \"\")" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating table notes - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] created new table notes" );
}

void CGHostDBSQLite :: UpgradeAdminTable( )
{
	CONSOLE_Print( "[SQLITE3] upgrading admin table" );

	// add new columns to table admins
	//  + access INT64 DEFAULT 4294963199
	string squery = "ALTER TABLE admins ADD access INT64 DEFAULT "+UTIL_ToString(m_AdminAccess);
	if( m_DB->Exec( squery ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error adding new column access to table admins - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] added new column access to table admins" );

	CONSOLE_Print( "[SQLITE3] admin upgrade finished" );
}

void CGHostDBSQLite :: UpgradeSafeListTable( )
{
	CONSOLE_Print( "[SQLITE3] upgrading safelist table" );

	if( m_DB->Exec( "ALTER TABLE safelist ADD voucher TEXT NOT NULL DEFAULT \"\" ; " ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error upgrading safelist table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] safelist table has been upgraded" );
}


void CGHostDBSQLite :: UpgradeScoresTable( )
{
	CONSOLE_Print( "[SQLITE3] upgrading scores table" );
	m_DB->Exec( "DROP TABLE scores;");

	if( m_DB->Exec( "CREATE TABLE scores (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, category VARCHAR(25) NOT NULL, name VARCHAR(15) NOT NULL, server VARCHAR(100) NOT NULL, score REAL NOT NULL);" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error creating scores table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] created a new score table" );
}

void CGHostDBSQLite :: UpgradeScoresTable2( )
{
	CONSOLE_Print( "[SQLITE3] upgrading scores table2" );

	if( m_DB->Exec( "ALTER TABLE scores DROP COLUMN rank" ) != SQLITE_OK )
		CONSOLE_Print( "[SQLITE3] error modifying scores table - " + m_DB->GetError( ) );
	else
		CONSOLE_Print( "[SQLITE3] modified the score table" );
}

bool CGHostDBSQLite :: Begin( )
{
	return m_DB->Exec( "BEGIN TRANSACTION" ) == SQLITE_OK;
}

bool CGHostDBSQLite :: Commit( )
{
	return m_DB->Exec( "COMMIT TRANSACTION" ) == SQLITE_OK;
}

uint32_t CGHostDBSQLite :: AdminCount( string server )
{
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(*) FROM admins WHERE server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
			Count = sqlite3_column_int( Statement, 0 );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting admins [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error counting admins [" + server + "] - " + m_DB->GetError( ) );

	return Count;
}

bool CGHostDBSQLite :: AdminCheck( string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	m_LastAccess = 0;
	bool IsAdmin = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT access FROM admins WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ROW )
		{			
			vector<string> *Row = m_DB->GetRow( );
			m_LastAccess = UTIL_ToUInt32((*Row)[0]);
			IsAdmin = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking admin [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking admin [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return IsAdmin;
}

bool CGHostDBSQLite :: AdminSetAccess( string server, string user, uint32_t access )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	sqlite3_stmt *Statement;
	bool ok = false;
	m_DB->Prepare( "UPDATE admins SET access=? WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_int( Statement, 1, access);
		sqlite3_bind_text( Statement, 2, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ROW )
		{
//			vector<string> *Row = m_DB->GetRow( );
			ok = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error setting admin access[" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error setting admin access[" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return ok;
}


uint32_t CGHostDBSQLite :: AdminAccess( string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	uint32_t Access = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT access FROM admins WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

		if( Row->size( ) == 1 )
			Access = UTIL_ToUInt32((*Row)[0]);
			m_LastAccess = Access;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking admin access[" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking admin access[" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Access;
}

bool CGHostDBSQLite :: SafeCheck( string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool IsSafe = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT name FROM safelist WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		// we're just checking to see if the query returned a row, we don't need to check the row data itself

		if( RC == SQLITE_ROW )
		{			
			vector<string> *Row = m_DB->GetRow( );
			IsSafe = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking safelisted player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking safelisted player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return IsSafe;
}


bool CGHostDBSQLite :: SafeAdd( string server, string user, string voucher )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	transform( voucher.begin( ), voucher.end( ), voucher.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO safelist ( server, name, voucher) VALUES ( ?, ?, ?)", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, voucher.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding safelisted player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding safelisted player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: SafeRemove( string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "DELETE FROM safelist WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
		{
			uint32_t rows = m_DB->Changes();
			if (rows>0)
				Success = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error removing safelisted player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error removing safelisted player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: NoteAdd( string server, string user, string note )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO notes ( server, name, note) VALUES ( ?, ?)", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, note.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding notes for player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding notes for player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: NoteUpdate( string server, string user, string note )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "UPDATE notes SET note=? WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, note.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error updating notes for player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error updating notes for player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: NoteRemove( string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "DELETE FROM notes WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
		{
			uint32_t rows = m_DB->Changes();
			if (rows>0)
				Success = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error removing notes for player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error removing notes for player [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: CalculateScores( string formula, string mingames )
{
	bool Success = false;
	sqlite3_stmt *Statement;
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

	m_DB->Prepare( Query3, (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );
		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error calculating scores [" + server + "] - " + m_DB->GetError( ) );
		m_DB->Finalize( Statement );
	}

	m_DB->Prepare( Query, (void **)&Statement );

	if( Statement )
	{
//		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
//		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 3 )
			{
				names.push_back((*Row)[0]);
				servers.push_back((*Row)[1]);
				scores.push_back(UTIL_ToDouble( (*Row)[2]));
			}
			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error calculating scores [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error calculating scores [" + server + "] - " + m_DB->GetError( ) );
	if (Success)
		for (uint32_t i = 0; i<names.size();i++)
		{
			Query2 = "INSERT into scores (category,name,server,score) VALUES ('dota_elo','"+names[i]+"','"+servers[i]+"',"+UTIL_ToString(scores[i],6)+")";
			m_DB->Prepare( Query2, (void **)&Statement );
			if( Statement )
			{
				int RC = m_DB->Step( Statement );
				if( RC == SQLITE_DONE )
					Success = true;
				else if( RC == SQLITE_ERROR )
					CONSOLE_Print( "[SQLITE3] error calculating scores [" + server + "] - " + m_DB->GetError( ) );
			}
			else
				CONSOLE_Print( "[SQLITE3] prepare error calculating scores [" + server + "] - " + m_DB->GetError( ) );			
			m_DB->Finalize( Statement );
		}
	names.clear();
	servers.clear();
	scores.clear();
	return Success;
}

bool CGHostDBSQLite :: AdminAdd( string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	uint32_t acc = m_AdminAccess;
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO admins ( server, name, access ) VALUES ( ?, ?, ? )", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int64( Statement, 3, acc);
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding admin [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding admin [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: AdminRemove( string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "DELETE FROM admins WHERE server=? AND name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
		{
			uint32_t rows = m_DB->Changes();
			if (rows>0)
				Success = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error removing admin [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error removing admin [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

vector<string> CGHostDBSQLite :: AdminList( string server )
{
	vector<string> AdminList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT name FROM admins WHERE server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
			{
				AdminList.push_back( (*Row)[0] );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving admin list [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving admin list [" + server + "] - " + m_DB->GetError( ) );

	return AdminList;
}

vector<string> CGHostDBSQLite :: RemoveTempBanList( string server )
{
	vector<string> TempBanList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT name FROM bans WHERE server=? AND expiredate=date('now')", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
			{
				TempBanList.push_back( (*Row)[0] );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving temp ban list [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving temp ban list [" + server + "] - " + m_DB->GetError( ) );

	return TempBanList;
}

vector<string> CGHostDBSQLite :: RemoveBanListDay( string server, uint32_t day )
{
	vector<string> TempBanList;
	sqlite3_stmt *Statement;
	string Day = UTIL_ToString(day);
	m_DB->Prepare( "SELECT name FROM bans WHERE server=? AND date=date('now','-? day')", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, Day.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
			{
				TempBanList.push_back( (*Row)[0] );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving temp ban list [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving temp ban list [" + server + "] - " + m_DB->GetError( ) );

	sqlite3_stmt *Statement2;
	m_DB->Prepare( "DELETE FROM bans WHERE server=? AND date=date('now','-? day')", (void **)&Statement2 );

	if( Statement2 )
	{
		sqlite3_bind_text( Statement2, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement2, 2, Day.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement2 );

		if( RC == SQLITE_DONE )
		{
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error deleting bans [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement2 );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error deleting bans [" + server + "] - " + m_DB->GetError( ) );

	return TempBanList;
}

vector<uint32_t> CGHostDBSQLite :: AccessesList( string server )
{
	vector<uint32_t> AccessesList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT name,access FROM admins WHERE server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 2 )
			{
				AccessesList.push_back( UTIL_ToUInt32((*Row)[1]) );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving accesses list [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving accesses list [" + server + "] - " + m_DB->GetError( ) );

	return AccessesList;
}

vector<string> CGHostDBSQLite :: SafeList( string server )
{
	vector<string> SafeList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT name FROM safelist WHERE server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
			{
				SafeList.push_back( (*Row)[0] );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving safe list [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving safe list [" + server + "] - " + m_DB->GetError( ) );

	return SafeList;
}

vector<string> CGHostDBSQLite :: SafeListV( string server )
{
	vector<string> SafeList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT voucher FROM safelist WHERE server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
			{
				SafeList.push_back( (*Row)[0] );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving safe list [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving safe list [" + server + "] - " + m_DB->GetError( ) );

	return SafeList;
}

vector<string> CGHostDBSQLite :: Notes( string server )
{
	vector<string> NotesList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT name FROM notes WHERE server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
			{
				NotesList.push_back( (*Row)[0] );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving notes [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving notes [" + server + "] - " + m_DB->GetError( ) );

	return NotesList;
}

vector<string> CGHostDBSQLite :: NotesN( string server )
{
	vector<string> NotesList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT note FROM notes WHERE server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
			{
				NotesList.push_back( (*Row)[0] );
			}

			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving notes [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving notes [" + server + "] - " + m_DB->GetError( ) );

	return NotesList;
}

uint32_t CGHostDBSQLite :: TodayGamesCount( )
{
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(*) FROM games WHERE JULIANDAY(datetime) = JULIANDAY('now')", (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
			Count = sqlite3_column_int( Statement, 0 );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting games - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error counting games - " + m_DB->GetError( ) );

	return Count;
}

uint32_t CGHostDBSQLite :: BanCount( string server )
{
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(*) FROM bans WHERE server=? AND warn=0 AND (expiredate == '' OR JULIANDAY(expiredate) > JULIANDAY('now'))", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
			Count = sqlite3_column_int( Statement, 0 );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting bans [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error counting bans [" + server + "] - " + m_DB->GetError( ) );

	return Count;
}

uint32_t CGHostDBSQLite :: BanCount( string name, uint32_t warn )
{
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(*) FROM bans WHERE name=? AND warn=? AND (expiredate == '' OR JULIANDAY(expiredate) > JULIANDAY('now'))", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 2, warn );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
			Count = sqlite3_column_int( Statement, 0 );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting warns/bans - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error counting warns/bans - " + m_DB->GetError( ) );

	return Count;
}

uint32_t CGHostDBSQLite :: BanCount( string server, string name, uint32_t warn )
{
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(*) FROM bans WHERE server=? AND name=? AND warn=? AND (expiredate == '' OR JULIANDAY(expiredate) > JULIANDAY('now'))", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, name.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 3, warn );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
			Count = sqlite3_column_int( Statement, 0 );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting warns/bans [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error counting warns/bans [" + server + "] - " + m_DB->GetError( ) );

	return Count;
}

string CGHostDBSQLite :: Ranks ( string server )
{
	string Result = string();
	uint32_t Rank = 1;
	sqlite3_stmt *Statement;
//	m_DB->Prepare( "SELECT name,score FROM scores WHERE server=? ORDER BY score desc LIMIT 0,9", (void **)&Statement );
	m_DB->Prepare( "SELECT name,score FROM scores ORDER BY score desc LIMIT 0,15", (void **)&Statement );

	if( Statement )
	{
		vector<string> *Row;
		
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		while (m_DB->Step( Statement ) == SQLITE_ROW )
		{
			Row = m_DB->GetRow( );

			if( Row->size( ) == 2 )
			{
				if (!Result.empty())
					Result +=" ";
				Result += "["+UTIL_ToString(Rank)+"]"+ (*Row)[0]+"=" + UTIL_ToString(strtod( ((*Row)[1]).c_str(),NULL ));
			}
			else
				CONSOLE_Print( "[SQLITE3] error checking Ranks - row doesn't have 3 columns" );
			Rank++;
		}
//		else ( RC == SQLITE_ERROR )
			//CONSOLE_Print( "[SQLITE3] error checking Ranks - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] No top calculated yet!");
//		CONSOLE_Print( "[SQLITE3] prepare error checking Ranks - " + m_DB->GetError( ) );
	return Result;
};

uint32_t CGHostDBSQLite :: ScoresCount ( string server)
{
	sqlite3_stmt *Statement;
//	m_DB->Prepare( "SELECT count(1) FROM scores WHERE server=?", (void **)&Statement );
	m_DB->Prepare( "SELECT count(1) FROM scores", (void **)&Statement );

	uint32_t Count = 0;
	if( Statement )
	{
//		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );
			if( Row->size( ) == 1 )
				Count = UTIL_ToUInt32((*Row)[0]);
			else
				CONSOLE_Print( "[SQLITE3] error checking scores count - row doesn't have 1 columns" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking scores count - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking scores count - " + m_DB->GetError( ) );

	return Count;
}

CDBScoreSummary *CGHostDBSQLite :: ScoreCheck( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	CDBScoreSummary *Score = NULL;
	sqlite3_stmt *Statement;
	double sc = -10000.00;
	uint32_t rank = 0;
	m_DB->Prepare( "SELECT score FROM scores WHERE name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
				sc = UTIL_ToDouble((*Row)[0]);
			else
				CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - row doesn't have 2 columns" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking scores [" + name + "] - " + m_DB->GetError( ) );

	if (sc!=-10000)
	{
		m_DB->Prepare( "SELECT count(*) as rank FROM scores WHERE score>?", (void **)&Statement );

		if( Statement )
		{
			sqlite3_bind_double( Statement, 1, sc);
			int RC = m_DB->Step( Statement );

			if( RC == SQLITE_ROW )
			{
				vector<string> *Row = m_DB->GetRow( );

				if( Row->size( ) == 1 )
				{
					rank = UTIL_ToUInt32((*Row)[0]);
					rank = rank + 1;
					Score = new CDBScoreSummary( name, sc, rank);
				}
				else
					CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - row doesn't have 2 columns" );
			}
			else if( RC == SQLITE_ERROR )
				CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - " + m_DB->GetError( ) );

			m_DB->Finalize( Statement );
		}
		else
			CONSOLE_Print( "[SQLITE3] prepare error checking scores [" + name + "] - " + m_DB->GetError( ) );
		//				Score = new CDBScoreSummary( name, strtod( ((*Row)[0]).c_str(),NULL ), UTIL_ToUInt32((*Row)[1]));
	}

	return Score;

}

CDBScoreSummary *CGHostDBSQLite :: ScoreCheck( string category, string name, string server)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	CDBScoreSummary *Score = NULL;
	sqlite3_stmt *Statement;
	double sc = -10000.00;
	uint32_t rank = 0;
	m_DB->Prepare( "SELECT score FROM scores WHERE category=? AND name=? AND server=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, category.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, name.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
				sc = UTIL_ToDouble((*Row)[0]);
			else
				CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - row doesn't have 1 column" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking scores [" + name + "] - " + m_DB->GetError( ) );

	if (sc!=-10000)
	{
		m_DB->Prepare( "SELECT count(*) as rank FROM scores WHERE score>?", (void **)&Statement );

		if( Statement )
		{
			sqlite3_bind_double( Statement, 1, sc);
			int RC = m_DB->Step( Statement );

			if( RC == SQLITE_ROW )
			{
				vector<string> *Row = m_DB->GetRow( );

				if( Row->size( ) == 1 )
				{
					rank = UTIL_ToUInt32((*Row)[0]);
					rank = rank + 1;
					Score = new CDBScoreSummary( name, sc, rank);
				}
				else
					CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - row doesn't have 2 columns" );
			}
			else if( RC == SQLITE_ERROR )
				CONSOLE_Print( "[SQLITE3] error checking scores [" + name + "] - " + m_DB->GetError( ) );

			m_DB->Finalize( Statement );
		}
		else
			CONSOLE_Print( "[SQLITE3] prepare error checking scores [" + name + "] - " + m_DB->GetError( ) );
		//				Score = new CDBScoreSummary( name, strtod( ((*Row)[0]).c_str(),NULL ), UTIL_ToUInt32((*Row)[1]));
	}

	return Score;

}

CDBBan *CGHostDBSQLite :: BanCheck( string server, string user, string ip, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	CDBBan *Ban = NULL;
	sqlite3_stmt *Statement;

	if( ip.empty( ) )
		m_DB->Prepare( "SELECT name, ip, date, gamename, admin, reason, expiredate FROM bans WHERE server=? AND name=? AND warn=? AND (JULIANDAY(expiredate) > JULIANDAY('now') OR expiredate == \"\")", (void **)&Statement );
	else
		m_DB->Prepare( "SELECT name, ip, date, gamename, admin, reason, expiredate FROM bans WHERE ((server=? AND name=?) OR ip=?) AND warn=? AND (JULIANDAY(expiredate) > JULIANDAY('now') OR expiredate == \"\")", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 3, warn );

		if( !ip.empty( ) )
			sqlite3_bind_text( Statement, 3, ip.c_str( ), -1, SQLITE_TRANSIENT );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 7 )
				Ban = new CDBBan( server, (*Row)[0], (*Row)[1], (*Row)[2], (*Row)[3], (*Row)[4], (*Row)[5], (*Row)[6] );
			else
				CONSOLE_Print( "[SQLITE3] error checking ban [" + server + " : " + user + " : " + ip + "] - row doesn't have 7 columns" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking ban [" + server + " : " + user + " : " + ip + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking ban [" + server + " : " + user + " : " + ip + "] - " + m_DB->GetError( ) );

	return Ban;
}

CDBBan *CGHostDBSQLite :: BanCheckIP( string server, string ip, uint32_t warn )
{
	CDBBan *Ban = NULL;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT ip, date, gamename, admin, reason, name, expiredate FROM bans WHERE server=? AND ip=? AND warn=? AND (JULIANDAY(expiredate) > JULIANDAY('now') OR expiredate == \"\")", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, ip.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 3, warn );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 7 )
				Ban = new CDBBan( server, (*Row)[5], ip, (*Row)[1], (*Row)[2], (*Row)[3], (*Row)[4], (*Row)[6]);
			else
				CONSOLE_Print( "[SQLITE3] error checking ban [" + server + " : " + ip + "] - row doesn't have 7 columns" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking ban [" + server + " : " + ip + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking ban [" + server + " : " + ip + "] - " + m_DB->GetError( ) );

	return Ban;
}

bool CGHostDBSQLite :: BanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn )
{
	uint32_t NumOfGames = CountPlayerGames( user );
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	// don't ban people on GArena by ip
	if (ip=="127.0.0.1")
		ip = "";
	string Query = "INSERT INTO bans ( server, name, ip, date, gamename, admin, reason, warn, gamecount, expiredate ) VALUES ( ?, ?, ?, date('now'), ?, ?, ?, ?, ?,'' )";
	if(expiredaytime > 0) // means the ban is not intended to be permanent
		Query = "INSERT INTO bans ( server, name, ip, date, gamename, admin, reason, warn, gamecount, expiredate ) VALUES ( ?, ?, ?, date('now'), ?, ?, ?, ?, ?, date(julianday('now') + ?))";
	m_DB->Prepare( Query, (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, ip.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 4, gamename.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 5, admin.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 6, reason.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 7, warn );
		sqlite3_bind_int( Statement, 8, NumOfGames );
		if(expiredaytime > 0)
			sqlite3_bind_int( Statement, 9, expiredaytime );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding ban [" + server + " : " + user + " : " + ip + " : " + gamename + " : " + admin + " : " + reason + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding ban [" + server + " : " + user + " : " + ip + " : " + gamename + " : " + admin + " : " + reason + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: BanRemove( string server, string user, string admin, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	string Query = "DELETE FROM bans WHERE server=? AND name=? AND warn=?";
	if (admin.length()>0)
		Query = "DELETE FROM bans WHERE server=? AND name=? AND warn=? AND admin=?";
	m_DB->Prepare( Query, (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 3, warn );
		if (admin.length()>0)
		sqlite3_bind_text( Statement, 4, admin.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
		{
			uint32_t rows = m_DB->Changes();
			if (rows>0)
				Success = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error removing ban [" + server + " : " + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error removing ban [" + server + " : " + user + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: BanRemove( string user, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	string Query = "DELETE FROM bans WHERE name=? AND warn=?";

	m_DB->Prepare( Query, (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 2, warn );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
		{
			uint32_t rows = m_DB->Changes();
			if (rows>0)
				Success = true;
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error removing ban [" + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error removing ban [" + user + "] - " + m_DB->GetError( ) );

	return Success;
}

uint32_t CGHostDBSQLite :: RemoveTempBans( string server )
{
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	string Query = "DELETE FROM bans WHERE server=? AND expiredate=date('now')";

	m_DB->Prepare( Query, (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
		{
			Count = m_DB->Changes();
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error removing temp bans - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error removing temp bans - " + m_DB->GetError( ) );

	return Count;
}

bool CGHostDBSQLite :: WarnUpdate( string user, uint32_t before, uint32_t after )
{
	// warns are set to 1 if active
	// 2 if inactive
	// 3 if inactive, but consider the ban of the player
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	if(before <= 1) // either active warns or bans
		m_DB->Prepare( "UPDATE bans SET warn=? WHERE name=? AND warn=? AND JULIANDAY(expiredate) > JULIANDAY('now')", (void **)&Statement );
	else
		m_DB->Prepare( "UPDATE bans SET warn=? WHERE name=? AND warn=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_int( Statement, 1, after );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 3, before );
		
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error hiding warns [" + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error hiding warns [" + user + "] - " + m_DB->GetError( ) );

	return Success;
}

uint32_t CGHostDBSQLite :: CountPlayerGames( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t NumOfGames = 0;

	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(*) FROM gameplayers WHERE name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			NumOfGames = sqlite3_column_int( Statement, 0 );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting games of the gameplayer - " + m_DB->GetError( ));
	}
	else
		CONSOLE_Print( "[SQLITE3] error counting games of the gameplayer - " + m_DB->GetError( ));

	return NumOfGames;
}

bool CGHostDBSQLite :: WarnForget( string name, uint32_t threshold )
{
	uint32_t NumOfGames = CountPlayerGames( name );
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	uint32_t LastWarn = 9999;

	sqlite3_stmt *Statement;
	
	if(NumOfGames >= threshold)
	{
		m_DB->Prepare( "SELECT MAX(gamecount) FROM bans WHERE name=? AND warn = 1 AND (JULIANDAY(expiredate) > JULIANDAY('now') OR expiredate == \"\")", (void **)&Statement );
	
		if( Statement )
		{
			sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );

			int RC = m_DB->Step( Statement );
			if( RC == SQLITE_ROW )
			{
				LastWarn = sqlite3_column_int( Statement, 0 );
			}
			else if( RC == SQLITE_ERROR )
				CONSOLE_Print( "[SQLITE3] error checking game number of the gameplayer - " + m_DB->GetError( ));
		}
		else
			CONSOLE_Print( "[SQLITE3] error checking game number of the gameplayer - " + m_DB->GetError( ));
	}

	if(LastWarn != 9999 && (NumOfGames - LastWarn) % threshold == 0)
	{
		// delete the oldest of the player's active warns

		m_DB->Prepare( "SELECT id FROM bans WHERE name=? AND warn = 1 AND (JULIANDAY(expiredate) > JULIANDAY('now') OR expiredate == \"\") ORDER BY id ASC LIMIT 1", (void **)&Statement );

		if( Statement )
		{
			sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );

			int RC = m_DB->Step( Statement );
			if( RC == SQLITE_ROW )
			{
				uint32_t id = sqlite3_column_int( Statement, 0 );
				m_DB->Prepare( "DELETE FROM bans WHERE id = ?", (void **)&Statement);
				sqlite3_bind_int( Statement, 1, id);
				m_DB->Step( Statement );
			}
		}

		return true;
	}
	
	return false;
}

bool CGHostDBSQLite :: BanUpdate( string user, string ip )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "UPDATE bans SET ip=? WHERE name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, ip.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, user.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error updating ban [" + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error updating ban [" + user + "] - " + m_DB->GetError( ) );

	return Success;
}

string CGHostDBSQLite :: RunQuery( string query )
{
	string Result = "OK ";
	sqlite3_stmt *Statement;
	m_DB->Prepare( query, (void **)&Statement );

	if( Statement )
	{
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
		{
			vector<string> *Row;
			Row = m_DB->GetRow( );
			for (uint32_t i=0; i<Row->size(); i++)
			{
				Result += (*Row)[i];
			}
		}
		else if( RC == SQLITE_ERROR )
		{
			CONSOLE_Print( "[SQLITE3] error running query [" + query + "] - " + m_DB->GetError( ) );
			Result = "Error running query: "+m_DB->GetError( );
		}

		m_DB->Finalize( Statement );
	}
	else
	{
		CONSOLE_Print( "[SQLITE3] prepare error running query [" + query + "] - " + m_DB->GetError( ) );
		Result = "Error preparing query: "+m_DB->GetError( );
	}
	return Result;
}

vector<CDBBan *> CGHostDBSQLite :: BanList( string server )
{
	vector<CDBBan *> BanList;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT name, ip, date, gamename, admin, reason, expiredate FROM bans WHERE warn='0' AND server=? ORDER BY name", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		string bb;
		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 7 )
			{
				BanList.push_back( new CDBBan( server, (*Row)[0], (*Row)[1], (*Row)[2], (*Row)[3], (*Row)[4], (*Row)[5], (*Row)[6] ) );
			}
			RC = m_DB->Step( Statement );
		}

		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error retrieving ban list [" + server + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error retrieving ban list [" + server + "] - " + m_DB->GetError( ) );

	return BanList;
}

uint32_t CGHostDBSQLite :: GameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	uint32_t RowID = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO games ( server, map, datetime, gamename, ownername, duration, gamestate, creatorname, creatorserver ) VALUES ( ?, ?, datetime('now','localtime'), ?, ?, ?, ?, ?, ? )", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, server.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 2, map.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, gamename.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 4, ownername.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 5, duration );
		sqlite3_bind_int( Statement, 6, gamestate );
		sqlite3_bind_text( Statement, 7, creatorname.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 8, creatorserver.c_str( ), -1, SQLITE_TRANSIENT );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			RowID = m_DB->LastRowID( );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding game [" + server + " : " + map + " : " + gamename + " : " + ownername + " : " + UTIL_ToString( duration ) + " : " + UTIL_ToString( gamestate ) + " : " + creatorname + " : " + creatorserver + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding game [" + server + " : " + map + " : " + gamename + " : " + ownername + " : " + UTIL_ToString( duration ) + " : " + UTIL_ToString( gamestate ) + " : " + creatorname + " : " + creatorserver + "] - " + m_DB->GetError( ) );

	return RowID;
}

uint32_t CGHostDBSQLite :: GamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t RowID = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO gameplayers ( gameid, name, ip, spoofed, reserved, loadingtime, left, leftreason, team, colour, spoofedrealm ) VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_int( Statement, 1, gameid );
		sqlite3_bind_text( Statement, 2, name.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 3, ip.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 4, spoofed );
		sqlite3_bind_int( Statement, 5, reserved );
		sqlite3_bind_int( Statement, 6, loadingtime );
		sqlite3_bind_int( Statement, 7, left );
		sqlite3_bind_text( Statement, 8, leftreason.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 9, team );
		sqlite3_bind_int( Statement, 10, colour );
		sqlite3_bind_text( Statement, 11, spoofedrealm.c_str( ), -1, SQLITE_TRANSIENT );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			RowID = m_DB->LastRowID( );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding gameplayer [" + UTIL_ToString( gameid ) + " : " + name + " : " + ip + " : " + UTIL_ToString( spoofed ) + " : " + spoofedrealm + " : " + UTIL_ToString( reserved ) + " : " + UTIL_ToString( loadingtime ) + " : " + UTIL_ToString( left ) + " : " + leftreason + " : " + UTIL_ToString( team ) + " : " + UTIL_ToString( colour ) + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding gameplayer [" + UTIL_ToString( gameid ) + " : " + name + " : " + ip + " : " + UTIL_ToString( spoofed ) + " : " + spoofedrealm + " : " + UTIL_ToString( reserved ) + " : " + UTIL_ToString( loadingtime ) + " : " + UTIL_ToString( left ) + " : " + leftreason + " : " + UTIL_ToString( team ) + " : " + UTIL_ToString( colour ) + "] - " + m_DB->GetError( ) );

	return RowID;
}

uint32_t CGHostDBSQLite :: GamePlayerCount( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(*) FROM gameplayers LEFT JOIN games ON games.id=gameid WHERE name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
			Count = sqlite3_column_int( Statement, 0 );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting gameplayers [" + name + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error counting gameplayers [" + name + "] - " + m_DB->GetError( ) );

	return Count;
}

CDBGamePlayerSummary *CGHostDBSQLite :: GamePlayerSummaryCheck( string name )
{
	if( GamePlayerCount( name ) == 0 )
		return NULL;

	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	CDBGamePlayerSummary *GamePlayerSummary = NULL;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT MIN(datetime), MAX(datetime), COUNT(*), MIN(loadingtime), AVG(loadingtime), MAX(loadingtime), MIN(left/CAST(duration AS REAL))*100, AVG(left/CAST(duration AS REAL))*100, MAX(left/CAST(duration AS REAL))*100, MIN(duration), AVG(duration), MAX(duration) FROM gameplayers LEFT JOIN games ON games.id=gameid WHERE name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			if( sqlite3_column_count( (sqlite3_stmt *)Statement ) == 12 )
			{
				char *First = (char *)sqlite3_column_text( (sqlite3_stmt *)Statement, 0 );
				char *Last = (char *)sqlite3_column_text( (sqlite3_stmt *)Statement, 1 );
				string FirstGameDateTime;
				string LastGameDateTime;

				if( First )
					FirstGameDateTime = First;

				if( Last )
					LastGameDateTime = Last;

				uint32_t TotalGames = sqlite3_column_int( (sqlite3_stmt *)Statement, 2 );
				uint32_t MinLoadingTime = sqlite3_column_int( (sqlite3_stmt *)Statement, 3 );
				uint32_t AvgLoadingTime = sqlite3_column_int( (sqlite3_stmt *)Statement, 4 );
				uint32_t MaxLoadingTime = sqlite3_column_int( (sqlite3_stmt *)Statement, 5 );
				uint32_t MinLeftPercent = sqlite3_column_int( (sqlite3_stmt *)Statement, 6 );
				uint32_t AvgLeftPercent = sqlite3_column_int( (sqlite3_stmt *)Statement, 7 );
				uint32_t MaxLeftPercent = sqlite3_column_int( (sqlite3_stmt *)Statement, 8 );
				uint32_t MinDuration = sqlite3_column_int( (sqlite3_stmt *)Statement, 9 );
				uint32_t AvgDuration = sqlite3_column_int( (sqlite3_stmt *)Statement, 10 );
				uint32_t MaxDuration = sqlite3_column_int( (sqlite3_stmt *)Statement, 11 );
				GamePlayerSummary = new CDBGamePlayerSummary( string( ), name, FirstGameDateTime, LastGameDateTime, TotalGames, MinLoadingTime, AvgLoadingTime, MaxLoadingTime, MinLeftPercent, AvgLeftPercent, MaxLeftPercent, MinDuration, AvgDuration, MaxDuration );
			}
			else
				CONSOLE_Print( "[SQLITE3] error checking gameplayersummary [" + name + "] - row doesn't have 12 columns" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking gameplayersummary [" + name + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking gameplayersummary [" + name + "] - " + m_DB->GetError( ) );

	return GamePlayerSummary;
}

uint32_t CGHostDBSQLite :: DotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	uint32_t RowID = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO dotagames ( gameid, winner, min, sec ) VALUES ( ?, ?, ?, ? )", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_int( Statement, 1, gameid );
		sqlite3_bind_int( Statement, 2, winner );
		sqlite3_bind_int( Statement, 3, min );
		sqlite3_bind_int( Statement, 4, sec );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			RowID = m_DB->LastRowID( );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding dotagame [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( winner ) + " : " + UTIL_ToString( min ) + " : " + UTIL_ToString( sec ) + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding dotagame [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( winner ) + " : " + UTIL_ToString( min ) + " : " + UTIL_ToString( sec ) + "] - " + m_DB->GetError( ) );

	return RowID;
}

uint32_t CGHostDBSQLite :: DotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills )
{
	uint32_t RowID = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO dotaplayers ( gameid, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolour, towerkills, raxkills, courierkills ) VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_int( Statement, 1, gameid );
		sqlite3_bind_int( Statement, 2, colour );
		sqlite3_bind_int( Statement, 3, kills );
		sqlite3_bind_int( Statement, 4, deaths );
		sqlite3_bind_int( Statement, 5, creepkills );
		sqlite3_bind_int( Statement, 6, creepdenies );
		sqlite3_bind_int( Statement, 7, assists );
		sqlite3_bind_int( Statement, 8, gold );
		sqlite3_bind_int( Statement, 9, neutralkills );
		sqlite3_bind_text( Statement, 10, item1.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 11, item2.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 12, item3.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 13, item4.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 14, item5.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 15, item6.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 16, hero.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 17, newcolour );
		sqlite3_bind_int( Statement, 18, towerkills );
		sqlite3_bind_int( Statement, 19, raxkills );
		sqlite3_bind_int( Statement, 20, courierkills );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			RowID = m_DB->LastRowID( );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding dotaplayer [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( colour ) + " : " + UTIL_ToString( kills ) + " : " + UTIL_ToString( deaths ) + " : " + UTIL_ToString( creepkills ) + " : " + UTIL_ToString( creepdenies ) + " : " + UTIL_ToString( assists ) + " : " + UTIL_ToString( gold ) + " : " + UTIL_ToString( neutralkills ) + " : " + item1 + " : " + item2 + " : " + item3 + " : " + item4 + " : " + item5 + " : " + item6 + " : " + hero + " : " + UTIL_ToString( newcolour ) + " : " + UTIL_ToString( towerkills ) + " : " + UTIL_ToString( raxkills ) + " : " + UTIL_ToString( courierkills ) + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding dotaplayer [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( colour ) + " : " + UTIL_ToString( kills ) + " : " + UTIL_ToString( deaths ) + " : " + UTIL_ToString( creepkills ) + " : " + UTIL_ToString( creepdenies ) + " : " + UTIL_ToString( assists ) + " : " + UTIL_ToString( gold ) + " : " + UTIL_ToString( neutralkills ) + " : " + item1 + " : " + item2 + " : " + item3 + " : " + item4 + " : " + item5 + " : " + item6 + " : " + hero + " : " + UTIL_ToString( newcolour ) + " : " + UTIL_ToString( towerkills ) + " : " + UTIL_ToString( raxkills ) + " : " + UTIL_ToString( courierkills ) + "] - " + m_DB->GetError( ) );

	return RowID;
}

uint32_t CGHostDBSQLite :: DotAPlayerCount( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t Count = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT COUNT(dotaplayers.id) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour WHERE name=?", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
			Count = sqlite3_column_int( Statement, 0 );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error counting dotaplayers [" + name + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error counting dotaplayers [" + name + "] - " + m_DB->GetError( ) );

	return Count;
}

CDBDotAPlayerSummary *CGHostDBSQLite :: DotAPlayerSummaryCheck( string name, string formula, string mingames, string gamestate )
{
	if( DotAPlayerCount( name ) == 0 )
		return NULL;

	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	CDBDotAPlayerSummary *DotAPlayerSummary = NULL;
	bool Success = true;
	string server = string();
	sqlite3_stmt *Statement;
//	string Query = "select totgames,wins,losses,killstotal,deathstotal,creepkillstotal,creepdeniestotal,assiststotal,neutralkillstotal,towerkillstotal,raxkillstotal,courierkillstotal,kills,deaths,creepkills,creepdenies,assists,neutralkills,towerkills,raxkills,courierkills, ("+GetFormula()+") as totalscore,server from(select *, (kills/deaths) as killdeathratio, (totgames-wins) as losses from (select gp.name as name,ga.server as server,gp.gameid as gameid, gp.colour as colour, avg(dp.courierkills) as courierkills, sum(dp.raxkills) as raxkillstotal, sum(dp.towerkills) as towerkillstotal, sum(dp.assists) as assiststotal,sum(dp.courierkills) as courierkillstotal, sum(dp.creepdenies) as creepdeniestotal, sum(dp.creepkills) as creepkillstotal,sum(dp.neutralkills) as neutralkillstotal, sum(dp.deaths) as deathstotal, sum(dp.kills) as killstotal,avg(dp.raxkills) as raxkills,avg(dp.towerkills) as towerkills, avg(dp.assists) as assists, avg(dp.creepdenies) as creepdenies, avg(dp.creepkills) as creepkills,avg(dp.neutralkills) as neutralkills, avg(dp.deaths) as deaths, avg(dp.kills) as kills,count(*) as totgames, SUM(case when((dg.winner = 1 and dp.newcolour < 6) or (dg.winner = 2 and dp.newcolour > 6)) then 1 else 0 end) as wins from gameplayers as gp, dotagames as dg, games as ga,dotaplayers as dp where dg.winner <> 0 and dp.gameid = gp.gameid and dg.gameid = dp.gameid and dp.gameid = ga.id and gp.gameid = dg.gameid and gp.colour = dp.colour and gp.name='"+name+"' group by gp.name) as h) as i";
	string Query = "select totgames,wins,losses,killstotal,deathstotal,creepkillstotal,creepdeniestotal,assiststotal,neutralkillstotal,towerkillstotal,raxkillstotal,courierkillstotal,kills,deaths,creepkills,creepdenies,assists,neutralkills,towerkills,raxkills,courierkills, server from(select *, (kills/deaths) as killdeathratio, (totgames-wins) as losses from (select gp.name as name,ga.server as server,gp.gameid as gameid, gp.colour as colour, avg(dp.courierkills) as courierkills, sum(dp.raxkills) as raxkillstotal, sum(dp.towerkills) as towerkillstotal, sum(dp.assists) as assiststotal,sum(dp.courierkills) as courierkillstotal, sum(dp.creepdenies) as creepdeniestotal, sum(dp.creepkills) as creepkillstotal,sum(dp.neutralkills) as neutralkillstotal, sum(dp.deaths) as deathstotal, sum(dp.kills) as killstotal,avg(dp.raxkills) as raxkills,avg(dp.towerkills) as towerkills, avg(dp.assists) as assists, avg(dp.creepdenies) as creepdenies, avg(dp.creepkills) as creepkills,avg(dp.neutralkills) as neutralkills, avg(dp.deaths) as deaths, avg(dp.kills) as kills,count(*) as totgames, SUM(case when((dg.winner = 1 and dp.newcolour < 6) or (dg.winner = 2 and dp.newcolour > 6)) then 1 else 0 end) as wins from gameplayers as gp, dotagames as dg, games as ga,dotaplayers as dp where dg.winner <> 0 and dp.gameid = gp.gameid and dg.gameid = dp.gameid and dp.gameid = ga.id and gp.gameid = dg.gameid and gp.colour = dp.colour and gp.name='"+name+"' group by gp.name) as h) as i";
	if (gamestate!="")
		Query = "select totgames,wins,losses,killstotal,deathstotal,creepkillstotal,creepdeniestotal,assiststotal,neutralkillstotal,towerkillstotal,raxkillstotal,courierkillstotal,kills,deaths,creepkills,creepdenies,assists,neutralkills,towerkills,raxkills,courierkills, server from(select *, (kills/deaths) as killdeathratio, (totgames-wins) as losses from (select gp.name as name,ga.server as server,gp.gameid as gameid, gp.colour as colour, avg(dp.courierkills) as courierkills, sum(dp.raxkills) as raxkillstotal, sum(dp.towerkills) as towerkillstotal, sum(dp.assists) as assiststotal,sum(dp.courierkills) as courierkillstotal, sum(dp.creepdenies) as creepdeniestotal, sum(dp.creepkills) as creepkillstotal,sum(dp.neutralkills) as neutralkillstotal, sum(dp.deaths) as deathstotal, sum(dp.kills) as killstotal,avg(dp.raxkills) as raxkills,avg(dp.towerkills) as towerkills, avg(dp.assists) as assists, avg(dp.creepdenies) as creepdenies, avg(dp.creepkills) as creepkills,avg(dp.neutralkills) as neutralkills, avg(dp.deaths) as deaths, avg(dp.kills) as kills,count(*) as totgames, SUM(case when((dg.winner = 1 and dp.newcolour < 6) or (dg.winner = 2 and dp.newcolour > 6)) then 1 else 0 end) as wins from gameplayers as gp, dotagames as dg, games as ga,dotaplayers as dp where dg.winner <> 0 and dp.gameid = gp.gameid and dg.gameid = dp.gameid and dp.gameid = ga.id and gp.gameid = dg.gameid and gp.colour = dp.colour and gp.name='"+name+"' and ga.gamestate='"+ gamestate +"' group by gp.name) as h) as i";
	
	m_DB->Prepare( Query, (void **)&Statement );
	if( Statement )
	{
//		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			if( sqlite3_column_count( (sqlite3_stmt *)Statement ) == 22 )
			{
				uint32_t TotalGames = sqlite3_column_int( (sqlite3_stmt *)Statement, 0 );
				uint32_t TotalWins = sqlite3_column_int( (sqlite3_stmt *)Statement, 1 );
				uint32_t TotalLosses = sqlite3_column_int( (sqlite3_stmt *)Statement, 2 );
				uint32_t TotalKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 3 );
				uint32_t TotalDeaths = sqlite3_column_int( (sqlite3_stmt *)Statement, 4 );
				uint32_t TotalCreepKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 5 );
				uint32_t TotalCreepDenies = sqlite3_column_int( (sqlite3_stmt *)Statement, 6 );
				uint32_t TotalAssists = sqlite3_column_int( (sqlite3_stmt *)Statement, 7 );
				uint32_t TotalNeutralKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 8 );
				uint32_t TotalTowerKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 9 );
				uint32_t TotalRaxKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 10 );
				uint32_t TotalCourierKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 11 );
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
				char *srv = (char *)sqlite3_column_text( (sqlite3_stmt *)Statement, 21 );
				server = srv;
				double Score = -10000;
//				double Score = sqlite3_column_double( (sqlite3_stmt *)Statement, 21 );
				uint32_t Rank = 0;
				wpg = (double)TotalWins/TotalGames;
				lpg = (double)TotalLosses/TotalGames;
				wpg = wpg * 100;
				lpg = lpg * 100;
/*
				if (TotalGames>=GetMinGames())
				{

					string Query3 = "SELECT NAME FROM scores WHERE NAME='"+name+"'";
					sqlite3_stmt *Statement3;
					bool found = false;
					m_DB->Prepare( Query3, (void **)&Statement3 );
					if( Statement3 )
					{
						int RC3 = m_DB->Step( Statement3 );
						if( RC3 == SQLITE_ROW )
						{
							found = true;
						}
						m_DB->Finalize( Statement3 );

					}	
					string Query2 = "INSERT INTO scores (category,server,name,score) VALUES ('dota_elo','"+server+"','"+name+"',"+UTIL_ToString(Score)+ ")";
					if (found)
					Query2 = "UPDATE scores SET score="+UTIL_ToString(Score)+" WHERE name='"+name+"'";

					sqlite3_stmt *Statement2;
					m_DB->Prepare( Query2, (void **)&Statement2 );
					if( Statement2 )
					{
						int RC2 = m_DB->Step( Statement2 );
						if( RC2 == SQLITE_DONE )
						{

						}
						else 
							CONSOLE_Print( "[SQLITE3] error updating score [" + name + "]" );
						m_DB->Finalize( Statement2 );
					}				

					CDBScoreSummary *DotAScoreSummary = ScoreCheck(name);
					if (DotAScoreSummary)
					{
						Rank = DotAScoreSummary->GetRank();
						CONSOLE_Print("[SQLITE3] rank = "+ UTIL_ToString(Rank));
					}
					delete DotAScoreSummary;
				}
*/
				CDBScoreSummary *DotAScoreSummary = ScoreCheck(name);
				if (DotAScoreSummary)
				{
					if (TotalGames>=GetMinGames())
						Rank = DotAScoreSummary->GetRank();
					Score = DotAScoreSummary->GetScore();
				}
				delete DotAScoreSummary;

				DotAPlayerSummary = new CDBDotAPlayerSummary( string( ), name, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills, wpg,lpg, kpg, dpg, ckpg, cdpg, apg, nkpg, Score, tkpg, rkpg, coukpg, Rank );
			}
			else;
//				CONSOLE_Print( "[SQLITE3] error checking dotaplayersummary [" + name + "] - row doesn't have 23 columns" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking dotaplayersummary [" + name + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking dotaplayersummary [" + name + "] - " + m_DB->GetError( ) );

/*
	m_DB->Prepare( "SELECT COUNT(dotaplayers.id), SUM(kills), SUM(deaths), SUM(creepkills), SUM(creepdenies), SUM(assists), SUM(neutralkills), SUM(towerkills), SUM(raxkills), SUM(courierkills) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON dotagames.gameid=games.id WHERE name=? AND winner!=0", (void **)&Statement );
	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			if( sqlite3_column_count( (sqlite3_stmt *)Statement ) == 10 )
			{
				uint32_t TotalGames = sqlite3_column_int( (sqlite3_stmt *)Statement, 0 );
				uint32_t TotalWins = 0;
				uint32_t TotalLosses = 0;
				uint32_t TotalKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 1 );
				uint32_t TotalDeaths = sqlite3_column_int( (sqlite3_stmt *)Statement, 2 );
				uint32_t TotalCreepKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 3 );
				uint32_t TotalCreepDenies = sqlite3_column_int( (sqlite3_stmt *)Statement, 4 );
				uint32_t TotalAssists = sqlite3_column_int( (sqlite3_stmt *)Statement, 5 );
				uint32_t TotalNeutralKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 6 );
				uint32_t TotalTowerKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 7 );
				uint32_t TotalRaxKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 8 );
				uint32_t TotalCourierKills = sqlite3_column_int( (sqlite3_stmt *)Statement, 9 );

				// calculate total wins

				sqlite3_stmt *Statement2;
				m_DB->Prepare( "SELECT COUNT(*) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON games.id=dotagames.gameid WHERE name=? AND ((winner=1 AND dotaplayers.newcolour>=1 AND dotaplayers.newcolour<=5) OR (winner=2 AND dotaplayers.newcolour>=7 AND dotaplayers.newcolour<=11))", (void **)&Statement2 );

				if( Statement2 )
				{
					sqlite3_bind_text( Statement2, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
					int RC2 = m_DB->Step( Statement2 );

					if( RC2 == SQLITE_ROW )
						TotalWins = sqlite3_column_int( Statement2, 0 );
					else if( RC2 == SQLITE_ERROR )
						CONSOLE_Print( "[SQLITE3] error counting dotaplayersummary wins [" + name + "] - " + m_DB->GetError( ) );

					m_DB->Finalize( Statement2 );
				}
				else
					CONSOLE_Print( "[SQLITE3] prepare error counting dotaplayersummary wins [" + name + "] - " + m_DB->GetError( ) );

				// calculate total losses

				sqlite3_stmt *Statement3;
				m_DB->Prepare( "SELECT COUNT(*) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON games.id=dotagames.gameid WHERE name=? AND ((winner=2 AND dotaplayers.newcolour>=1 AND dotaplayers.newcolour<=5) OR (winner=1 AND dotaplayers.newcolour>=7 AND dotaplayers.newcolour<=11))", (void **)&Statement3 );

				if( Statement3 )
				{
					sqlite3_bind_text( Statement3, 1, name.c_str( ), -1, SQLITE_TRANSIENT );
					int RC3 = m_DB->Step( Statement3 );

					if( RC3 == SQLITE_ROW )
						TotalLosses = sqlite3_column_int( Statement3, 0 );
					else if( RC3 == SQLITE_ERROR )
						CONSOLE_Print( "[SQLITE3] error counting dotaplayersummary losses [" + name + "] - " + m_DB->GetError( ) );

					m_DB->Finalize( Statement3 );
				}
				else
					CONSOLE_Print( "[SQLITE3] prepare error counting dotaplayersummary losses [" + name + "] - " + m_DB->GetError( ) );

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
	
//					Score1 = (double)(TotalWins - TotalLosses)/TotalGames*5;
//					Score2 = ((double)((double)(TotalKills-TotalDeaths+TotalAssists)/4)/TotalGames);
//					Score3 = ((double)((double)(TotalCreepKills/50)+(double)(TotalCreepDenies/5)+(double)(TotalNeutralKills/30))/TotalGames);
					Score1 = TotalWins;
					Score1 = Score1 - (double)TotalLosses;
					Score1 = Score1/TotalGames;
					Score2 = kpg-dpg+apg/10;
					Score3 = ckpg/100+cdpg/10+nkpg/50;

					Score = Score1+Score2+Score3;

					wpg = wpg * 100;
					lpg = lpg * 100;

					DotAPlayerSummary = new CDBDotAPlayerSummary( string( ), name, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills, wpg,lpg, kpg, dpg, ckpg, cdpg, apg, nkpg, Score, tkpg, rkpg, coukpg );
				}
			}
			else
				CONSOLE_Print( "[SQLITE3] error checking dotaplayersummary [" + name + "] - row doesn't have 7 columns" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking dotaplayersummary [" + name + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking dotaplayersummary [" + name + "] - " + m_DB->GetError( ) );
*/
	return DotAPlayerSummary;
}

string CGHostDBSQLite :: FromCheck( uint32_t ip )
{
	// a big thank you to tjado for help with the iptocountry feature

	string From = "??";
	sqlite3_stmt *Statement;
	m_DB->Prepare( "SELECT country FROM iptocountry WHERE ip1<=? AND ip2>=?", (void **)&Statement );

	if( Statement )
	{
		// we bind the ip as an int64 because SQLite treats it as signed

		sqlite3_bind_int64( Statement, 1, ip );
		sqlite3_bind_int64( Statement, 2, ip );
		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );

			if( Row->size( ) == 1 )
				From = (*Row)[0];
			else
				CONSOLE_Print( "[SQLITE3] error checking iptocountry [" + UTIL_ToString( ip ) + "] - row doesn't have 1 column" );
		}
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking iptocountry [" + UTIL_ToString( ip ) + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking iptocountry [" + UTIL_ToString( ip ) + "] - " + m_DB->GetError( ) );

	return From;
}

string CGHostDBSQLite :: WarnReasonsCheck( string user, uint32_t warn )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	sqlite3_stmt *Statement;
	if(warn <= 1) // either active warns or bans
		m_DB->Prepare( "SELECT (reason) FROM bans WHERE name=? AND warn=? AND (JULIANDAY(expiredate) > JULIANDAY('now') OR expiredate == \"\")", (void **)&Statement );
	else
		m_DB->Prepare( "SELECT (reason) FROM bans WHERE name=? AND warn=?", (void **)&Statement );

	string result = "";

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, user.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 2, warn );

		bool firstreason = true;

		int RC = m_DB->Step( Statement );
		
		while( RC == SQLITE_ROW )
		{
			vector<string> *Row = m_DB->GetRow( );
			if(!firstreason)
			{
					result += " | ";
			}
			firstreason = false;

			result += (*Row)[0];

			RC = m_DB->Step( Statement );
		}
		if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error checking warn reasons [" + user + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error checking warn reasons [" + user + "] - " + m_DB->GetError( ) );

	return result;
}

bool CGHostDBSQLite :: FromAdd( uint32_t ip1, uint32_t ip2, string country )
{
	// a big thank you to tjado for help with the iptocountry feature

	bool Success = false;

	if( !FromAddStmt )
		m_DB->Prepare( "INSERT INTO iptocountry VALUES ( ?, ?, ? )", (void **)&FromAddStmt );

	if( FromAddStmt )
	{
		// we bind the ip as an int64 because SQLite treats it as signed

		sqlite3_bind_int64( (sqlite3_stmt *)FromAddStmt, 1, ip1 );
		sqlite3_bind_int64( (sqlite3_stmt *)FromAddStmt, 2, ip2 );
		sqlite3_bind_text( (sqlite3_stmt *)FromAddStmt, 3, country.c_str( ), -1, SQLITE_TRANSIENT );

		int RC = m_DB->Step( FromAddStmt );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding iptocountry [" + UTIL_ToString( ip1 ) + " : " + UTIL_ToString( ip2 ) + " : " + country + "] - " + m_DB->GetError( ) );

		m_DB->Reset( FromAddStmt );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding iptocountry [" + UTIL_ToString( ip1 ) + " : " + UTIL_ToString( ip2 ) + " : " + country + "] - " + m_DB->GetError( ) );

	return Success;
}

bool CGHostDBSQLite :: DownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	bool Success = false;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO downloads ( map, mapsize, datetime, name, ip, spoofed, spoofedrealm, downloadtime ) VALUES ( ?, ?, datetime('now'), ?, ?, ?, ?, ? )", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, map.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 2, mapsize );
		sqlite3_bind_text( Statement, 3, name.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 4, ip.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 5, spoofed );
		sqlite3_bind_text( Statement, 6, spoofedrealm.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 7, downloadtime );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			Success = true;
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding download [" + map + " : " + UTIL_ToString( mapsize ) + " : " + name + " : " + ip + " : " + UTIL_ToString( spoofed ) + " : " + spoofedrealm + " : " + UTIL_ToString( downloadtime ) + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding download [" + map + " : " + UTIL_ToString( mapsize ) + " : " + name + " : " + ip + " : " + UTIL_ToString( spoofed ) + " : " + spoofedrealm + " : " + UTIL_ToString( downloadtime ) + "] - " + m_DB->GetError( ) );

	return Success;
}

uint32_t CGHostDBSQLite :: W3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	uint32_t RowID = 0;
	sqlite3_stmt *Statement;
	m_DB->Prepare( "INSERT INTO w3mmdplayers ( category, gameid, pid, name, flag, leaver, practicing ) VALUES ( ?, ?, ?, ?, ?, ?, ? )", (void **)&Statement );

	if( Statement )
	{
		sqlite3_bind_text( Statement, 1, category.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 2, gameid );
		sqlite3_bind_int( Statement, 3, pid );
		sqlite3_bind_text( Statement, 4, name.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_text( Statement, 5, flag.c_str( ), -1, SQLITE_TRANSIENT );
		sqlite3_bind_int( Statement, 6, leaver );
		sqlite3_bind_int( Statement, 7, practicing );

		int RC = m_DB->Step( Statement );

		if( RC == SQLITE_DONE )
			RowID = m_DB->LastRowID( );
		else if( RC == SQLITE_ERROR )
			CONSOLE_Print( "[SQLITE3] error adding w3mmdplayer [" + category + " : " + UTIL_ToString( gameid ) + " : " + UTIL_ToString( pid ) + " : " + name + " : " + flag + " : " + UTIL_ToString( leaver ) + " : " + UTIL_ToString( practicing ) + "] - " + m_DB->GetError( ) );

		m_DB->Finalize( Statement );
	}
	else
		CONSOLE_Print( "[SQLITE3] prepare error adding w3mmdplayer [" + category + " : " + UTIL_ToString( gameid ) + " : " + UTIL_ToString( pid ) + " : " + name + " : " + flag + " : " + UTIL_ToString( leaver ) + " : " + UTIL_ToString( practicing ) + "] - " + m_DB->GetError( ) );

	return RowID;
}

bool CGHostDBSQLite :: W3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints )
{
	if( var_ints.empty( ) )
		return false;

	bool Success = true;
	sqlite3_stmt *Statement = NULL;

	for( map<VarP,int32_t> :: iterator i = var_ints.begin( ); i != var_ints.end( ); i++ )
	{
		if( !Statement )
			m_DB->Prepare( "INSERT INTO w3mmdvars ( gameid, pid, varname, value_int ) VALUES ( ?, ?, ?, ? )", (void **)&Statement );

		if( Statement )
		{
			sqlite3_bind_int( Statement, 1, gameid );
			sqlite3_bind_int( Statement, 2, i->first.first );
			sqlite3_bind_text( Statement, 3, i->first.second.c_str( ), -1, SQLITE_TRANSIENT );
			sqlite3_bind_int( Statement, 4, i->second );

			int RC = m_DB->Step( Statement );

			if( RC == SQLITE_ERROR )
			{
				Success = false;
				CONSOLE_Print( "[SQLITE3] error adding w3mmdvar-int [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( i->first.first ) + " : " + i->first.second + " : " + UTIL_ToString( i->second ) + "] - " + m_DB->GetError( ) );
				break;
			}

			m_DB->Reset( Statement );
		}
		else
		{
			Success = false;
			CONSOLE_Print( "[SQLITE3] prepare error adding w3mmdvar-int [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( i->first.first ) + " : " + i->first.second + " : " + UTIL_ToString( i->second ) + "] - " + m_DB->GetError( ) );
			break;
		}
	}

	if( Statement )
		m_DB->Finalize( Statement );

	return Success;
}

bool CGHostDBSQLite :: W3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals )
{
	if( var_reals.empty( ) )
		return false;

	bool Success = true;
	sqlite3_stmt *Statement = NULL;

	for( map<VarP,double> :: iterator i = var_reals.begin( ); i != var_reals.end( ); i++ )
	{
		if( !Statement )
			m_DB->Prepare( "INSERT INTO w3mmdvars ( gameid, pid, varname, value_real ) VALUES ( ?, ?, ?, ? )", (void **)&Statement );

		if( Statement )
		{
			sqlite3_bind_int( Statement, 1, gameid );
			sqlite3_bind_int( Statement, 2, i->first.first );
			sqlite3_bind_text( Statement, 3, i->first.second.c_str( ), -1, SQLITE_TRANSIENT );
			sqlite3_bind_double( Statement, 4, i->second );

			int RC = m_DB->Step( Statement );

			if( RC == SQLITE_ERROR )
			{
				Success = false;
				CONSOLE_Print( "[SQLITE3] error adding w3mmdvar-real [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( i->first.first ) + " : " + i->first.second + " : " + UTIL_ToString( i->second, 10 ) + "] - " + m_DB->GetError( ) );
				break;
			}

			m_DB->Reset( Statement );
		}
		else
		{
			Success = false;
			CONSOLE_Print( "[SQLITE3] prepare error adding w3mmdvar-real [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( i->first.first ) + " : " + i->first.second + " : " + UTIL_ToString( i->second, 10 ) + "] - " + m_DB->GetError( ) );
			break;
		}
	}

	if( Statement )
		m_DB->Finalize( Statement );

	return Success;
}

bool CGHostDBSQLite :: W3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings )
{
	if( var_strings.empty( ) )
		return false;

	bool Success = true;
	sqlite3_stmt *Statement = NULL;

	for( map<VarP,string> :: iterator i = var_strings.begin( ); i != var_strings.end( ); i++ )
	{
		if( !Statement )
			m_DB->Prepare( "INSERT INTO w3mmdvars ( gameid, pid, varname, value_string ) VALUES ( ?, ?, ?, ? )", (void **)&Statement );

		if( Statement )
		{
			sqlite3_bind_int( Statement, 1, gameid );
			sqlite3_bind_int( Statement, 2, i->first.first );
			sqlite3_bind_text( Statement, 3, i->first.second.c_str( ), -1, SQLITE_TRANSIENT );
			sqlite3_bind_text( Statement, 4, i->second.c_str( ), -1, SQLITE_TRANSIENT );

			int RC = m_DB->Step( Statement );

			if( RC == SQLITE_ERROR )
			{
				Success = false;
				CONSOLE_Print( "[SQLITE3] error adding w3mmdvar-string [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( i->first.first ) + " : " + i->first.second + " : " + i->second + "] - " + m_DB->GetError( ) );
				break;
			}

			m_DB->Reset( Statement );
		}
		else
		{
			Success = false;
			CONSOLE_Print( "[SQLITE3] prepare error adding w3mmdvar-string [" + UTIL_ToString( gameid ) + " : " + UTIL_ToString( i->first.first ) + " : " + i->first.second + " : " + i->second + "] - " + m_DB->GetError( ) );
			break;
		}
	}

	if( Statement )
		m_DB->Finalize( Statement );

	return Success;
}

CCallableWarnCount *CGHostDBSQLite :: ThreadedWarnCount( string name, uint32_t warn )
{
	CCallableWarnCount *Callable = new CCallableWarnCount( name, warn );
	Callable->SetResult( BanCount( name, warn ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableAdminCount *CGHostDBSQLite :: ThreadedAdminCount( string server )
{
	CCallableAdminCount *Callable = new CCallableAdminCount( server );
	Callable->SetResult( AdminCount( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableAdminCheck *CGHostDBSQLite :: ThreadedAdminCheck( string server, string user )
{
	CCallableAdminCheck *Callable = new CCallableAdminCheck( server, user );
	Callable->SetResult( AdminCheck( server, user ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableAdminAdd *CGHostDBSQLite :: ThreadedAdminAdd( string server, string user )
{
	CCallableAdminAdd *Callable = new CCallableAdminAdd( server, user, m_AdminAccess );
	Callable->SetResult( AdminAdd( server, user ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableAdminRemove *CGHostDBSQLite :: ThreadedAdminRemove( string server, string user )
{
	CCallableAdminRemove *Callable = new CCallableAdminRemove( server, user );
	Callable->SetResult( AdminRemove( server, user ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableAdminList *CGHostDBSQLite :: ThreadedAdminList( string server )
{
	CCallableAdminList *Callable = new CCallableAdminList( server );
	Callable->SetResult( AdminList( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableAccessesList *CGHostDBSQLite :: ThreadedAccessesList( string server )
{
	CCallableAccessesList *Callable = new CCallableAccessesList( server );
	Callable->SetResult( AccessesList( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableSafeList *CGHostDBSQLite :: ThreadedSafeList( string server )
{
	CCallableSafeList *Callable = new CCallableSafeList( server );
	Callable->SetResult( SafeList( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableSafeList *CGHostDBSQLite :: ThreadedSafeListV( string server )
{
	CCallableSafeList *Callable = new CCallableSafeList( server );
	Callable->SetResult( SafeListV( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableSafeList *CGHostDBSQLite :: ThreadedNotes( string server )
{
	CCallableSafeList *Callable = new CCallableSafeList( server );
	Callable->SetResult( Notes( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableSafeList *CGHostDBSQLite :: ThreadedNotesN( string server )
{
	CCallableSafeList *Callable = new CCallableSafeList( server );
	Callable->SetResult( NotesN( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableRanks *CGHostDBSQLite :: ThreadedRanks( string server)
{
	CCallableRanks *Callable = new CCallableRanks( server);
	Callable->SetResult( Ranks( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableSafeCheck *CGHostDBSQLite :: ThreadedSafeCheck( string server, string user )
{
	CCallableSafeCheck *Callable = new CCallableSafeCheck( server, user );
	Callable->SetResult( SafeCheck( server, user ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableBanUpdate *CGHostDBSQLite :: ThreadedBanUpdate( string name, string ip )
{
	CCallableBanUpdate *Callable = new CCallableBanUpdate( name, ip );
	Callable->SetResult( BanUpdate( name, ip ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableRunQuery *CGHostDBSQLite :: ThreadedRunQuery( string query )
{
	CCallableRunQuery *Callable = new CCallableRunQuery( query);
	Callable->SetResult( RunQuery( query) );
	Callable->SetReady( true );
	return Callable;
}

CCallableScoreCheck *CGHostDBSQLite :: ThreadedScoreCheck( string category, string name, string server )
{
	CCallableScoreCheck *Callable = new CCallableScoreCheck( category, name, server);
	CDBScoreSummary *DotAScoreSummary = ScoreCheck(category, name, server);

	uint32_t Rank = 0;
	double Score = 0;
	if (DotAScoreSummary)
	{
		Rank = DotAScoreSummary->GetRank();
		Score = DotAScoreSummary->GetScore();
	}
	delete DotAScoreSummary;

	Callable->SetResult( Score );
	Callable->SetRank( Rank);
	Callable->SetReady( true );
	return Callable;
}

CCallableSafeAdd *CGHostDBSQLite :: ThreadedSafeAdd( string server, string user, string voucher )
{
	CCallableSafeAdd *Callable = new CCallableSafeAdd( server, user, voucher );
	Callable->SetResult( SafeAdd( server, user, voucher ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableSafeRemove *CGHostDBSQLite :: ThreadedSafeRemove( string server, string user )
{
	CCallableSafeRemove *Callable = new CCallableSafeRemove( server, user );
	Callable->SetResult( SafeRemove( server, user ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableNoteAdd *CGHostDBSQLite :: ThreadedNoteAdd( string server, string user, string note )
{
	CCallableNoteAdd *Callable = new CCallableNoteAdd( server, user, note );
	Callable->SetResult( NoteAdd( server, user, note ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableNoteAdd *CGHostDBSQLite :: ThreadedNoteUpdate( string server, string user, string note )
{
	CCallableNoteAdd *Callable = new CCallableNoteAdd( server, user, note );
	Callable->SetResult( NoteUpdate( server, user, note ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableNoteRemove *CGHostDBSQLite :: ThreadedNoteRemove( string server, string user )
{
	CCallableNoteRemove *Callable = new CCallableNoteRemove( server, user );
	Callable->SetResult( NoteRemove( server, user ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableCalculateScores *CGHostDBSQLite :: ThreadedCalculateScores( string formula, string mingames )
{
	CCallableCalculateScores *Callable = new CCallableCalculateScores( formula, mingames );
	Callable->SetResult( CalculateScores( formula, mingames ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableWarnUpdate *CGHostDBSQLite :: ThreadedWarnUpdate( string user, uint32_t before, uint32_t after )
{
	CCallableWarnUpdate *Callable = new CCallableWarnUpdate( user, before, after);
	Callable->SetResult( WarnUpdate( user, before, after) );
	Callable->SetReady( true );
	return Callable;
}

CCallableWarnForget *CGHostDBSQLite :: ThreadedWarnForget( string user, uint32_t gamethreshold )
{
	CCallableWarnForget *Callable = new CCallableWarnForget( user, gamethreshold);
	Callable->SetResult( WarnForget( user, gamethreshold) );
	Callable->SetReady( true );
	return Callable;
}

CCallableTodayGamesCount *CGHostDBSQLite :: ThreadedTodayGamesCount( )
{
	CCallableTodayGamesCount *Callable = new CCallableTodayGamesCount( );
	Callable->SetResult( TodayGamesCount( ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableBanCount *CGHostDBSQLite :: ThreadedBanCount( string server )
{
	CCallableBanCount *Callable = new CCallableBanCount( server );
	Callable->SetResult( BanCount( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableBanCheck *CGHostDBSQLite :: ThreadedBanCheck( string server, string user, string ip, uint32_t warn )
{
	CCallableBanCheck *Callable = new CCallableBanCheck( server, user, ip, warn );
	Callable->SetResult( BanCheck( server, user, ip, warn ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableBanAdd *CGHostDBSQLite :: ThreadedBanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn )
{
	CCallableBanAdd *Callable = new CCallableBanAdd( server, user, ip, gamename, admin, reason, expiredaytime, warn );
	Callable->SetResult( BanAdd( server, user, ip, gamename, admin, reason, expiredaytime, warn ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableBanRemove *CGHostDBSQLite :: ThreadedBanRemove( string server, string user, string admin, uint32_t warn )
{
	CCallableBanRemove *Callable = new CCallableBanRemove( server, user, admin, warn );
	Callable->SetResult( BanRemove( server, user, admin, warn ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableBanRemove *CGHostDBSQLite :: ThreadedBanRemove( string user, uint32_t warn )
{
	CCallableBanRemove *Callable = new CCallableBanRemove( string( ), user, string( ), warn );
	Callable->SetResult( BanRemove( user, warn ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableBanList *CGHostDBSQLite :: ThreadedBanList( string server )
{
	CCallableBanList *Callable = new CCallableBanList( server );
	Callable->SetResult( BanList( server ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableGameAdd *CGHostDBSQLite :: ThreadedGameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	CCallableGameAdd *Callable = new CCallableGameAdd( server, map, gamename, ownername, duration, gamestate, creatorname, creatorserver );
	Callable->SetResult( GameAdd( server, map, gamename, ownername, duration, gamestate, creatorname, creatorserver ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableGamePlayerAdd *CGHostDBSQLite :: ThreadedGamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	CCallableGamePlayerAdd *Callable = new CCallableGamePlayerAdd( gameid, name, ip, spoofed, spoofedrealm, reserved, loadingtime, left, leftreason, team, colour );
	Callable->SetResult( GamePlayerAdd( gameid, name, ip, spoofed, spoofedrealm, reserved, loadingtime, left, leftreason, team, colour ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableGamePlayerSummaryCheck *CGHostDBSQLite :: ThreadedGamePlayerSummaryCheck( string name )
{
	CCallableGamePlayerSummaryCheck *Callable = new CCallableGamePlayerSummaryCheck( name );
	Callable->SetResult( GamePlayerSummaryCheck( name ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableDotAGameAdd *CGHostDBSQLite :: ThreadedDotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	CCallableDotAGameAdd *Callable = new CCallableDotAGameAdd( gameid, winner, min, sec );
	Callable->SetResult( DotAGameAdd( gameid, winner, min, sec ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableDotAPlayerAdd *CGHostDBSQLite :: ThreadedDotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills )
{
	CCallableDotAPlayerAdd *Callable = new CCallableDotAPlayerAdd( gameid, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolour, towerkills, raxkills, courierkills );
	Callable->SetResult( DotAPlayerAdd( gameid, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolour, towerkills, raxkills, courierkills ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableDotAPlayerSummaryCheck *CGHostDBSQLite :: ThreadedDotAPlayerSummaryCheck( string name, string formula, string mingames, string gamestate )
{
	CCallableDotAPlayerSummaryCheck *Callable = new CCallableDotAPlayerSummaryCheck( name, formula, mingames, gamestate );
	Callable->SetResult( DotAPlayerSummaryCheck( name, formula, mingames, gamestate ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableDownloadAdd *CGHostDBSQLite :: ThreadedDownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	CCallableDownloadAdd *Callable = new CCallableDownloadAdd( map, mapsize, name, ip, spoofed, spoofedrealm, downloadtime );
	Callable->SetResult( DownloadAdd( map, mapsize, name, ip, spoofed, spoofedrealm, downloadtime ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableW3MMDPlayerAdd *CGHostDBSQLite :: ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	CCallableW3MMDPlayerAdd *Callable = new CCallableW3MMDPlayerAdd( category, gameid, pid, name, flag, leaver, practicing );
	Callable->SetResult( W3MMDPlayerAdd( category, gameid, pid, name, flag, leaver, practicing ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBSQLite :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints )
{
	CCallableW3MMDVarAdd *Callable = new CCallableW3MMDVarAdd( gameid, var_ints );
	Callable->SetResult( W3MMDVarAdd( gameid, var_ints ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBSQLite :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals )
{
	CCallableW3MMDVarAdd *Callable = new CCallableW3MMDVarAdd( gameid, var_reals );
	Callable->SetResult( W3MMDVarAdd( gameid, var_reals ) );
	Callable->SetReady( true );
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBSQLite :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings )
{
	CCallableW3MMDVarAdd *Callable = new CCallableW3MMDVarAdd( gameid, var_strings );
	Callable->SetResult( W3MMDVarAdd( gameid, var_strings ) );
	Callable->SetReady( true );
	return Callable;
}

uint32_t CGHostDBSQLite :: LastAccess( )
{
	return m_LastAccess;
}
