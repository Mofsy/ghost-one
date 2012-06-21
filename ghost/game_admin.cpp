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
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "replay.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "game_admin.h"

#include <string.h>

#include <boost/filesystem.hpp>

using namespace boost :: filesystem;

//
// CAdminGame
//

CAdminGame :: CAdminGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nPassword ) : CBaseGame( nGHost, nMap, nSaveGame, nHostPort, nGameState, nGameName, string( ), string( ), string( ) )
{
	m_VirtualHostName = "|cFFC04040Admin";
	m_MuteLobby = true;
	m_Password = nPassword;
}

CAdminGame :: ~CAdminGame( )
{
	for( vector<PairedAdminCount> :: iterator i = m_PairedAdminCounts.begin( ); i != m_PairedAdminCounts.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedAdminAdd> :: iterator i = m_PairedAdminAdds.begin( ); i != m_PairedAdminAdds.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedAdminRemove> :: iterator i = m_PairedAdminRemoves.begin( ); i != m_PairedAdminRemoves.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedBanCount> :: iterator i = m_PairedBanCounts.begin( ); i != m_PairedBanCounts.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	/*

	for( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	*/

	for( vector<PairedBanRemove> :: iterator i = m_PairedBanRemoves.begin( ); i != m_PairedBanRemoves.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );
}

bool CAdminGame :: Update( void *fd, void *send_fd )
{
	//
	// update callables
	//

	for( vector<PairedAdminCount> :: iterator i = m_PairedAdminCounts.begin( ); i != m_PairedAdminCounts.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CGamePlayer *Player = GetPlayerFromName( i->first, true );

			if( Player )
			{
				uint32_t Count = i->second->GetResult( );

				if( Count == 0 )
					SendChat( Player, m_GHost->m_Language->ThereAreNoAdmins( i->second->GetServer( ) ) );
				else if( Count == 1 )
					SendChat( Player, m_GHost->m_Language->ThereIsAdmin( i->second->GetServer( ) ) );
				else
					SendChat( Player, m_GHost->m_Language->ThereAreAdmins( i->second->GetServer( ), UTIL_ToString( Count ) ) );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedAdminCounts.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedAdminAdd> :: iterator i = m_PairedAdminAdds.begin( ); i != m_PairedAdminAdds.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->AddAdmin( i->second->GetUser( ) );
				}
			}

			CGamePlayer *Player = GetPlayerFromName( i->first, true );

			if( Player )
			{
				if( i->second->GetResult( ) )
					SendChat( Player, m_GHost->m_Language->AddedUserToAdminDatabase( i->second->GetServer( ), i->second->GetUser( ) ) );
				else
					SendChat( Player, m_GHost->m_Language->ErrorAddingUserToAdminDatabase( i->second->GetServer( ), i->second->GetUser( ) ) );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedAdminAdds.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedAdminRemove> :: iterator i = m_PairedAdminRemoves.begin( ); i != m_PairedAdminRemoves.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->RemoveAdmin( i->second->GetUser( ) );
				}
			}

			CGamePlayer *Player = GetPlayerFromName( i->first, true );

			if( Player )
			{
				if( i->second->GetResult( ) )
					SendChat( Player, m_GHost->m_Language->DeletedUserFromAdminDatabase( i->second->GetServer( ), i->second->GetUser( ) ) );
				else
					SendChat( Player, m_GHost->m_Language->ErrorDeletingUserFromAdminDatabase( i->second->GetServer( ), i->second->GetUser( ) ) );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedAdminRemoves.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedBanCount> :: iterator i = m_PairedBanCounts.begin( ); i != m_PairedBanCounts.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CGamePlayer *Player = GetPlayerFromName( i->first, true );

			if( Player )
			{
				uint32_t Count = i->second->GetResult( );

				if( Count == 0 )
					SendChat( Player, m_GHost->m_Language->ThereAreNoBannedUsers( i->second->GetServer( ) ) );
				else if( Count == 1 )
					SendChat( Player, m_GHost->m_Language->ThereIsBannedUser( i->second->GetServer( ) ) );
				else
					SendChat( Player, m_GHost->m_Language->ThereAreBannedUsers( i->second->GetServer( ), UTIL_ToString( Count ) ) );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanCounts.erase( i );
		}
		else
			i++;
	}

	/*

	for( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->AddBan( i->second->GetUser( ), i->second->GetIP( ), i->second->GetGameName( ), i->second->GetAdmin( ), i->second->GetReason( ) );
				}
			}

			CGamePlayer *Player = GetPlayerFromName( i->first, true );

			if( Player )
			{
				if( i->second->GetResult( ) )
					SendChat( Player, m_GHost->m_Language->BannedUser( i->second->GetServer( ), i->second->GetUser( ) ) );
				else
					SendChat( Player, m_GHost->m_Language->ErrorBanningUser( i->second->GetServer( ), i->second->GetUser( ) ) );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanAdds.erase( i );
		}
		else
			i++;
	}

	*/

	for( vector<PairedBanRemove> :: iterator i = m_PairedBanRemoves.begin( ); i != m_PairedBanRemoves.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->RemoveBan( i->second->GetUser( ) );
				}
			}

			CGamePlayer *Player = GetPlayerFromName( i->first, true );

			if( Player )
			{
				if( i->second->GetResult( ) )
					SendChat( Player, m_GHost->m_Language->UnbannedUser( i->second->GetUser( ) ) );
				else
					SendChat( Player, m_GHost->m_Language->ErrorUnbanningUser( i->second->GetUser( ) ) );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanRemoves.erase( i );
		}
		else
			i++;
	}

	// reset the last reserved seen timer since the admin game should never be considered abandoned

	m_LastReservedSeen = GetTime( );
	return CBaseGame :: Update( fd, send_fd );
}

