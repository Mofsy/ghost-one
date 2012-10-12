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
	string CFGFile = "update_dota_elo.cfg";

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

	string QCreate1 = "CREATE TABLE IF NOT EXISTS dota_elo_scores ( id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(15) NOT NULL, server VARCHAR(100) NOT NULL, score REAL NOT NULL, games INT NOT NULL, wins INT NOT NULL, losses INT NOT NULL, kills INT NOT NULL, deaths INT NOT NULL, creepkills INT NOT NULL, creepdenies INT NOT NULL, assists INT NOT NULL, neutralkills INT NOT NULL, towerkills INT NOT NULL, raxkills INT NOT NULL, courierkills INT NOT NULL )";
	string QCreate2 = "CREATE TABLE IF NOT EXISTS dota_elo_games_scored ( id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, gameid INT NOT NULL )";

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

	string QSelectUnscored = "SELECT id FROM games WHERE id > ( SELECT IFNULL(MAX(gameid), 0) FROM dota_elo_games_scored ) ORDER BY id LIMIT 1000";

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

		string QSelectPlayers = "SELECT dota_elo_scores.id, gameplayers.name, spoofedrealm, newcolour, winner, score, dotaplayers.kills, dotaplayers.deaths, dotaplayers.creepkills, dotaplayers.creepdenies, dotaplayers.assists, dotaplayers.neutralkills, dotaplayers.towerkills, dotaplayers.raxkills, dotaplayers.courierkills FROM dotaplayers LEFT JOIN dotagames ON dotagames.gameid=dotaplayers.gameid LEFT JOIN gameplayers ON gameplayers.gameid=dotaplayers.gameid AND gameplayers.colour=dotaplayers.colour LEFT JOIN dota_elo_scores ON dota_elo_scores.name=gameplayers.name AND server=spoofedrealm WHERE dotaplayers.gameid=" + UTIL_ToString( GameID );

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

				bool ignore = false;
				uint32_t rowids[10];
				string names[10];
				string servers[10];
				uint32_t kills[10];
				uint32_t deaths[10];
				uint32_t creepkills[10];
				uint32_t creepdenies[10];
				uint32_t assists[10];
				uint32_t neutralkills[10];
				uint32_t towerkills[10];
				uint32_t raxkills[10];
				uint32_t courierkills[10];
				bool exists[10];
				int num_players = 0;
				float player_ratings[10];
				int player_teams[10];
				int num_teams = 2;
				float team_ratings[2];
				float team_winners[2];
				int team_numplayers[2];
				team_ratings[0] = 0.0;
				team_ratings[1] = 0.0;
				team_numplayers[0] = 0;
				team_numplayers[1] = 0;

				vector<string> Row = MySQLFetchRow( Result );

				while( Row.size( ) == 15 )
				{
					if( num_players >= 10 )
					{
						cout << "gameid " << UTIL_ToString( GameID ) << " has more than 10 players, ignoring" << endl;
						ignore = true;
						break;
					}

					uint32_t Winner = UTIL_ToUInt32( Row[4] );

					if( Winner != 1 && Winner != 2 )
					{
						cout << "gameid " << UTIL_ToString( GameID ) << " has no winner, ignoring" << endl;
						ignore = true;
						break;
					}
					else if( Winner == 1 )
					{
						team_winners[0] = 1.0;
						team_winners[1] = 0.0;
					}
					else
					{
						team_winners[0] = 0.0;
						team_winners[1] = 1.0;
					}

					if( !Row[0].empty( ) )
						rowids[num_players] = UTIL_ToUInt32( Row[0] );
					else
						rowids[num_players] = 0;

					names[num_players] = Row[1];
					servers[num_players] = Row[2];
					
					kills[num_players] = UTIL_ToUInt32( Row[6] );
					deaths[num_players] = UTIL_ToUInt32( Row[7] );
					creepkills[num_players] = UTIL_ToUInt32( Row[8] );
					creepdenies[num_players] = UTIL_ToUInt32( Row[9] );
					assists[num_players] = UTIL_ToUInt32( Row[10] );
					neutralkills[num_players] = UTIL_ToUInt32( Row[11] );
					towerkills[num_players] = UTIL_ToUInt32( Row[12] );
					raxkills[num_players] = UTIL_ToUInt32( Row[13] );
					courierkills[num_players] = UTIL_ToUInt32( Row[14] );

					if( !Row[5].empty( ) )
					{
						exists[num_players] = true;
						player_ratings[num_players] = UTIL_ToFloat( Row[5] );
					}
					else
					{
						cout << "new player [" << Row[1] << "] found" << endl;
						exists[num_players] = false;
						player_ratings[num_players] = 1000.0;
					}

					uint32_t Colour = UTIL_ToUInt32( Row[3] );

					if( Colour >= 1 && Colour <= 5 )
					{
						player_teams[num_players] = 0;
						team_ratings[0] += player_ratings[num_players];
						team_numplayers[0]++;
					}
					else if( Colour >= 7 && Colour <= 11 )
					{
						player_teams[num_players] = 1;
						team_ratings[1] += player_ratings[num_players];
						team_numplayers[1]++;
					}
					else
					{
						cout << "gameid " << UTIL_ToString( GameID ) << " has a player with an invalid newcolour, ignoring" << endl;
						ignore = true;
						break;
					}

					num_players++;
					Row = MySQLFetchRow( Result );
				}

				mysql_free_result( Result );

				if( !ignore )
				{
					if( num_players == 0 )
						cout << "gameid " << UTIL_ToString( GameID ) << " has no players, ignoring" << endl;
					else if( team_numplayers[0] == 0 )
						cout << "gameid " << UTIL_ToString( GameID ) << " has no Sentinel players, ignoring" << endl;
					else if( team_numplayers[1] == 0 )
						cout << "gameid " << UTIL_ToString( GameID ) << " has no Scourge players, ignoring" << endl;
					else
					{
						cout << "gameid " << UTIL_ToString( GameID ) << " is calculating" << endl;

						float old_player_ratings[10];
						memcpy( old_player_ratings, player_ratings, sizeof( float ) * 10 );
						team_ratings[0] /= team_numplayers[0];
						team_ratings[1] /= team_numplayers[1];
						elo_recalculate_ratings( num_players, player_ratings, player_teams, num_teams, team_ratings, team_winners );

						for( int i = 0; i < num_players; i++ )
						{
							cout << "player [" << names[i] << "] rating " << UTIL_ToString( (uint32_t)old_player_ratings[i] ) << " -> " << UTIL_ToString( (uint32_t)player_ratings[i] ) << endl;

							uint32_t WinIncrement = (uint32_t) team_winners[player_teams[i]];
							uint32_t LossIncrement = 1 - WinIncrement;

							if( exists[i] )
							{
								string QUpdateScore = "UPDATE dota_elo_scores SET score=" + UTIL_ToString( player_ratings[i], 2 ) + ", games=games+1, wins=wins+" + UTIL_ToString( WinIncrement ) + ", losses=losses+" + UTIL_ToString( LossIncrement ) + ", kills=kills+" + UTIL_ToString( kills[i] ) + ", deaths=deaths+" + UTIL_ToString( deaths[i] ) + ", creepkills=creepkills+" + UTIL_ToString( creepkills[i] ) + ", creepdenies=creepdenies+" + UTIL_ToString( creepdenies[i] ) + ", assists=assists+" + UTIL_ToString( assists[i] ) + ", neutralkills=neutralkills+" + UTIL_ToString( neutralkills[i] ) + ", towerkills=towerkills+" + UTIL_ToString( towerkills[i] ) + ", raxkills=raxkills+" + UTIL_ToString( raxkills[i] ) + ", courierkills=courierkills+" + UTIL_ToString( courierkills[i] ) + " WHERE id=" + UTIL_ToString( rowids[i] );

								if( mysql_real_query( Connection, QUpdateScore.c_str( ), QUpdateScore.size( ) ) != 0 )
								{
									cout << "error: " << mysql_error( Connection ) << endl;
									return 1;
								}
							}
							else
							{
								string EscName = MySQLEscapeString( Connection, names[i] );
								string EscServer = MySQLEscapeString( Connection, servers[i] );
								
								string QInsertScore = "INSERT INTO dota_elo_scores ( name, server, score, games, wins, losses, kills, deaths, creepkills, creepdenies, assists, neutralkills, towerkills, raxkills, courierkills ) VALUES ( '" + EscName + "', '" + EscServer + "', " + UTIL_ToString( player_ratings[i], 2 ) + ", 1, " + UTIL_ToString( WinIncrement ) + ", " + UTIL_ToString( LossIncrement ) + ", " + UTIL_ToString( kills[i] ) + ", " + UTIL_ToString( deaths[i] ) + ", " + UTIL_ToString( creepkills[i] ) + ", " + UTIL_ToString( creepdenies[i] ) + ", " + UTIL_ToString( assists[i] ) + ", " + UTIL_ToString( neutralkills[i] ) + ", " + UTIL_ToString( towerkills[i] ) + ", " + UTIL_ToString( raxkills[i] ) + ", " + UTIL_ToString( courierkills[i] ) + " )";

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

		string QInsertScored = "INSERT INTO dota_elo_games_scored ( gameid ) VALUES ( " + UTIL_ToString( GameID ) + " )";

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
