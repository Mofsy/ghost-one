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
	This modification is released under the GNU GPL v3.
   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

#ifdef WIN32
 #include "ms_stdint.h"
#else
 #include <stdint.h>
#endif

#include "config.h"
#include "elo.h"

#include <string.h>

#ifdef WIN32
 #include <winsock.h>
#endif

#include <mysql/mysql.h>

void CONSOLE_Print( string message )
{
	cout << message << endl;
}

string MySQLEscapeString( MYSQL *conn, string str )
{
	char *to = new char[str.size( ) * 2 + 1];
	unsigned long size = mysql_real_escape_string( conn, to, str.c_str( ), str.size( ) );
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

string UTIL_ToString( uint32_t i )
{
	string result;
	stringstream SS;
	SS << i;
	SS >> result;
	return result;
}

string UTIL_ToString( float f, int digits )
{
	string result;
	stringstream SS;
	SS << std :: fixed << std :: setprecision( digits ) << f;
	SS >> result;
	return result;
}

uint32_t UTIL_ToUInt32( string &s )
{
	uint32_t result;
	stringstream SS;
	SS << s;
	SS >> result;
	return result;
}

float UTIL_ToFloat( string &s )
{
	float result;
	stringstream SS;
	SS << s;
	SS >> result;
	return result;
}

int main( int argc, char **argv )
{
	string CFGFile = "update_w3mmd_elo.cfg";

	if( argc > 1 && argv[1] )
		CFGFile = argv[1];

	CConfig CFG;
	CFG.Read( CFGFile );
	string Server = CFG.GetString( "db_mysql_server", string( ) );
	string Database = CFG.GetString( "db_mysql_database", "ghost" );
	string User = CFG.GetString( "db_mysql_user", string( ) );
	string Password = CFG.GetString( "db_mysql_password", string( ) );
	int Port = CFG.GetInt( "db_mysql_port", 0 );

	cout << "connecting to database server" << endl;
	MYSQL *Connection = NULL;

	if( !( Connection = mysql_init( NULL ) ) )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	my_bool Reconnect = true;
	mysql_options( Connection, MYSQL_OPT_RECONNECT, &Reconnect );

	if( !( mysql_real_connect( Connection, Server.c_str( ), User.c_str( ), Password.c_str( ), Database.c_str( ), Port, NULL, 0 ) ) )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	cout << "connected" << endl;
	cout << "beginning transaction" << endl;

	string QBegin = "BEGIN";

	if( mysql_real_query( Connection, QBegin.c_str( ), QBegin.size( ) ) != 0 )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	cout << "creating tables" << endl;

	string QCreate1 = "CREATE TABLE IF NOT EXISTS w3mmd_elo_scores ( id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(15) NOT NULL, category VARCHAR(25), server VARCHAR(100) NOT NULL, score REAL NOT NULL, games INT NOT NULL, wins INT NOT NULL, losses INT NOT NULL, intstats0 INT NOT NULL, intstats1 INT NOT NULL, intstats2 INT NOT NULL, intstats3 INT NOT NULL, intstats4 INT NOT NULL, intstats5 INT NOT NULL, intstats6 INT NOT NULL, intstats7 INT NOT NULL, doublestats0 DOUBLE NOT NULL, doublestats1 DOUBLE NOT NULL, doublestats2 DOUBLE NOT NULL, doublestats3 DOUBLE NOT NULL )";
	string QCreate2 = "CREATE TABLE IF NOT EXISTS w3mmd_elo_games_scored ( id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, gameid INT NOT NULL )";

	if( mysql_real_query( Connection, QCreate1.c_str( ), QCreate1.size( ) ) != 0 )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	if( mysql_real_query( Connection, QCreate2.c_str( ), QCreate2.size( ) ) != 0 )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	cout << "getting unscored games" << endl;
	queue<uint32_t> UnscoredGames;

	string QSelectUnscored = "SELECT id FROM games WHERE id > ( SELECT IFNULL(MAX(gameid), 0) FROM w3mmd_elo_games_scored ) ORDER BY id LIMIT 1000";

	if( mysql_real_query( Connection, QSelectUnscored.c_str( ), QSelectUnscored.size( ) ) != 0 )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}
	else
	{
		MYSQL_RES *Result = mysql_store_result( Connection );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				UnscoredGames.push( UTIL_ToUInt32( Row[0] ) );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
		{
			cout << "error: " << mysql_error( Connection ) << endl;
			return 1;
		}
	}

	cout << "found " << UnscoredGames.size( ) << " unscored games" << endl;

	while( !UnscoredGames.empty( ) )
	{
		uint32_t GameID = UnscoredGames.front( );
		UnscoredGames.pop( );

		// lowercase the name because there was a bug in GHost++ 13.3 and earlier that didn't automatically lowercase it when using MySQL

		string QSelectPlayers = "SELECT w3mmd_elo_scores.id, LOWER(gameplayers.name), spoofedrealm, flag, practicing, score, w3mmdplayers.pid, w3mmdplayers.category FROM w3mmdplayers LEFT JOIN gameplayers ON gameplayers.gameid=w3mmdplayers.gameid AND gameplayers.name=w3mmdplayers.name LEFT JOIN w3mmd_elo_scores ON w3mmd_elo_scores.name=w3mmdplayers.name AND server=spoofedrealm AND w3mmd_elo_scores.category=w3mmdplayers.category WHERE w3mmdplayers.gameid=" + UTIL_ToString( GameID );

		if( mysql_real_query( Connection, QSelectPlayers.c_str( ), QSelectPlayers.size( ) ) != 0 )
		{
			cout << "error: " << mysql_error( Connection ) << endl;
			return 1;
		}
		else
		{
			MYSQL_RES *Result = mysql_store_result( Connection );

			if( Result )
			{
				cout << "gameid " << UTIL_ToString( GameID ) << " found" << endl;

				string category = "";

				bool ignore = false;
				bool winner = false;
				uint32_t rowids[12];
				string names[12];
				string servers[12];
				bool exists[12];
				int num_players = 0;
				float player_ratings[12];
				int player_teams[12];
				int num_teams = 0;
				float team_ratings[2];
				float team_winners[2];
				int team_numplayers[2];
				
				team_ratings[0] = 0.0;
				team_ratings[1] = 0.0;
				team_numplayers[0] = 0;
				team_numplayers[1] = 0;
				team_winners[0] = 1.0;
				team_winners[1] = 0.0;
				
				//statistics
				uint32_t intstats[12][8];
				double doublestats[12][4];

				vector<string> Row = MySQLFetchRow( Result );

				while( Row.size( ) == 8 )
				{
					// Row[0] = rowid
					// Row[1] = name
					// Row[2] = server
					// Row[3] = flag
					// Row[4] = practicing
					// Row[5] = score
					// Row[6] = pid
					// Row[7] = category

					if( num_players >= 12 )
					{
						cout << "gameid " << UTIL_ToString( GameID ) << " has more than 12 players, ignoring" << endl;
						ignore = true;
						break;
					}

					if( Row[3] == "drawer" )
					{
						cout << "ignoring player [" << Row[1] << "|" << Row[2] << "] because they drew" << endl;
						Row = MySQLFetchRow( Result );
						continue;
					}

					if( Row[4] == "1" )
					{
						cout << "ignoring player [" << Row[1] << "|" << Row[2] << "] because they were practicing" << endl;
						Row = MySQLFetchRow( Result );
						continue;
					}

					category = Row[7];
					uint32_t pid = UTIL_ToUInt32( Row[6] );
					
					for( int x = 0; x < 8; x++ ) intstats[num_players][x] = 0;
					for( int x = 0; x < 4; x++ ) doublestats[num_players][x] = 0;

					if( !Row[0].empty( ) )
						rowids[num_players] = UTIL_ToUInt32( Row[0] );
					else
						rowids[num_players] = 0;

					names[num_players] = Row[1];
					servers[num_players] = Row[2];

					if( !Row[5].empty( ) )
					{
						exists[num_players] = true;
						player_ratings[num_players] = UTIL_ToFloat( Row[5] );
					}
					else
					{
						cout << "new player [" << Row[1] << "|" << Row[2] << "] found, team=" << player_teams[num_players] << endl;
						exists[num_players] = false;
						player_ratings[num_players] = 1000.0;
					}

					if( Row[3] == "winner" )
					{
						team_numplayers[0]++;
						team_ratings[0] += player_ratings[num_players];
						player_teams[num_players] = 0;
					}
					else
					{
						team_numplayers[1]++;
						team_ratings[1] += player_ratings[num_players];
						player_teams[num_players] = 1;
					}
					
					if( Row[7] == "treetag" || Row[7] == "battleships" || Row[7] == "elitesnipers" )
					{
						string QSelectStats = "SELECT varname, value_int FROM w3mmdvars WHERE gameid=" + UTIL_ToString( GameID ) + " AND pid=" + UTIL_ToString( pid );
						
						if( mysql_real_query( Connection, QSelectStats.c_str( ), QSelectStats.size( ) ) != 0 )
						{
							cout << "error: " << mysql_error( Connection ) << endl;
							return 1;
						}
						else
						{
							MYSQL_RES *StatsResult = mysql_store_result( Connection );

							if( Result )
							{
								vector<string> StatsRow = MySQLFetchRow( StatsResult );
								
								while( StatsRow.size( ) == 2 )
								{
									if( Row[7] == "treetag" )
									{
										if( StatsRow[0] == "kills" && pid < 9 ) intstats[num_players][0] = UTIL_ToUInt32( StatsRow[1] );
										else if( StatsRow[0] == "deaths" && pid < 9  ) intstats[num_players][1] = UTIL_ToUInt32( StatsRow[1] );
										else if( StatsRow[0] == "saves" && pid < 9 ) intstats[num_players][2] = UTIL_ToUInt32( StatsRow[1] );
										else if( StatsRow[0] == "kills" && pid >= 9 ) intstats[num_players][3] = UTIL_ToUInt32( StatsRow[1] );
										
										if( pid < 9 ) intstats[num_players][4] = 1;
										else intstats[num_players][5] = 1;
									}
									
									else if( Row[7] == "battleships" )
									{
										if( StatsRow[0] == "kills" ) intstats[num_players][0] = UTIL_ToUInt32( StatsRow[1] );
										else if( StatsRow[0] == "deaths"  ) intstats[num_players][1] = UTIL_ToUInt32( StatsRow[1] );
									}
									
									else if( Row[7] == "elitesnipers" )
									{
										if( StatsRow[0] == "kills" ) intstats[num_players][0] = UTIL_ToUInt32( StatsRow[1] );
										else if( StatsRow[0] == "deaths"  ) intstats[num_players][1] = UTIL_ToUInt32( StatsRow[1] );
									}
									
									StatsRow = MySQLFetchRow( StatsResult );
								}
							}
							
							mysql_free_result( StatsResult );
						}
					}

					num_players++;
					Row = MySQLFetchRow( Result );
				}

				num_teams = 2;
				mysql_free_result( Result );

				if( !ignore )
				{
					if( num_players == 0 || team_numplayers[0] == 0 || team_numplayers[1] == 0 )
						cout << "gameid " << UTIL_ToString( GameID ) << " has no players or has invalid teams or is the wrong category, ignoring" << endl;
					else
					{
						cout << "gameid " << UTIL_ToString( GameID ) << " is calculating" << endl;

						float old_player_ratings[12];
						memcpy( old_player_ratings, player_ratings, sizeof( float ) * 12 );
						team_ratings[0] /= team_numplayers[0];
						team_ratings[1] /= team_numplayers[1];
						elo_recalculate_ratings( num_players, player_ratings, player_teams, num_teams, team_ratings, team_winners );

						for( int i = 0; i < num_players; i++ )
						{
							if( names[i].empty( ) ) continue;
							
							cout << "player [" << names[i] << "|" << servers[i] << "] rating " << UTIL_ToString( (uint32_t)old_player_ratings[i] ) << " -> " << UTIL_ToString( (uint32_t)player_ratings[i] ) << endl;

							uint32_t WinIncrement = (uint32_t) team_winners[player_teams[i]];
							uint32_t LossIncrement = 1 - WinIncrement;

							if( exists[i] )
							{
								string QUpdateScore = "UPDATE w3mmd_elo_scores SET score=" + UTIL_ToString( player_ratings[i], 2 ) + ", games=games+1, wins=wins+" + UTIL_ToString( WinIncrement ) + ", losses=losses+" + UTIL_ToString( LossIncrement ) + ", intstats0=intstats0+" + UTIL_ToString( intstats[i][0] ) + ", intstats1=intstats1+" + UTIL_ToString( intstats[i][1] ) + ", intstats2=intstats2+" + UTIL_ToString( intstats[i][2] ) + ", intstats3=intstats3+" + UTIL_ToString( intstats[i][3] ) + ", intstats4=intstats4+" + UTIL_ToString( intstats[i][4] ) + ", intstats5=intstats5+" + UTIL_ToString( intstats[i][5] ) + ", intstats6=intstats6+" + UTIL_ToString( intstats[i][6] ) + ", intstats7=intstats7+" + UTIL_ToString( intstats[i][7] ) + ", doublestats0=doublestats0+" + UTIL_ToString( doublestats[i][0] ) + ", doublestats1=doublestats1+" + UTIL_ToString( doublestats[i][1] ) + ", doublestats2=doublestats2+" + UTIL_ToString( doublestats[i][2] ) + ", doublestats3=doublestats3+" + UTIL_ToString( doublestats[i][3] ) + " WHERE id=" + UTIL_ToString( rowids[i] );

								if( mysql_real_query( Connection, QUpdateScore.c_str( ), QUpdateScore.size( ) ) != 0 )
								{
									cout << "error: " << mysql_error( Connection ) << endl;
									return 1;
								}
							}
							else
							{
								string EscCategory = MySQLEscapeString( Connection, category );
								string EscName = MySQLEscapeString( Connection, names[i] );
								string EscServer = MySQLEscapeString( Connection, servers[i] );
								string QInsertScore = "INSERT INTO w3mmd_elo_scores ( category, name, server, score, games, wins, losses, intstats0, intstats1, intstats2, intstats3, intstats4, intstats5, intstats6, intstats7, doublestats0, doublestats1, doublestats2, doublestats3 ) VALUES ( '" + EscCategory + "', '" + EscName + "', '" + EscServer + "', " + UTIL_ToString( player_ratings[i], 2 ) + ", 1, " + UTIL_ToString( WinIncrement ) + ", " + UTIL_ToString( LossIncrement ) + ", " + UTIL_ToString( intstats[i][0] ) + ", " + UTIL_ToString( intstats[i][1] ) + ", " + UTIL_ToString( intstats[i][2] ) + ", " + UTIL_ToString( intstats[i][3] ) + ", " + UTIL_ToString( intstats[i][4] ) + ", " + UTIL_ToString( intstats[i][5] ) + ", " + UTIL_ToString( intstats[i][6] ) + ", " + UTIL_ToString( intstats[i][7] ) + ", " + UTIL_ToString( doublestats[i][0] ) + ", " + UTIL_ToString( doublestats[i][1] ) + ", " + UTIL_ToString( doublestats[i][2] ) + ", " + UTIL_ToString( doublestats[i][3] ) + " )";

								if( mysql_real_query( Connection, QInsertScore.c_str( ), QInsertScore.size( ) ) != 0 )
								{
									cout << "error: " << mysql_error( Connection ) << endl;
									return 1;
								}
							}
						}
					}
				}				
			}
			else
			{
				cout << "error: " << mysql_error( Connection ) << endl;
				return 1;
			}
		}

		string QInsertScored = "INSERT INTO w3mmd_elo_games_scored ( gameid ) VALUES ( " + UTIL_ToString( GameID ) + " )";

		if( mysql_real_query( Connection, QInsertScored.c_str( ), QInsertScored.size( ) ) != 0 )
		{
			cout << "error: " << mysql_error( Connection ) << endl;
			return 1;
		}
	}

	cout << "committing transaction" << endl;

	string QCommit = "COMMIT";

	if( mysql_real_query( Connection, QCommit.c_str( ), QCommit.size( ) ) != 0 )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	cout << "done" << endl;
	return 0;
}