void CAdminGame :: SendAdminChat( string message )
{
	if (!m_GHost->m_AdminMessages)
		return;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetLoggedIn( ) )
			SendChat( *i, message );
	}
}

void CAdminGame :: SendWelcomeMessage( CGamePlayer *player )
{
	SendChat( player, "GHost++ Admin Game                     http://www.codelain.com/" );
	SendChat( player, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-" );
	SendChat( player, "Commands: addadmin, autohost, autohostmm, checkadmin" );
	SendChat( player, "Commands: checkban, countadmins, countbans, deladmin" );
	SendChat( player, "Commands: delban, disable, downloads, enable, end, enforcesg" );
	SendChat( player, "Commands: exit, getgame, getgames, hostsg, load, loadsg" );
	SendChat( player, "Commands: map, password, priv, privby, pub, pubby, quit" );
	SendChat( player, "Commands: reload, say, saygame, saygames, unban, unhost, w" );
}

void CAdminGame :: EventPlayerJoined( CPotentialPlayer *potential, CIncomingJoinPlayer *joinPlayer )
{
	uint32_t Time = GetTime( );

	for( vector<TempBan> :: iterator i = m_TempBans.begin( ); i != m_TempBans.end( ); )
	{
		// remove old tempbans (after 5 seconds)

		if( Time - (*i).second >= 5 )
			i = m_TempBans.erase( i );
		else
		{
			if( (*i).first == potential->GetExternalIPString( ) )
			{
				// tempbanned, goodbye

				potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_WRONGPASSWORD ) );
				potential->SetDeleteMe( true );
				CONSOLE_Print( "[ADMINGAME] player [" + joinPlayer->GetName( ) + "] at ip [" + (*i).first + "] is trying to join the game but is tempbanned" );
				return;
			}

			i++;
		}
	}

	CBaseGame :: EventPlayerJoined( potential, joinPlayer );
}

bool CAdminGame :: EventPlayerBotCommand( CGamePlayer *player, string command, string payload )
{
	CBaseGame :: EventPlayerBotCommand( player, command, payload );

	// todotodo: don't be lazy

	string User = player->GetName( );
	string Command = command;
	string Payload = payload;

	if( player->GetLoggedIn( ) )
	{
		CONSOLE_Print( "[ADMINGAME] admin [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );

		/*****************
		* ADMIN COMMANDS *
		******************/

		//
		// !ADDADMIN
		//

		if( Command == "addadmin" && !Payload.empty( ) )
		{
			// extract the name and the server
			// e.g. "Varlock useast.battle.net" -> name: "Gen", server: "useast.battle.net"

			string Name;
			string Server;
			stringstream SS;
			SS << Payload;
			SS >> Name;

			if( SS.eof( ) )
			{
				if( m_GHost->m_BNETs.size( ) == 1 )
					Server = m_GHost->m_BNETs[0]->GetServer( );
				else
					CONSOLE_Print( "[ADMINGAME] missing input #2 to addadmin command" );
			}
			else
				SS >> Server;

			if( !Server.empty( ) )
			{
				string Servers;
				bool FoundServer = false;

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( Servers.empty( ) )
						Servers = (*i)->GetServer( );
					else
						Servers += ", " + (*i)->GetServer( );

					if( (*i)->GetServer( ) == Server )
					{
						FoundServer = true;

						if( (*i)->IsAdmin( Name ) )
							SendChat( player, m_GHost->m_Language->UserIsAlreadyAnAdmin( Server, Name ) );
						else
							m_PairedAdminAdds.push_back( PairedAdminAdd( player->GetName( ), m_GHost->m_DB->ThreadedAdminAdd( Server, Name ) ) );

						break;
					}
				}

				if( !FoundServer )
					SendChat( player, m_GHost->m_Language->ValidServers( Servers ) );
			}
		}

		//
		// !AUTOHOST
		//

		if( Command == "autohost" )
		{
			if( Payload.empty( ) || Payload == "off" )
			{
				SendChat( player, m_GHost->m_Language->AutoHostDisabled( ) );
				m_GHost->m_AutoHostGameName.clear( );
				m_GHost->m_AutoHostOwner.clear( );
				m_GHost->m_AutoHostServer.clear( );
				m_GHost->m_AutoHostMaximumGames = 0;
				m_GHost->m_AutoHostAutoStartPlayers = 0;
				m_GHost->m_LastAutoHostTime = GetTime( );
				m_GHost->m_AutoHostMatchMaking = false;
				m_GHost->m_AutoHostMinimumScore = 0.0;
				m_GHost->m_AutoHostMaximumScore = 0.0;
			}
			else
			{
				// extract the maximum games, auto start players, and the game name
				// e.g. "5 10 BattleShips Pro" -> maximum games: "5", auto start players: "10", game name: "BattleShips Pro"

				uint32_t MaximumGames;
				uint32_t AutoStartPlayers;
				string GameName;
				stringstream SS;
				SS << Payload;
				SS >> MaximumGames;

				if( SS.fail( ) || MaximumGames == 0 )
					CONSOLE_Print( "[ADMINGAME] bad input #1 to autohost command" );
				else
				{
					SS >> AutoStartPlayers;

					if( SS.fail( ) || AutoStartPlayers == 0 )
						CONSOLE_Print( "[ADMINGAME] bad input #2 to autohost command" );
					else
					{
						if( SS.eof( ) )
							CONSOLE_Print( "[ADMINGAME] missing input #3 to autohost command" );
						else
						{
							getline( SS, GameName );
							string :: size_type Start = GameName.find_first_not_of( " " );

							if( Start != string :: npos )
								GameName = GameName.substr( Start );

							SendChat( player, m_GHost->m_Language->AutoHostEnabled( ) );
							m_GHost->ClearAutoHostMap( );
							m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
							m_GHost->m_AutoHostGameName = GameName;
							m_GHost->m_AutoHostOwner = User;
							m_GHost->m_AutoHostServer.clear( );
							m_GHost->m_AutoHostMaximumGames = MaximumGames;
							m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
							m_GHost->m_LastAutoHostTime = GetTime( );
							m_GHost->m_AutoHostMatchMaking = false;
							m_GHost->m_AutoHostMinimumScore = 0.0;
							m_GHost->m_AutoHostMaximumScore = 0.0;
						}
					}
				}
			}
		}

		//
		// !AUTOHOSTMM
		//

		if( Command == "autohostmm" )
		{
			if( Payload.empty( ) || Payload == "off" )
			{
				SendChat( player, m_GHost->m_Language->AutoHostDisabled( ) );
				m_GHost->m_AutoHostGameName.clear( );
				m_GHost->m_AutoHostOwner.clear( );
				m_GHost->m_AutoHostServer.clear( );
				m_GHost->m_AutoHostMaximumGames = 0;
				m_GHost->m_AutoHostAutoStartPlayers = 0;
				m_GHost->m_LastAutoHostTime = GetTime( );
				m_GHost->m_AutoHostMatchMaking = false;
				m_GHost->m_AutoHostMinimumScore = 0.0;
				m_GHost->m_AutoHostMaximumScore = 0.0;
			}
			else
			{
				// extract the maximum games, auto start players, and the game name
				// e.g. "5 10 800 1200 BattleShips Pro" -> maximum games: "5", auto start players: "10", minimum score: "800", maximum score: "1200", game name: "BattleShips Pro"

				uint32_t MaximumGames;
				uint32_t AutoStartPlayers;
				double MinimumScore;
				double MaximumScore;
				string GameName;
				stringstream SS;
				SS << Payload;
				SS >> MaximumGames;

				if( SS.fail( ) || MaximumGames == 0 )
					CONSOLE_Print( "[ADMINGAME] bad input #1 to autohostmm command" );
				else
				{
					SS >> AutoStartPlayers;

					if( SS.fail( ) || AutoStartPlayers == 0 )
						CONSOLE_Print( "[ADMINGAME] bad input #2 to autohostmm command" );
					else
					{
						SS >> MinimumScore;

						if( SS.fail( ) )
							CONSOLE_Print( "[ADMINGAME] bad input #3 to autohostmm command" );
						else
						{
							SS >> MaximumScore;

							if( SS.fail( ) )
								CONSOLE_Print( "[ADMINGAME] bad input #4 to autohostmm command" );
							else
							{
								if( SS.eof( ) )
									CONSOLE_Print( "[ADMINGAME] missing input #5 to autohostmm command" );
								else
								{
									getline( SS, GameName );
									string :: size_type Start = GameName.find_first_not_of( " " );

									if( Start != string :: npos )
										GameName = GameName.substr( Start );

									SendChat( player, m_GHost->m_Language->AutoHostEnabled( ) );
									m_GHost->ClearAutoHostMap( );
									m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
									m_GHost->m_AutoHostGameName = GameName;
									m_GHost->m_AutoHostOwner = User;
									m_GHost->m_AutoHostServer.clear( );
									m_GHost->m_AutoHostMaximumGames = MaximumGames;
									m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
									m_GHost->m_LastAutoHostTime = GetTime( );
									m_GHost->m_AutoHostMatchMaking = true;
									m_GHost->m_AutoHostMinimumScore = MinimumScore;
									m_GHost->m_AutoHostMaximumScore = MaximumScore;
								}
							}
						}
					}
				}
			}
		}

		//
		// !CHECKADMIN
		//

		if( Command == "checkadmin" && !Payload.empty( ) )
		{
			// extract the name and the server
			// e.g. "Varlock useast.battle.net" -> name: "Gen", server: "useast.battle.net"

			string Name;
			string Server;
			stringstream SS;
			SS << Payload;
			SS >> Name;

			if( SS.eof( ) )
			{
				if( m_GHost->m_BNETs.size( ) == 1 )
					Server = m_GHost->m_BNETs[0]->GetServer( );
				else
					CONSOLE_Print( "[ADMINGAME] missing input #2 to checkadmin command" );
			}
			else
				SS >> Server;

			if( !Server.empty( ) )
			{
				string Servers;
				bool FoundServer = false;

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( Servers.empty( ) )
						Servers = (*i)->GetServer( );
					else
						Servers += ", " + (*i)->GetServer( );

					if( (*i)->GetServer( ) == Server )
					{
						FoundServer = true;

						if( (*i)->IsAdmin( Name ) )
							SendChat( player, m_GHost->m_Language->UserIsAnAdmin( Server, Name ) );
						else
							SendChat( player, m_GHost->m_Language->UserIsNotAnAdmin( Server, Name ) );

						break;
					}
				}

				if( !FoundServer )
					SendChat( player, m_GHost->m_Language->ValidServers( Servers ) );
			}
		}

		//
		// !CHECKBAN
		//

		if( Command == "checkban" && !Payload.empty( ) )
		{
			// extract the name and the server
			// e.g. "Varlock useast.battle.net" -> name: "Gen", server: "useast.battle.net"

			string Name;
			string Server;
			stringstream SS;
			SS << Payload;
			SS >> Name;

			if( SS.eof( ) )
			{
				if( m_GHost->m_BNETs.size( ) == 1 )
					Server = m_GHost->m_BNETs[0]->GetServer( );
				else
					CONSOLE_Print( "[ADMINGAME] missing input #2 to checkban command" );
			}
			else
				SS >> Server;

			if( !Server.empty( ) )
			{
				string Servers;
				bool FoundServer = false;

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( Servers.empty( ) )
						Servers = (*i)->GetServer( );
					else
						Servers += ", " + (*i)->GetServer( );

					if( (*i)->GetServer( ) == Server )
					{
						FoundServer = true;
						CDBBan *Ban = (*i)->IsBannedName( Name );

						if( Ban )
							SendChat( player, m_GHost->m_Language->UserWasBannedOnByBecause( Server, Name, Ban->GetDate( ), Ban->GetDaysRemaining( ), Ban->GetAdmin( ), Ban->GetReason( ), Ban->GetExpireDate() ) );
						else
							SendChat( player, m_GHost->m_Language->UserIsNotBanned( Server, Name ) );

						break;
					}
				}

				if( !FoundServer )
					SendChat( player, m_GHost->m_Language->ValidServers( Servers ) );
			}
		}

		//
		// !COUNTADMINS
		//

		if( Command == "countadmins" )
		{
			string Server = Payload;

			if( Server.empty( ) && m_GHost->m_BNETs.size( ) == 1 )
				Server = m_GHost->m_BNETs[0]->GetServer( );

			if( !Server.empty( ) )
				m_PairedAdminCounts.push_back( PairedAdminCount( player->GetName( ), m_GHost->m_DB->ThreadedAdminCount( Server ) ) );
		}

		//
		// !COUNTBANS
		//

		if( Command == "countbans" )
		{
			string Server = Payload;

			if( Server.empty( ) && m_GHost->m_BNETs.size( ) == 1 )
				Server = m_GHost->m_BNETs[0]->GetServer( );

			if( !Server.empty( ) )
				m_PairedBanCounts.push_back( PairedBanCount( player->GetName( ), m_GHost->m_DB->ThreadedBanCount( Server ) ) );
		}

		//
		// !DELADMIN
		//

		if( Command == "deladmin" && !Payload.empty( ) )
		{
			// extract the name and the server
			// e.g. "Varlock useast.battle.net" -> name: "Gen", server: "useast.battle.net"

			string Name;
			string Server;
			stringstream SS;
			SS << Payload;
			SS >> Name;

			if( SS.eof( ) )
			{
				if( m_GHost->m_BNETs.size( ) == 1 )
					Server = m_GHost->m_BNETs[0]->GetServer( );
				else
					CONSOLE_Print( "[ADMINGAME] missing input #2 to deladmin command" );
			}
			else
				SS >> Server;

			if( !Server.empty( ) )
			{
				string Servers;
				bool FoundServer = false;

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( Servers.empty( ) )
						Servers = (*i)->GetServer( );
					else
						Servers += ", " + (*i)->GetServer( );

					if( (*i)->GetServer( ) == Server )
					{
						FoundServer = true;

						if( !(*i)->IsAdmin( Name ) )
							SendChat( player, m_GHost->m_Language->UserIsNotAnAdmin( Server, Name ) );
						else
							m_PairedAdminRemoves.push_back( PairedAdminRemove( player->GetName( ), m_GHost->m_DB->ThreadedAdminRemove( Server, Name ) ) );

						break;
					}
				}

				if( !FoundServer )
					SendChat( player, m_GHost->m_Language->ValidServers( Servers ) );
			}
		}

		//
		// !DELBAN
		// !UNBAN
		//

		if( ( Command == "delban" || Command == "unban" ) && !Payload.empty( ) )
			m_PairedBanRemoves.push_back( PairedBanRemove( player->GetName( ), m_GHost->m_DB->ThreadedBanRemove( Payload, 0 ) ) );

		//
		// !DISABLE
		//

		if( Command == "disable" )
		{
			SendChat( player, m_GHost->m_Language->BotDisabled( ) );
			m_GHost->m_Enabled = false;
		}

		//
		// !DOWNLOADS
		//

		if( Command == "downloads" && !Payload.empty( ) )
		{
			uint32_t Downloads = UTIL_ToUInt32( Payload );

			if( Downloads == 0 )
			{
				SendChat( player, m_GHost->m_Language->MapDownloadsDisabled( ) );
				m_GHost->m_AllowDownloads = 0;
			}
			else if( Downloads == 1 )
			{
				SendChat( player, m_GHost->m_Language->MapDownloadsEnabled( ) );
				m_GHost->m_AllowDownloads = 1;
			}
			else if( Downloads == 2 )
			{
				SendChat( player, m_GHost->m_Language->MapDownloadsConditional( ) );
				m_GHost->m_AllowDownloads = 2;
			}
		}

		//
		// !ENABLE
		//

		if( Command == "enable" )
		{
			SendChat( player, m_GHost->m_Language->BotEnabled( ) );
			m_GHost->m_Enabled = true;
		}

		//
		// !END
		//

		if( Command == "end" && !Payload.empty( ) )
		{
			// todotodo: what if a game ends just as you're typing this command and the numbering changes?

			uint32_t GameNumber = UTIL_ToUInt32( Payload ) - 1;

			if( GameNumber < m_GHost->m_Games.size( ) )
			{
				SendChat( player, m_GHost->m_Language->EndingGame( m_GHost->m_Games[GameNumber]->GetDescription( ) ) );
				CONSOLE_Print( "[GAME: " + m_GHost->m_Games[GameNumber]->GetGameName( ) + "] is over (admin ended game)" );
				m_GHost->m_Games[GameNumber]->StopPlayers( "was disconnected (admin ended game)" );
			}
			else
				SendChat( player, m_GHost->m_Language->GameNumberDoesntExist( Payload ) );
		}

		//
		// !ENFORCESG
		//

		if( Command == "enforcesg" && !Payload.empty( ) )
		{
			// only load files in the current directory just to be safe

			if( Payload.find( "/" ) != string :: npos || Payload.find( "\\" ) != string :: npos )
				SendChat( player, m_GHost->m_Language->UnableToLoadReplaysOutside( ) );
			else
			{
				string File = m_GHost->m_ReplayPath + Payload + ".w3g";

				if( UTIL_FileExists( File ) )
				{
					SendChat( player, m_GHost->m_Language->LoadingReplay( File ) );
					CReplay *Replay = new CReplay( );
					Replay->Load( File, false );
					Replay->ParseReplay( false );
					m_GHost->m_EnforcePlayers = Replay->GetPlayers( );
					delete Replay;
				}
				else
					SendChat( player, m_GHost->m_Language->UnableToLoadReplayDoesntExist( File ) );
			}
		}

		//
		// !EXIT
		// !QUIT
		//

		if( Command == "exit" || Command == "quit" )
		{
			if( Payload == "nice" )
				m_GHost->m_ExitingNice = true;
			else if( Payload == "force" )
				m_Exiting = true;
			else
			{
				if( m_GHost->m_CurrentGame || !m_GHost->m_Games.empty( ) )
					SendChat( player, m_GHost->m_Language->AtLeastOneGameActiveUseForceToShutdown( ) );
				else
					m_Exiting = true;
			}
		}

		//
		// !GETGAME
		//

		if( Command == "getgame" && !Payload.empty( ) )
		{
			uint32_t GameNumber = UTIL_ToUInt32( Payload ) - 1;

			if( GameNumber < m_GHost->m_Games.size( ) )
				SendChat( player, m_GHost->m_Language->GameNumberIs( Payload, m_GHost->m_Games[GameNumber]->GetDescription( ) ) );
			else
				SendChat( player, m_GHost->m_Language->GameNumberDoesntExist( Payload ) );
		}

		//
		// !GETGAMES
		//

		if( Command == "getgames" )
		{
			if( m_GHost->m_CurrentGame )
				SendChat( player, m_GHost->m_Language->GameIsInTheLobby( m_GHost->m_CurrentGame->GetDescription( ), UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ) );
			else
				SendChat( player, m_GHost->m_Language->ThereIsNoGameInTheLobby( UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ) );
		}

		//
		// !HOSTSG
		//

		if( Command == "hostsg" && !Payload.empty( ) )
			m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, true, Payload, User, User, string( ), false );

		//
		// !LOAD (load config file)
		//

		if( Command == "load" )
		{
			if( Payload.empty( ) )
				SendChat( player, m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ) );
			else
			{
				string FoundMapConfigs;

				try
				{
					path MapCFGPath( m_GHost->m_MapCFGPath );
					string Pattern = Payload;
					transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

					if( !exists( MapCFGPath ) )
					{
						CONSOLE_Print( "[ADMINGAME] error listing map configs - map config path doesn't exist" );
						SendChat( player, m_GHost->m_Language->ErrorListingMapConfigs( ) );
					}
					else
					{
						directory_iterator EndIterator;
						path LastMatch;
						uint32_t Matches = 0;

						for( directory_iterator i( MapCFGPath ); i != EndIterator; i++ )
						{
							string FileName = i->filename( );
							string Stem = i->path( ).stem( );
							transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
							transform( Stem.begin( ), Stem.end( ), Stem.begin( ), (int(*)(int))tolower );

							if( !is_directory( i->status( ) ) && i->path( ).extension( ) == ".cfg" && FileName.find( Pattern ) != string :: npos )
							{
								LastMatch = i->path( );
								Matches++;

								if( FoundMapConfigs.empty( ) )
									FoundMapConfigs = i->filename( );
								else
									FoundMapConfigs += ", " + i->filename( );

								// if the pattern matches the filename exactly, with or without extension, stop any further matching

								if( FileName == Pattern || Stem == Pattern )
								{
									Matches = 1;
									break;
								}
							}
						}

						if( Matches == 0 )
							SendChat( player, m_GHost->m_Language->NoMapConfigsFound( ) );
						else if( Matches == 1 )
						{
							string File = LastMatch.filename( );
							SendChat( player, m_GHost->m_Language->LoadingConfigFile( m_GHost->m_MapCFGPath + File ) );
							CConfig MapCFG;
							MapCFG.Read( LastMatch.string( ) );
							m_GHost->m_Map->Load( &MapCFG, m_GHost->m_MapCFGPath + File );
						}
						else
							SendChat( player, m_GHost->m_Language->FoundMapConfigs( FoundMapConfigs ) );
					}
				}
				catch( const exception &ex )
				{
					CONSOLE_Print( string( "[ADMINGAME] error listing map configs - caught exception [" ) + ex.what( ) + "]" );
					SendChat( player, m_GHost->m_Language->ErrorListingMapConfigs( ) );
				}
			}
		}

		//
		// !LOADSG
		//

		if( Command == "loadsg" && !Payload.empty( ) )
		{
			// only load files in the current directory just to be safe

			if( Payload.find( "/" ) != string :: npos || Payload.find( "\\" ) != string :: npos )
				SendChat( player, m_GHost->m_Language->UnableToLoadSaveGamesOutside( ) );
			else
			{
				string File = m_GHost->m_SaveGamePath + Payload + ".w3z";
				string FileNoPath = Payload + ".w3z";

				if( UTIL_FileExists( File ) )
				{
					if( m_GHost->m_CurrentGame )
						SendChat( player, m_GHost->m_Language->UnableToLoadSaveGameGameInLobby( ) );
					else
					{
						SendChat( player, m_GHost->m_Language->LoadingSaveGame( File ) );
						m_GHost->m_SaveGame->Load( File, false );
						m_GHost->m_SaveGame->ParseSaveGame( );
						m_GHost->m_SaveGame->SetFileName( File );
						m_GHost->m_SaveGame->SetFileNameNoPath( FileNoPath );
					}
				}
				else
					SendChat( player, m_GHost->m_Language->UnableToLoadSaveGameDoesntExist( File ) );
			}
		}

		//
		// !PUBNxx (host public game) xx=country name (ex: RO), denies players from that country
		//

		if  (( Command.length()>=6) && ( Command.substr(0,4) == "pubn" ) && 
			(!Payload.empty( )) && (((Command.length()-4) % 2)==0))
		{
			int k = Command.length()-4;
			if (k % 2 != 0)
			{
				SendChat( player, "Syntax error, use for ex: !pubnroes gamename" );
				return true;
			}
			k = k / 2;
			string from="";
			for (int i=1; i<k+1; i++)
			{
				if (i!=1)
					from=from+" "+Command.substr(3+(i-1)*2,2);
				else
					from=Command.substr(3+(i-1)*2,2);
			}
			transform( from.begin( ), from.end( ), from.begin( ), (int(*)(int))toupper );
			SendChat( player, "Country check enabled, denied countries: "+from);
			from=from+ " ??";
			m_GHost->CreateGame(  m_GHost->m_Map, GAME_PUBLIC, false, Payload, User, User, string( ), false );
			m_Countries2=from;
			m_CountryCheck2=true;
			return true;
		}


		//
		// !PUBxx (host public game) xx=country name (ex: RO), accepts only players from that country
		//

		if  (( Command.length()>=5) && ( Command != "pubby" ) && ( Command.substr(0,3) == "pub" ) && 
			(!Payload.empty( )))
		{
			int k = Command.length()-3;
			if (k % 2 != 0)
			{
				SendChat( player, "Syntax error, use for ex: !pubroes gamename" );
				return true;
			}
			k = k / 2;
			string from="";
			for (int i=1; i<k+1; i++)
			{
				if (i!=1)
					from=from+" "+Command.substr(3+(i-1)*2,2);
				else
				from=Command.substr(3+(i-1)*2,2);
			}
			transform( from.begin( ), from.end( ), from.begin( ), (int(*)(int))toupper );
			SendChat( player, "Country check enabled, allowed countries: "+from);
			from=from+ " ??";
			m_GHost->CreateGame(  m_GHost->m_Map, GAME_PUBLIC, false, Payload, User, User, string( ), false );
			m_Countries=from;
			m_CountryCheck=true;
			return true;
		}

		//
		// !REHOSTDELAY
		// !RD
		//
		if ( ( Command == "rehostdelay" || Command == "rd" ) )
		{
			if (Payload.empty())
				SendChat(player, "rehostdelay is set to "+ UTIL_ToString(m_GHost->m_AutoRehostDelay));
			else
			{
				m_GHost->m_AutoRehostDelay = UTIL_ToUInt32(Payload);
				SendChat(player, "rehostdelay set to "+ Payload);
			}
		}

		//
		// !MAP (load map file)
		//

		if( Command == "map" )
		{
			if( Payload.empty( ) )
				SendChat( player, m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ) );
			else
			{
				string FoundMaps;

				try
				{
					path MapPath( m_GHost->m_MapPath );
					string Pattern = Payload;
					transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

					if( !exists( MapPath ) )
					{
						CONSOLE_Print( "[ADMINGAME] error listing maps - map path doesn't exist" );
						SendChat( player, m_GHost->m_Language->ErrorListingMaps( ) );
					}
					else
					{
						directory_iterator EndIterator;
						path LastMatch;
						uint32_t Matches = 0;

						for( directory_iterator i( MapPath ); i != EndIterator; i++ )
						{
							string FileName = i->filename( );
							string Stem = i->path( ).stem( );
							transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
							transform( Stem.begin( ), Stem.end( ), Stem.begin( ), (int(*)(int))tolower );

							if( !is_directory( i->status( ) ) && FileName.find( Pattern ) != string :: npos )
							{
								LastMatch = i->path( );
								Matches++;

								if( FoundMaps.empty( ) )
									FoundMaps = i->filename( );
								else
									FoundMaps += ", " + i->filename( );

								// if the pattern matches the filename exactly, with or without extension, stop any further matching

								if( FileName == Pattern || Stem == Pattern )
								{
									Matches = 1;
									break;
								}
							}
						}

						if( Matches == 0 )
							SendChat( player, m_GHost->m_Language->NoMapsFound( ) );
						else if( Matches == 1 )
						{
							string File = LastMatch.filename( );
							SendChat( player, m_GHost->m_Language->LoadingConfigFile( File ) );

							// hackhack: create a config file in memory with the required information to load the map

							CConfig MapCFG;
							MapCFG.Set( "map_path", "Maps\\Download\\" + File );
							MapCFG.Set( "map_localpath", File );
							m_GHost->m_Map->Load( &MapCFG, File );
						}
						else
							SendChat( player, m_GHost->m_Language->FoundMaps( FoundMaps ) );
					}
				}
				catch( const exception &ex )
				{
					CONSOLE_Print( string( "[ADMINGAME] error listing maps - caught exception [" ) + ex.what( ) + "]" );
					SendChat( player, m_GHost->m_Language->ErrorListingMaps( ) );
				}
			}
		}

		//
		// !PRIV (host private game)
		//

		if( Command == "priv" && !Payload.empty( ) )
			m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, false, Payload, User, User, string( ), false );

		//
		// !PRIVBY (host private game by other player)
		//

		if( Command == "privby" && !Payload.empty( ) )
		{
			// extract the owner and the game name
			// e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Gen", game name: "dota 6.54b arem ~~~"

			string Owner;
			string GameName;
			string :: size_type GameNameStart = Payload.find( " " );

			if( GameNameStart != string :: npos )
			{
				Owner = Payload.substr( 0, GameNameStart );
				GameName = Payload.substr( GameNameStart + 1 );
				m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, false, GameName, Owner, User, string( ), false );
			}
		}

		//
		// !PUB (host public game)
		//

		if( Command == "pub" && !Payload.empty( ) )
			m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, Payload, User, User, string( ), false );

		//
		// !PUBBY (host public game by other player)
		//

		if( Command == "pubby" && !Payload.empty( ) )
		{
			// extract the owner and the game name
			// e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Gen", game name: "dota 6.54b arem ~~~"

			string Owner;
			string GameName;
			string :: size_type GameNameStart = Payload.find( " " );

			if( GameNameStart != string :: npos )
			{
				Owner = Payload.substr( 0, GameNameStart );
				GameName = Payload.substr( GameNameStart + 1 );
				m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, GameName, Owner, User, string( ), false );
			}
		}

		//
		// !RELOAD
		//

		if( Command == "reload" )
		{
			SendChat( player, m_GHost->m_Language->ReloadingConfigurationFiles( ) );
			m_GHost->ReloadConfig( );
		}

		//
		// !SAY
		//

		if( Command == "say" && !Payload.empty( ) )
		{
			for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				(*i)->QueueChatCommand( Payload );
		}

		//
		// !SAYGAME
		//

		if( Command == "saygame" && !Payload.empty( ) )
		{
			// extract the game number and the message
			// e.g. "3 hello everyone" -> game number: "3", message: "hello everyone"

			uint32_t GameNumber;
			string Message;
			stringstream SS;
			SS << Payload;
			SS >> GameNumber;

			if( SS.fail( ) )
				CONSOLE_Print( "[ADMINGAME] bad input #1 to saygame command" );
			else
			{
				if( SS.eof( ) )
					CONSOLE_Print( "[ADMINGAME] missing input #2 to saygame command" );
				else
				{
					getline( SS, Message );
					string :: size_type Start = Message.find_first_not_of( " " );

					if( Start != string :: npos )
						Message = Message.substr( Start );

					if( GameNumber - 1 < m_GHost->m_Games.size( ) )
						m_GHost->m_Games[GameNumber - 1]->SendAllChat( "ADMIN: " + Message );
					else
						SendChat( player, m_GHost->m_Language->GameNumberDoesntExist( UTIL_ToString( GameNumber ) ) );
				}
			}
		}

		//
		// !SAYGAMES
		//

		if( Command == "saygames" && !Payload.empty( ) )
		{
			if( m_GHost->m_CurrentGame )
				m_GHost->m_CurrentGame->SendAllChat( Payload );

			for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); i++ )
				(*i)->SendAllChat( "ADMIN: " + Payload );
		}

		//
		// !GETNAMES
		//

		if( Command == "getnames" )
		{
			string GameList = "Lobby: ";
			if( m_GHost->m_CurrentGame )
				GameList += m_GHost->m_CurrentGame->GetGameName( );
			else
				GameList += "-";
			GameList += "; Started:";
			uint32_t Index = 0;
			while( Index < m_GHost->m_Games.size( ) )
			{
				GameList += " [" + UTIL_ToString( Index + 1 ) + "] " + m_GHost->m_Games[Index]->GetGameName( );
				Index++;
			}
			SendChat( player, GameList);
		}

		//
		// !UDP
		//

		if( Command == "udp" )
		{
			m_GHost->m_UDPConsole = !m_GHost->m_UDPConsole;
			if (m_GHost->m_UDPConsole)
				SendChat( player, "UDP Console ON" );
			else
				SendChat( player, "UDP Console OFF" );
		}

		//
		// !ACCESS , !ACCLST, !ACC
		//

		if(( Command == "access" || Command == "acc" || Command == "acclst"))
		{
			// show available commands
			if (Payload.empty() || Command == "acclst")
			{
				string cmds = string();
				for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
				{
					cmds +=m_GHost->m_Commands[i]+"["+UTIL_ToString(i)+"] ";

					if (i<m_GHost->m_Commands.size()-1)
						cmds +=" ";
				}
				SendAllChat( cmds );
				return true;
			}
			uint32_t access = 0;
			bool isAdmin = false;
			string saccess = string();
			string cmd = string();
			string acc = string();
			string suser = Payload;
			uint32_t scmd = 0;
			uint32_t sacc = 0;
			uint32_t Mask = 1;
			uint32_t son = 0;
			bool showonly = true;

			if( Payload.find( " " )!= string :: npos)
			{
				showonly = false;

				stringstream SS;
				SS<<Payload;
				SS>>suser;
				SS>>scmd;
				if( SS.fail( )  )
				{
					SendAllChat( "bad input #2 to access command" );
					return true;
				}
				SS>>sacc;
				if( SS.fail( )  )
				{
					// acc suser 1 or 0 : set access to all commands or none.
					isAdmin = false;
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == m_Server && (*i)->IsAdmin( suser ) )
						{
							isAdmin = true;
							access = (*i)->LastAccess();
							break;
						}
					}
					if (!isAdmin)
					{
						SendAllChat( suser+" is not an admin on "+m_Server);
						return true;
					}
					if (scmd == 1)
						access = CMDAccessAll();
					else
						access = 0;
					m_GHost->m_DB->AdminSetAccess(m_Server, suser, access);
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == m_Server)
						{
							(*i)->UpdateAccess( suser, access);
							break;
						}
					}

					for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
					{
						cmd = m_GHost->Commands(i);
						if (access & Mask)
							acc = "1";
						else
							acc = "0";
						if (acc =="1" && m_GHost->CommandAllowedToShow(cmd))
						{
							if (saccess.length()>0)
								saccess +=", ";
							saccess +=cmd;
						}
						Mask = Mask * 2;
					}
					SendAllChat( suser+" can: "+saccess);
					return true;
				}
				if (scmd>=m_GHost->m_Commands.size())
				{
					SendAllChat( "cmd must be as high as "+UTIL_ToString(m_GHost->m_Commands.size()-1) );
					return true;
				}
				if (sacc!=0 && sacc!=1)
				{
					SendAllChat( "acc must be 0 or 1" );
					return true;
				}
			}
			else 
				showonly = true;

			isAdmin = false;
			for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == m_Server && (*i)->IsAdmin( suser ) )
				{
					isAdmin = true;
					access = (*i)->LastAccess();
					break;
				}
			}

			if (!isAdmin)
				SendAllChat( suser+" is not an admin on "+m_Server);
			else
				if (showonly)
					// show currently accessable commands
				{
					for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
					{
						cmd = m_GHost->Commands(i);
						if (access & Mask)
							acc = "1";
						else
							acc = "0";
						if (acc =="1" && m_GHost->CommandAllowedToShow(cmd))
						{
							if (saccess.length()>0)
								saccess +=", ";
							saccess +=cmd;
						}
						Mask = Mask * 2;
					}
					SendAllChat( suser+" can: "+saccess);
				} else
					// set access
				{
					Mask = 1;
					if (scmd != 0)
						for (unsigned int k=1;k<=scmd;k++)
							Mask = Mask * 2;
					if (Mask > access)
						son = 0;
					else
						son = access & Mask;
					if (son==sacc)
					{
						if (sacc == 0)
						{
							SendAllChat("Admin "+suser+ " already doesn't have access to "+m_GHost->m_Commands[scmd]);
						}
						else
						{
							SendAllChat("Admin "+suser+ " already has access to "+m_GHost->m_Commands[scmd]);
						}
						return true;
					}
					if (sacc == 1)
						access+= Mask;
					else
						access -= Mask;
					m_GHost->m_DB->AdminSetAccess(m_Server, suser, access);
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == m_Server)
						{
							(*i)->UpdateAccess( suser, access);
							break;
						}
					}
					Mask = 1;
					for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
					{
						cmd = m_GHost->Commands(i);
						if (access & Mask)
							acc = "1";
						else
							acc = "0";
						if (acc =="1" && m_GHost->CommandAllowedToShow(cmd))
						{
							if (saccess.length()>0)
								saccess +=", ";
							saccess +=cmd;
						}
						Mask = Mask * 2;
					}
					SendAllChat( suser+" can: "+saccess);
				}
		}


		//
		// !UNHOST
		//

		if( Command == "unhost" )
		{
			if( m_GHost->m_CurrentGame )
			{
				if( m_GHost->m_CurrentGame->GetCountDownStarted( ) )
					SendChat( player, m_GHost->m_Language->UnableToUnhostGameCountdownStarted( m_GHost->m_CurrentGame->GetDescription( ) ) );
				else
				{
					SendChat( player, m_GHost->m_Language->UnhostingGame( m_GHost->m_CurrentGame->GetDescription( ) ) );
					m_GHost->m_CurrentGame->SetExiting( true );
				}
			}
			else
				SendChat( player, m_GHost->m_Language->UnableToUnhostGameNoGameInLobby( ) );
		}

		//
		// !W
		//

		if( Command == "w" && !Payload.empty( ) )
		{
			// extract the name and the message
			// e.g. "Varlock hello there!" -> name: "Gen", message: "hello there!"

			string Name;
			string Message;
			string :: size_type MessageStart = Payload.find( " " );

			if( MessageStart != string :: npos )
			{
				Name = Payload.substr( 0, MessageStart );
				Message = Payload.substr( MessageStart + 1 );

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					(*i)->QueueChatCommand( Message, Name, true );
			}
		}
	}
	else
		CONSOLE_Print( "[ADMINGAME] user [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );

	/*********************
	* NON ADMIN COMMANDS *
	*********************/

	//
	// !PASSWORD
	//

	if( Command == "password" && !player->GetLoggedIn( ) )
	{
		if( !m_Password.empty( ) && Payload == m_Password )
		{
			CONSOLE_Print( "[ADMINGAME] user [" + User + "] logged in" );
			SendChat( player, m_GHost->m_Language->AdminLoggedIn( ) );
			player->SetLoggedIn( true );
		}
		else
		{
			uint32_t LoginAttempts = player->GetLoginAttempts( ) + 1;
			player->SetLoginAttempts( LoginAttempts );
			CONSOLE_Print( "[ADMINGAME] user [" + User + "] login attempt failed" );
			SendChat( player, m_GHost->m_Language->AdminInvalidPassword( UTIL_ToString( LoginAttempts ) ) );

			if( LoginAttempts >= 1 )
			{
				player->SetDeleteMe( true );
				player->SetLeftReason( "was kicked for too many failed login attempts" );
				player->SetLeftCode( PLAYERLEAVE_LOBBY );
				OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );

				// tempban for 5 seconds to prevent bruteforcing

				m_TempBans.push_back( TempBan( player->GetExternalIPString( ), GetTime( ) ) );
			}
		}
	}

	// always hide chat commands from other players in the admin game
	// note: this is actually redundant because we've already set m_MuteLobby = true so this has no effect
	// if you actually wanted to relay chat commands you would have to set m_MuteLobby = false AND return false here

	return true;
}
