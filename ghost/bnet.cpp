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

//#include "gameprotocol.h"
#ifdef WIN32
//#include "dirent.h"
#endif
//#include "stdio.h"
//#include "stdlib.h"
#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "commandpacket.h"
#include "ghostdb.h"
#include "bncsutilinterface.h"
#include "bnlsclient.h"
#include "bnetprotocol.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "replay.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
//#include <time.h>
#include <boost/filesystem.hpp>

using namespace boost :: filesystem;

//
// CBNET
//

CBNET :: CBNET( CGHost *nGHost, string nServer, string nServerAlias, string nBNLSServer, uint16_t nBNLSPort, uint32_t nBNLSWardenCookie, string nCDKeyROC, string nCDKeyTFT, string nCountryAbbrev, string nCountry, uint32_t nLocaleID, string nUserName, string nUserPassword, string nFirstChannel, string nRootAdmin, char nCommandTrigger, bool nHoldFriends, bool nHoldClan, bool nPublicCommands, unsigned char nWar3Version, BYTEARRAY nEXEVersion, BYTEARRAY nEXEVersionHash, string nPasswordHashType, string nPVPGNRealmName, uint32_t nMaxMessageLength, uint32_t nHostCounterID )
{
	// todotodo: append path seperator to Warcraft3Path if needed

	m_GHost = nGHost;
	m_Socket = new CTCPClient( );
	m_Protocol = new CBNETProtocol( );
	m_BNLSClient = NULL;
	m_BNCSUtil = new CBNCSUtilInterface( nUserName, nUserPassword );
	m_CallableAdminList = m_GHost->m_DB->ThreadedAdminList( nServer );
	m_CallableBanList = m_GHost->m_DB->ThreadedBanList( nServer );
	m_CallableAccessesList = m_GHost->m_DB->ThreadedAccessesList( nServer );
	m_CallableSafeList = m_GHost->m_DB->ThreadedSafeList( nServer );
	m_CallableSafeListV = m_GHost->m_DB->ThreadedSafeListV( nServer );
	m_CallableNotes = m_GHost->m_DB->ThreadedNotes( nServer );
	m_CallableNotesN = m_GHost->m_DB->ThreadedNotesN( nServer );
	m_CallableTodayGamesCount = m_GHost->m_DB->ThreadedTodayGamesCount();
	m_Exiting = false;
	m_Server = nServer;
	string LowerServer = m_Server;
	transform( LowerServer.begin( ), LowerServer.end( ), LowerServer.begin( ), (int(*)(int))tolower );

	if( !nServerAlias.empty( ) )
		m_ServerAlias = nServerAlias;
	else if( LowerServer == "useast.battle.net" )
		m_ServerAlias = "USEast";
	else if( LowerServer == "uswest.battle.net" )
		m_ServerAlias = "USWest";
	else if( LowerServer == "asia.battle.net" )
		m_ServerAlias = "Asia";
	else if( LowerServer == "europe.battle.net" )
		m_ServerAlias = "Europe";
	else if( LowerServer == "server.eurobattle.net" )
		m_ServerAlias = "XPAM";
	else if( LowerServer == "europe.warcraft3.eu" )
		m_ServerAlias = "W3EU";
	else if( LowerServer == "200.51.203.231" )
		m_ServerAlias = "Ombu";
	else
		m_ServerAlias = m_Server;

	if( nPasswordHashType == "pvpgn" && !nBNLSServer.empty( ) )
	{
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] pvpgn connection found with a configured BNLS server, ignoring BNLS server" );
		nBNLSServer.clear( );
		nBNLSPort = 0;
		nBNLSWardenCookie = 0;
	}

	m_BNLSServer = nBNLSServer;
	m_BNLSPort = nBNLSPort;
	m_BNLSWardenCookie = nBNLSWardenCookie;
	m_CDKeyROC = nCDKeyROC;
	m_CDKeyTFT = nCDKeyTFT;

	// remove dashes from CD keys and convert to uppercase

	m_CDKeyROC.erase( remove( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), '-' ), m_CDKeyROC.end( ) );
	m_CDKeyTFT.erase( remove( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), '-' ), m_CDKeyTFT.end( ) );
	transform( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), m_CDKeyROC.begin( ), (int(*)(int))toupper );
	transform( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), m_CDKeyTFT.begin( ), (int(*)(int))toupper );

	if( m_CDKeyROC.size( ) != 26 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - your ROC CD key is not 26 characters long and is probably invalid" );

	if( m_GHost->m_TFT && m_CDKeyTFT.size( ) != 26 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - your TFT CD key is not 26 characters long and is probably invalid" );

	m_CountryAbbrev = nCountryAbbrev;
	m_Country = nCountry;
	m_LocaleID = nLocaleID;
	m_UserName = nUserName;
	m_UserPassword = nUserPassword;
	m_FirstChannel = nFirstChannel;
	m_RootAdmin = nRootAdmin;
	transform( m_RootAdmin.begin( ), m_RootAdmin.end( ), m_RootAdmin.begin( ), (int(*)(int))tolower );
	m_CommandTrigger = nCommandTrigger;
	m_War3Version = nWar3Version;
	m_EXEVersion = nEXEVersion;
	m_EXEVersionHash = nEXEVersionHash;
	m_PasswordHashType = nPasswordHashType;
	m_PVPGNRealmName = nPVPGNRealmName;
	m_MaxMessageLength = nMaxMessageLength;
	m_HostCounterID = nHostCounterID;
	m_LastDisconnectedTime = 0;
	m_LastConnectionAttemptTime = 0;
	m_LastNullTime = 0;
	m_LastOutPacketTicks = 0;
	m_LastOutPacketTicks2 = 0;
	m_LastOutPacketSize = 0;
	m_LastAdminRefreshTime = GetTime( );
	m_LastBanRefreshTime = GetTime( );
	m_LastGameCountRefreshTime = GetTime( );
	m_FirstConnect = true;
	m_LastMassWhisperTime = 0;
	m_RemoveTempBansUser = string();
	m_RemoveTempBans = false;
	m_TodayGamesCount = 0;
	m_Whereis = false;
	m_LastMars = GetTime()-10;
//	m_LastChatCommandTime = 0;
//	m_AutoHostMaximumGames = 0;
	//m_AutoHostAutoStartPlayers = 0;
	//m_LastAutoHostTime = 0;
	m_LastStats = 0;
	m_WaitingToConnect = true;
	m_LoggedIn = false;
	m_InChat = false;
	m_HoldFriends = nHoldFriends;
	m_HoldClan = nHoldClan;
	m_PublicCommands = nPublicCommands;
}

CBNET :: ~CBNET( )
{
	delete m_Socket;
	delete m_Protocol;
	delete m_BNLSClient;

	while( !m_Packets.empty( ) )
	{
		delete m_Packets.front( );
		m_Packets.pop( );
	}

	delete m_BNCSUtil;

	for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); i++ )
		delete *i;

	for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
		delete *i;

	for( vector<PairedAdminCount> :: iterator i = m_PairedAdminCounts.begin( ); i != m_PairedAdminCounts.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedAdminAdd> :: iterator i = m_PairedAdminAdds.begin( ); i != m_PairedAdminAdds.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedAdminRemove> :: iterator i = m_PairedAdminRemoves.begin( ); i != m_PairedAdminRemoves.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedRanks> :: iterator i = m_PairedRanks.begin( ); i != m_PairedRanks.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedCalculateScores> :: iterator i = m_PairedCalculateScores.begin( ); i != m_PairedCalculateScores.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedSafeAdd> :: iterator i = m_PairedSafeAdds.begin( ); i != m_PairedSafeAdds.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedSafeRemove> :: iterator i = m_PairedSafeRemoves.begin( ); i != m_PairedSafeRemoves.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedRunQuery> :: iterator i = m_PairedRunQueries.begin( ); i != m_PairedRunQueries.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedBanCount> :: iterator i = m_PairedBanCounts.begin( ); i != m_PairedBanCounts.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedBanRemove> :: iterator i = m_PairedBanRemoves.begin( ); i != m_PairedBanRemoves.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );
	for( vector<PairedGameUpdate> :: iterator i = m_PairedGameUpdates.begin( ); i != m_PairedGameUpdates.end( ); ++i )
		m_GHost->m_Callables.push_back( i->second );

	if( m_CallableAdminList )
		m_GHost->m_Callables.push_back( m_CallableAdminList );

	if( m_CallableSafeList )
		m_GHost->m_Callables.push_back( m_CallableSafeList );

	if( m_CallableAccessesList )
		m_GHost->m_Callables.push_back( m_CallableAccessesList );

	if( m_CallableBanList )
		m_GHost->m_Callables.push_back( m_CallableBanList );

	for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); i++ )
		delete *i;
}

BYTEARRAY CBNET :: GetUniqueName( )
{
	return m_Protocol->GetUniqueName( );
}

unsigned int CBNET :: SetFD( void *fd, void *send_fd, int *nfds )
{
	unsigned int NumFDs = 0;

	if( !m_Socket->HasError( ) && m_Socket->GetConnected( ) )
	{
		m_Socket->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
		NumFDs++;

		if( m_BNLSClient )
			NumFDs += m_BNLSClient->SetFD( fd, send_fd, nfds );
	}

	return NumFDs;
}

bool CBNET :: Update( void *fd, void *send_fd )
{
	//
	// update callables
	//

	for( vector<PairedAdminCount> :: iterator i = m_PairedAdminCounts.begin( ); i != m_PairedAdminCounts.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			uint32_t Count = i->second->GetResult( );

			if( Count == 0 )
				QueueChatCommand( m_GHost->m_Language->ThereAreNoAdmins( m_Server ), i->first, !i->first.empty( ) );
			else if( Count == 1 )
				QueueChatCommand( m_GHost->m_Language->ThereIsAdmin( m_Server ), i->first, !i->first.empty( ) );
			else
				QueueChatCommand( m_GHost->m_Language->ThereAreAdmins( m_Server, UTIL_ToString( Count ) ), i->first, !i->first.empty( ) );

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
				AddAdmin( i->second->GetUser( ) );
				QueueChatCommand( m_GHost->m_Language->AddedUserToAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->ErrorAddingUserToAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );

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
				RemoveAdmin( i->second->GetUser( ) );
				QueueChatCommand( m_GHost->m_Language->DeletedUserFromAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->ErrorDeletingUserFromAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedAdminRemoves.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedRanks> :: iterator i = m_PairedRanks.begin( ); i != m_PairedRanks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			string Scores = i->second->GetResult();
//			if (Scores.length()>160)
//				Scores = Scores.substr(0,160);
			QueueChatCommand(Scores, i->first, !i->first.empty( ));

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedRanks.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedCalculateScores> :: iterator i = m_PairedCalculateScores.begin( ); i != m_PairedCalculateScores.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			bool ok = i->second->GetResult();
			m_GHost->m_CalculatingScores = false;
			if (ok)
			{
				m_GHost->CalculateScoresCount();
				QueueChatCommand( UTIL_ToString(m_GHost->ScoresCount())+ " scores have been calculated", i->first, !i->first.empty( ));
			}
			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedCalculateScores.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedSafeAdd> :: iterator i = m_PairedSafeAdds.begin( ); i != m_PairedSafeAdds.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				AddSafe( i->second->GetUser( ), i->second->GetVoucher( ) );
				QueueChatCommand(m_GHost->m_Language->AddedPlayerToTheSafeList(i->second->GetUser( )), i->first, !i->first.empty( ));
			}
			else
				QueueChatCommand("Error adding "+i->second->GetUser( )+" to safelist", i->first, !i->first.empty( ));

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedSafeAdds.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedSafeRemove> :: iterator i = m_PairedSafeRemoves.begin( ); i != m_PairedSafeRemoves.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				RemoveSafe( i->second->GetUser( ) );
				QueueChatCommand(m_GHost->m_Language->RemovedPlayerFromTheSafeList(i->second->GetUser( )), i->first, !i->first.empty( ));
			}
			else
				QueueChatCommand("Error removing "+i->second->GetUser( )+" from safelist", i->first, !i->first.empty( ));

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedSafeRemoves.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedRunQuery> :: iterator i = m_PairedRunQueries.begin( ); i != m_PairedRunQueries.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			string msg = i->second->GetResult( );
			QueueChatCommand("Query returned: "+msg, i->first, !i->first.empty( ));

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedRunQueries.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedBanCount> :: iterator i = m_PairedBanCounts.begin( ); i != m_PairedBanCounts.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			uint32_t Count = i->second->GetResult( );

			if( Count == 0 )
				QueueChatCommand( m_GHost->m_Language->ThereAreNoBannedUsers( m_Server ), i->first, !i->first.empty( ) );
			else if( Count == 1 )
				QueueChatCommand( m_GHost->m_Language->ThereIsBannedUser( m_Server ), i->first, !i->first.empty( ) );
			else
				QueueChatCommand( m_GHost->m_Language->ThereAreBannedUsers( m_Server, UTIL_ToString( Count ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanCounts.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				string sTDate = string();
				string sDate = string();
				struct tm * timeinfo;
				char buffer [80];
				time_t Now = time( NULL );
				timeinfo = localtime( &Now );
				strftime (buffer,80,"%d-%m-%Y",timeinfo);  
				sTDate = buffer;
				if (i->second->GetExpireDayTime()>0)
				{
					Now += 3600*24*i->second->GetExpireDayTime();
					timeinfo = localtime( &Now );
					strftime (buffer,80,"%d-%m-%Y",timeinfo);  
					sDate = buffer;
				}

				AddBan( i->second->GetUser( ), i->second->GetIP( ), sTDate, i->second->GetGameName( ), i->second->GetAdmin( ), i->second->GetReason( ), sDate );
				if (i->second->GetExpireDayTime()<=0)
					QueueChatCommand( m_GHost->m_Language->BannedUser( i->second->GetServer( ), i->second->GetUser( ) ), i->first, !i->first.empty( ) );
				else
					QueueChatCommand( m_GHost->m_Language->TempBannedUser( i->second->GetServer( ), i->second->GetUser( ), UTIL_ToString(i->second->GetExpireDayTime()) ), i->first, !i->first.empty( ) );
				m_GHost->UDPChatSend("|ban "+i->second->GetUser( ) );

				if (m_GHost->m_BanBannedFromChannel)
					ImmediateChatCommand( "/ban "+i->second->GetUser( ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->ErrorBanningUser( i->second->GetServer( ), i->second->GetUser( ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanAdds.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedBanRemove> :: iterator i = m_PairedBanRemoves.begin( ); i != m_PairedBanRemoves.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				RemoveBan( i->second->GetUser( ) );
				QueueChatCommand( m_GHost->m_Language->UnbannedUser( i->second->GetUser( ) ), i->first, !i->first.empty( ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->ErrorUnbanningUser( i->second->GetUser( ) ), i->first, !i->first.empty( ) );

			if (m_GHost->m_UnbanRemovesChannelBans)
				QueueChatCommand("/unban "+i->second->GetUser( ));

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanRemoves.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBGamePlayerSummary *GamePlayerSummary = i->second->GetResult( );

			if( GamePlayerSummary )
				QueueChatCommand( m_GHost->m_Language->HasPlayedGamesWithThisBot( i->second->GetName( ), GamePlayerSummary->GetFirstGameDateTime( ), GamePlayerSummary->GetLastGameDateTime( ), UTIL_ToString( GamePlayerSummary->GetTotalGames( ) ), UTIL_ToString( (float)GamePlayerSummary->GetAvgLoadingTime( ) / 1000, 2 ), UTIL_ToString( GamePlayerSummary->GetAvgLeftPercent( ) ) ), i->first, !i->first.empty( ) );
			else
				QueueChatCommand( m_GHost->m_Language->HasntPlayedGamesWithThisBot( i->second->GetName( ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedGPSChecks.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBDotAPlayerSummary *DotAPlayerSummary = i->second->GetResult( );

			bool sd = false;
			bool Whisper = !i->first.empty();
			string name = i->first;

			if (i->first[0]=='%')
			{
				name = i->first.substr(1,i->first.length()-1);
				Whisper = i->first.length()>1;
				sd = true;
			}

			if (sd)
			if( DotAPlayerSummary )
			{
				string RankS = UTIL_ToString( DotAPlayerSummary->GetRank());
				uint32_t scorescount = m_GHost->ScoresCount();

				if (DotAPlayerSummary->GetRank()>0)
					RankS = RankS + "/" + UTIL_ToString(scorescount);

				QueueChatCommand( m_GHost->m_Language->HasPlayedDotAGamesWithThisBot2( i->second->GetName( ),
					UTIL_ToString(DotAPlayerSummary->GetTotalGames( )), UTIL_ToString( DotAPlayerSummary->GetWinsPerGame( )),
					UTIL_ToString( DotAPlayerSummary->GetLossesPerGame( )),UTIL_ToString( DotAPlayerSummary->GetKillsPerGame( )),
					UTIL_ToString( DotAPlayerSummary->GetDeathsPerGame( )),UTIL_ToString( DotAPlayerSummary->GetCreepKillsPerGame( )),
					UTIL_ToString( DotAPlayerSummary->GetCreepDeniesPerGame( )),UTIL_ToString( DotAPlayerSummary->GetAssistsPerGame( )),
					UTIL_ToString( DotAPlayerSummary->GetNeutralKillsPerGame( )),UTIL_ToString( DotAPlayerSummary->GetTowerKillsPerGame( )),
					UTIL_ToString( DotAPlayerSummary->GetRaxKillsPerGame( )), UTIL_ToString( DotAPlayerSummary->GetCourierKillsPerGame( )), UTIL_ToString2( DotAPlayerSummary->GetScore()),RankS), name, Whisper );
			}
			else
				QueueChatCommand( m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot( i->second->GetName( ) ), name, Whisper );


			if (!sd)
			if( DotAPlayerSummary )
			{
				string Summary = m_GHost->m_Language->HasPlayedDotAGamesWithThisBot(	i->second->GetName( ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalGames( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalWins( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalLosses( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalDeaths( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalCreepKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalCreepDenies( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalAssists( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalNeutralKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalTowerKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalRaxKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalCourierKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgDeaths( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgCreepKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgCreepDenies( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgAssists( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgNeutralKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgTowerKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgRaxKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgCourierKills( ), 2 ) );

				QueueChatCommand( Summary, name, Whisper );
			}
			else
				QueueChatCommand( m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot( i->second->GetName( ) ), name, Whisper );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedDPSChecks.erase( i );
		}
		else
			i++;
	}

	// remove temp bans expiring this day every 6h
	if( GetLoggedIn() )
	if (GetTicks()-m_LastTempBansRemoveTime > 1000*3600*6 || m_RemoveTempBans)
	{
		m_RemoveTempBans = false;
		m_RemoveTempBansList = m_GHost->m_DB->RemoveTempBanList( m_Server);
		m_LastTempBansRemoveTime = GetTicks();
		uint32_t Count = m_GHost->m_DB->RemoveTempBans( m_Server);
		string mess = "Removed "+ UTIL_ToString(Count) +" temporary bans which expire today";
		if (!m_RemoveTempBansUser.empty())
		{
			ImmediateChatCommand("/w "+ m_RemoveTempBansUser+" "+mess);
			m_RemoveTempBansUser = string();
		}
		CONSOLE_Print("[BNET: " + m_ServerAlias + "] "+mess);
		m_LastTempBanRemoveTime = GetTicks();
	}

	// remove a temp ban every 10 seconds
	if( GetLoggedIn())
	if (m_RemoveTempBansList.size()>0)
	if (GetTicks()-m_LastTempBanRemoveTime > 10*1000)
	{
		string Forgiven = m_RemoveTempBansList[0];
		m_LastTempBanRemoveTime = GetTicks();
		m_RemoveTempBansList.erase(m_RemoveTempBansList.begin());
		if (m_GHost->m_UnbanRemovesChannelBans)
			QueueChatCommand("/unban "+Forgiven);
		m_GHost->m_DB->BanRemove( Forgiven, 1 );
		CONSOLE_Print("[BNET: " + m_ServerAlias + "] Removed "+ Forgiven+"'s temporary ban");
	}

	// remove a ban every 10 seconds
	if( GetLoggedIn())
		if (m_RemoveBansList.size()>0)
			if (GetTicks()-m_LastBanRemoveTime > 10*1000)
			{
				string Forgiven = m_RemoveBansList[0];
				m_LastBanRemoveTime = GetTicks();
				m_RemoveBansList.erase(m_RemoveBansList.begin());
				if (m_GHost->m_UnbanRemovesChannelBans)
					QueueChatCommand("/unban "+Forgiven);
//				m_GHost->m_DB->BanRemove( Forgiven, 1 );
				CONSOLE_Print("[BNET: " + m_ServerAlias + "] Removed "+ Forgiven+"'s ban");
			}

	// refresh friends list every 5 minutes
	if (GetTime()>= m_LastFriendListTime + 300)
	{
		SendGetFriendsList();
		m_LastFriendListTime = GetTime();
	}

	// check if a friend entered bnet and whisper status
	if (m_FriendsEnteringBnet.size()>0)
	if (GetTime() > m_LastFriendEnteredWhisper + 12)
	{
		string sUser = m_FriendsEnteringBnet[0];
		m_FriendsEnteringBnet.erase(m_FriendsEnteringBnet.begin());
		string sMsg = string ();
		if (m_GHost->m_CurrentGame)
		{
			string slots = UTIL_ToString(m_GHost->m_CurrentGame->GetSlotsOccupied())+"/"+UTIL_ToString(m_GHost->m_CurrentGame->m_Slots.size());
			sMsg = "Lobby: "+ m_GHost->m_CurrentGame->GetGameName()+" ("+slots+") - "+m_GHost->m_CurrentGame->GetOwnerName();
		} else
			if (m_GHost->m_Games.size()>0)
			{
				sMsg = m_GHost->m_Games[m_GHost->m_Games.size()-1]->GetGameInfo();
			}
		QueueChatCommand(sMsg, sUser, true);
	}
	for( vector<PairedGameUpdate> :: iterator i = m_PairedGameUpdates.begin( ); i != m_PairedGameUpdates.end( ); )
	{
	if( i->second->GetReady( ) )
	{
		string response = i->second->GetResult( );

			  QueueChatCommand( response, i->first, !i->first.empty( ) );
		m_GHost->m_DB->RecoverCallable( i->second );
		delete i->second;
		i = m_PairedGameUpdates.erase( i );
	}
	else
						  ++i;
	}

	// refresh the admin list every 5 minutes

	if( !m_CallableAdminList && GetTime( ) >= m_LastAdminRefreshTime + 300 )
	{
		m_CallableAdminList = m_GHost->m_DB->ThreadedAdminList( m_Server );
		m_CallableAccessesList = m_GHost->m_DB->ThreadedAccessesList( m_Server );
		m_CallableSafeList = m_GHost->m_DB->ThreadedSafeList( m_Server );
		m_CallableSafeListV = m_GHost->m_DB->ThreadedSafeListV( m_Server );
	}

	if( m_CallableAdminList && m_CallableAdminList->GetReady( ) )
	{
//		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed admin list (" + UTIL_ToString( m_Admins.size( ) ) + " -> " + UTIL_ToString( m_CallableAdminList->GetResult( ).size( ) ) + " admins)" );
		m_Admins = m_CallableAdminList->GetResult( );
		m_GHost->m_DB->RecoverCallable( m_CallableAdminList );
		delete m_CallableAdminList;
		m_CallableAdminList = NULL;
		m_LastAdminRefreshTime = GetTime( );
	}

	if( m_CallableAccessesList && m_CallableAccessesList->GetReady( ) )
	{
		// CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed accesses list (" + UTIL_ToString( m_Accesses.size( ) ) + " -> " + UTIL_ToString( m_CallableAccessesList->GetResult( ).size( ) ) + " admins)" );
		m_Accesses = m_CallableAccessesList->GetResult( );
		m_GHost->m_DB->RecoverCallable( m_CallableAccessesList );
		delete m_CallableAccessesList;
		m_CallableAccessesList = NULL;
	}

	if( m_CallableSafeList && m_CallableSafeList->GetReady( ) )
	{
		// CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed safe list (" + UTIL_ToString( m_Safe.size( ) ) + " -> " + UTIL_ToString( m_CallableSafeList->GetResult( ).size( ) ) + " safe listed)" );
		m_Safe = m_CallableSafeList->GetResult( );
		m_GHost->m_DB->RecoverCallable( m_CallableSafeList );
		delete m_CallableSafeList;
		m_CallableSafeList = NULL;
	}

	if( m_CallableSafeListV && m_CallableSafeListV->GetReady( ) )
	{
		// CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed safe list vouchers (" + UTIL_ToString( m_Safe.size( ) ) + " -> " + UTIL_ToString( m_CallableSafeList->GetResult( ).size( ) ) + " safe listed)" );
		m_SafeVouchers = m_CallableSafeListV->GetResult( );
		m_GHost->m_DB->RecoverCallable( m_CallableSafeListV );
		delete m_CallableSafeListV;
		m_CallableSafeListV = NULL;
	}

	if( m_CallableNotes && m_CallableNotes->GetReady( ) )
	{
		// CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed notes names (" + UTIL_ToString( m_Safe.size( ) ) + " -> " + UTIL_ToString( m_CallableSafeList->GetResult( ).size( ) ) + " safe listed)" );
		m_Notes = m_CallableNotes->GetResult( );
		m_GHost->m_DB->RecoverCallable( m_CallableNotes );
		delete m_CallableNotes;
		m_CallableNotes = NULL;
	}

	if( m_CallableNotesN && m_CallableNotesN->GetReady( ) )
	{
		// CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed notes (" + UTIL_ToString( m_Safe.size( ) ) + " -> " + UTIL_ToString( m_CallableSafeList->GetResult( ).size( ) ) + " safe listed)" );
		m_NotesN = m_CallableNotesN->GetResult( );
		m_GHost->m_DB->RecoverCallable( m_CallableNotesN );
		delete m_CallableNotesN;
		m_CallableNotesN = NULL;
	}

	// refresh today's game count

	if (!m_CallableTodayGamesCount && GetTime( ) >= m_LastGameCountRefreshTime + 3600)
		m_CallableTodayGamesCount = m_GHost->m_DB->ThreadedTodayGamesCount();

	if (m_CallableTodayGamesCount && m_CallableTodayGamesCount->GetReady())
	{
		m_TodayGamesCount = m_CallableTodayGamesCount->GetResult( );
		m_LastGameCountRefreshTime  = GetTime();
		m_GHost->m_DB->RecoverCallable( m_CallableTodayGamesCount );
		delete m_CallableTodayGamesCount;
		m_CallableTodayGamesCount = NULL;
	}

	// check download file thread
	if( m_GHost->m_CallableDownloadFile && m_GHost->m_CallableDownloadFile->GetReady( ) )
	{
		if (m_GHost->m_CallableDownloadFile->GetResult()>0)
		{
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] downloaded file " + m_GHost->m_CallableDownloadFile->GetFile());
			QueueChatCommand("File "+m_GHost->m_CallableDownloadFile->GetFile()+" downloaded OK in : "+m_GHost->m_CallableDownloadFile->GetPath(), m_DownloadFileUser, true);
		}
		else
		{
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] file download error: " + m_GHost->m_CallableDownloadFile->GetFile());
			QueueChatCommand(m_GHost->m_CallableDownloadFile->GetFile()+" didn't download OK!", m_DownloadFileUser, true);
		}
		delete m_GHost->m_CallableDownloadFile;
		m_GHost->m_CallableDownloadFile = NULL;
	}

	// refresh the ban list every 60 minutes

	if( !m_CallableBanList && GetTime( ) >= m_LastBanRefreshTime + 3600 )
		m_CallableBanList = m_GHost->m_DB->ThreadedBanList( m_Server );

	if( m_CallableBanList && m_CallableBanList->GetReady( ) )
	{
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed ban list (" + UTIL_ToString( m_Bans.size( ) ) + " -> " + UTIL_ToString( m_CallableBanList->GetResult( ).size( ) ) + " bans)" );

		for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); i++ )
			delete *i;

		m_Bans = m_CallableBanList->GetResult( );
		UpdateMap();
		m_GHost->m_DB->RecoverCallable( m_CallableBanList );
		delete m_CallableBanList;
		m_CallableBanList = NULL;
		m_LastBanRefreshTime = GetTime( );
	}

	// we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
	// that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
	// but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

	if( m_Socket->HasError( ) )
	{
		// the socket has an error

		uint32_t timetowait = 90;
		if (m_PasswordHashType == "pvpgn")
			timetowait = 30;

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] disconnected from battle.net due to socket error" );

		if( m_Socket->GetError( ) == ECONNRESET && GetTime( ) - m_LastConnectionAttemptTime <= 15 )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - you are probably temporarily IP banned from battle.net" );

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting "+UTIL_ToString(timetowait)+" seconds to reconnect" );
		m_GHost->EventBNETDisconnected( this );
		delete m_BNLSClient;
		m_BNLSClient = NULL;
		m_BNCSUtil->Reset( m_UserName, m_UserPassword );
		m_Socket->Reset( );
		m_LastDisconnectedTime = GetTime( );
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) && !m_WaitingToConnect )
	{
		// the socket was disconnected

		uint32_t timetowait = 90;
		if (m_PasswordHashType == "pvpgn")
			timetowait = 30;

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] disconnected from battle.net" );
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting "+UTIL_ToString(timetowait)+" seconds to reconnect" );
		m_GHost->EventBNETDisconnected( this );
		delete m_BNLSClient;
		m_BNLSClient = NULL;
		m_BNCSUtil->Reset( m_UserName, m_UserPassword );
		m_Socket->Reset( );
		m_LastDisconnectedTime = GetTime( );
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if( m_Socket->GetConnected( ) )
	{
		// the socket is connected and everything appears to be working properly

		m_Socket->DoRecv( (fd_set *)fd );
		ExtractPackets( );
		ProcessPackets( );

		// update the BNLS client

		if( m_BNLSClient )
		{
			if( m_BNLSClient->Update( fd, send_fd ) )
			{
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] deleting BNLS client" );
				delete m_BNLSClient;
				m_BNLSClient = NULL;
			}
			else
			{
				BYTEARRAY WardenResponse = m_BNLSClient->GetWardenResponse( );

				if( !WardenResponse.empty( ) )
					m_Socket->PutBytes( m_Protocol->SEND_SID_WARDEN( WardenResponse ) );
			}
		}

		// check if at least one packet is waiting to be sent and if we've waited long enough to prevent flooding
		// this formula has changed many times but currently we wait 1 second if the last packet was "small", 3.5 seconds if it was "medium", and 4 seconds if it was "big"

		uint32_t WaitTicks = 0;

		uint32_t medium = UTIL_ToUInt32(m_GHost->m_bnetpacketdelaymedium);
		uint32_t big = UTIL_ToUInt32(m_GHost->m_bnetpacketdelaybig);

		if (m_PasswordHashType == "pvpgn")
		{
			medium = UTIL_ToUInt32(m_GHost->m_bnetpacketdelaymediumpvpgn);
			big = UTIL_ToUInt32(m_GHost->m_bnetpacketdelaybigpvpgn);
		}

		if( m_LastOutPacketSize < 10 )
			WaitTicks = 1000;
		else if( m_LastOutPacketSize < 100 )
			WaitTicks = medium;
		else
			WaitTicks = big;

		if( !m_OutPackets.empty( ) && GetTicks( ) - m_LastOutPacketTicks >= WaitTicks )
		{
			if( m_OutPackets.size( ) > 7 )
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] packet queue warning - there are " + UTIL_ToString( m_OutPackets.size( ) ) + " packets waiting to be sent" );

			m_Socket->PutBytes( m_OutPackets.front( ) );
			m_LastOutPacketSize = m_OutPackets.front( ).size( );
			m_OutPackets.pop( );
			m_LastOutPacketTicks = GetTicks( );
		}

	// send immediate commands
		if( !m_OutPackets2.empty( ) && GetTicks( ) - m_LastOutPacketTicks2 >= 45 )
		{
			uint32_t y = m_OutPackets2.size()-1;
			if (y>2)
				y = 2;
			for (uint32_t i=0;i<=y;i++)
			{
				m_Socket->PutBytes( m_OutPackets2.front( ) );
				m_OutPackets2.pop( );
			}
			m_LastOutPacketTicks2 = GetTicks( );
		}

		// send a null packet every 60 seconds to detect disconnects

		if( GetTime( ) - m_LastNullTime >= 60 && GetTicks( ) - m_LastOutPacketTicks >= 60000 )
		{
			m_Socket->PutBytes( m_Protocol->SEND_SID_NULL( ) );
			m_LastNullTime = GetTime( );
		}

		m_Socket->DoSend( (fd_set *)send_fd );
		return m_Exiting;
	}

	uint32_t timetowait = 90;
	if (m_PasswordHashType == "pvpgn")
		timetowait = 30;

	if( m_Socket->GetConnecting( ) )
	{
		// we are currently attempting to connect to battle.net

		if( m_Socket->CheckConnect( ) )
		{
			// the connection attempt completed

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connected" );
			m_GHost->EventBNETConnected( this );
			m_Socket->PutBytes( m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR( ) );
			m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_INFO( m_War3Version, m_GHost->m_TFT, m_LocaleID, m_CountryAbbrev, m_Country ) );
			m_Socket->DoSend( (fd_set *)send_fd );
			m_LastNullTime = GetTime( );
			m_LastOutPacketTicks = GetTicks( );
			m_LastOutPacketTicks2 = GetTicks( );

			while( !m_OutPackets.empty( ) )
				m_OutPackets.pop( );
			m_LastChatCommandTime = GetTime( );
			return m_Exiting;
		}
		else if( GetTime( ) - m_LastConnectionAttemptTime >= 15 )
		{
			// the connection attempt timed out (15 seconds)

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connect timed out" );
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting "+UTIL_ToString(timetowait)+" seconds to reconnect" );
			m_GHost->EventBNETConnectTimedOut( this );
			m_Socket->Reset( );
			m_LastDisconnectedTime = GetTime( );
			m_WaitingToConnect = true;
			return m_Exiting;
		}
	}

	if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) && ( m_FirstConnect || GetTime( ) - m_LastDisconnectedTime >= timetowait ) )
	{
		// attempt to connect to battle.net

		m_FirstConnect = false;
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connecting to server [" + m_Server + "] on port 6112" );
		m_GHost->EventBNETConnecting( this );

		if( !m_GHost->m_BindAddress.empty( ) )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to bind to address [" + m_GHost->m_BindAddress + "]" );

		if( m_ServerIP.empty( ) )
		{
			m_Socket->Connect( m_GHost->m_BindAddress, m_Server, 6112 );

			if( !m_Socket->HasError( ) )
			{
				m_ServerIP = m_Socket->GetIPString( );
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] resolved and cached server IP address " + m_ServerIP );
			}
		}
		else
		{
			// use cached server IP address since resolving takes time and is blocking

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using cached server IP address " + m_ServerIP );
			m_Socket->Connect( m_GHost->m_BindAddress, m_ServerIP, 6112 );
		}

		m_WaitingToConnect = false;
		m_LastConnectionAttemptTime = GetTime( );
		return m_Exiting;
	}

	if (m_GHost->m_NewChannel)
		if (GetTime() - m_GHost->m_ChannelJoinTime>3)
		{
			m_GHost->UDPChatSend("|channeljoined "+m_CurrentChannel);
			m_GHost->m_NewChannel=false;
		}

	return m_Exiting;
}

void CBNET :: ExtractPackets( )
{
	// extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

	string *RecvBuffer = m_Socket->GetBytes( );
	BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while( Bytes.size( ) >= 4 )
	{
		// byte 0 is always 255

		if( Bytes[0] == BNET_HEADER_CONSTANT )
		{
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

			if( Length >= 4 )
			{
				if( Bytes.size( ) >= Length )
				{
					m_Packets.push( new CCommandPacket( BNET_HEADER_CONSTANT, Bytes[1], BYTEARRAY( Bytes.begin( ), Bytes.begin( ) + Length ) ) );
					*RecvBuffer = RecvBuffer->substr( Length );
					Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
				}
				else
					return;
			}
			else
			{
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error - received invalid packet from battle.net (bad length), disconnecting" );
				m_Socket->Disconnect( );
				return;
			}
		}
		else
		{
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error - received invalid packet from battle.net (bad header constant), disconnecting" );
			m_Socket->Disconnect( );
			return;
		}
	}
}

void CBNET :: ProcessPackets( )
{
	CIncomingGameHost *GameHost = NULL;
	CIncomingChatEvent *ChatEvent = NULL;
	BYTEARRAY WardenData;
	vector<CIncomingFriendList *> Friends;
	vector<CIncomingClanList *> Clans;

	// process all the received packets in the m_Packets queue
	// this normally means sending some kind of response

	while( !m_Packets.empty( ) )
	{
		CCommandPacket *Packet = m_Packets.front( );
		m_Packets.pop( );

		if( Packet->GetPacketType( ) == BNET_HEADER_CONSTANT )
		{
			switch( Packet->GetID( ) )
			{
			case CBNETProtocol :: SID_NULL:
				// warning: we do not respond to NULL packets with a NULL packet of our own
				// this is because PVPGN servers are programmed to respond to NULL packets so it will create a vicious cycle of useless traffic
				// official battle.net servers do not respond to NULL packets

				m_Protocol->RECEIVE_SID_NULL( Packet->GetData( ) );
				break;

			case CBNETProtocol :: SID_GETADVLISTEX:
				GameHost = m_Protocol->RECEIVE_SID_GETADVLISTEX( Packet->GetData( ) );

				if( GameHost )
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joining game [" + GameHost->GetGameName( ) + "]" );

				delete GameHost;
				GameHost = NULL;
				break;

			case CBNETProtocol :: SID_ENTERCHAT:
				if( m_Protocol->RECEIVE_SID_ENTERCHAT( Packet->GetData( ) ) )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joining channel [" + m_FirstChannel + "]" );
					m_InChat = true;
					m_Socket->PutBytes( m_Protocol->SEND_SID_JOINCHANNEL( m_FirstChannel ) );
				}

				break;

			case CBNETProtocol :: SID_CHATEVENT:
				ChatEvent = m_Protocol->RECEIVE_SID_CHATEVENT( Packet->GetData( ) );

				if( ChatEvent )
					ProcessChatEvent( ChatEvent );

				delete ChatEvent;
				ChatEvent = NULL;
				break;

			case CBNETProtocol :: SID_CHECKAD:
				m_Protocol->RECEIVE_SID_CHECKAD( Packet->GetData( ) );
				break;

			case CBNETProtocol :: SID_STARTADVEX3:
				if( m_Protocol->RECEIVE_SID_STARTADVEX3( Packet->GetData( ) ) )
				{
					m_InChat = false;					
					m_GHost->EventBNETGameRefreshed( this );
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] startadvex3 failed" );
					m_GHost->EventBNETGameRefreshFailed( this );
				}

				break;

			case CBNETProtocol :: SID_PING:
				m_Socket->PutBytes( m_Protocol->SEND_SID_PING( m_Protocol->RECEIVE_SID_PING( Packet->GetData( ) ) ) );
				break;

			case CBNETProtocol :: SID_AUTH_INFO:
				if( m_Protocol->RECEIVE_SID_AUTH_INFO( Packet->GetData( ) ) )
				{
					if( m_BNCSUtil->HELP_SID_AUTH_CHECK( m_GHost->m_TFT, m_GHost->m_Warcraft3Path, m_CDKeyROC, m_CDKeyTFT, m_Protocol->GetValueStringFormulaString( ), m_Protocol->GetIX86VerFileNameString( ), m_Protocol->GetClientToken( ), m_Protocol->GetServerToken( ) ) )
					{
						// override the exe information generated by bncsutil if specified in the config file
						// apparently this is useful for pvpgn users

						if( m_EXEVersion.size( ) == 4 )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using custom exe version bnet_custom_exeversion = " + UTIL_ToString( m_EXEVersion[0] ) + " " + UTIL_ToString( m_EXEVersion[1] ) + " " + UTIL_ToString( m_EXEVersion[2] ) + " " + UTIL_ToString( m_EXEVersion[3] ) );
							m_BNCSUtil->SetEXEVersion( m_EXEVersion );
						}

						if( m_EXEVersionHash.size( ) == 4 )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using custom exe version hash bnet_custom_exeversionhash = " + UTIL_ToString( m_EXEVersionHash[0] ) + " " + UTIL_ToString( m_EXEVersionHash[1] ) + " " + UTIL_ToString( m_EXEVersionHash[2] ) + " " + UTIL_ToString( m_EXEVersionHash[3] ) );
							m_BNCSUtil->SetEXEVersionHash( m_EXEVersionHash );
						}

						if( m_GHost->m_TFT )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to auth as Warcraft III: The Frozen Throne" );
						else
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to auth as Warcraft III: Reign of Chaos" );							

						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_CHECK( m_GHost->m_TFT, m_Protocol->GetClientToken( ), m_BNCSUtil->GetEXEVersion( ), m_BNCSUtil->GetEXEVersionHash( ), m_BNCSUtil->GetKeyInfoROC( ), m_BNCSUtil->GetKeyInfoTFT( ), m_BNCSUtil->GetEXEInfo( ), "GHost" ) );

						// the Warden seed is the first 4 bytes of the ROC key hash
						// initialize the Warden handler

						if( !m_BNLSServer.empty( ) )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] creating BNLS client" );
							delete m_BNLSClient;
							m_BNLSClient = new CBNLSClient( m_BNLSServer, m_BNLSPort, m_BNLSWardenCookie );
							m_BNLSClient->QueueWardenSeed( UTIL_ByteArrayToUInt32( m_BNCSUtil->GetKeyInfoROC( ), false, 16 ) );
						}
					}
					else
					{
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - bncsutil key hash failed (check your Warcraft 3 path and cd keys), disconnecting" );
						m_Socket->Disconnect( );
						delete Packet;
						return;
					}
				}

				break;

			case CBNETProtocol :: SID_AUTH_CHECK:
				if( m_Protocol->RECEIVE_SID_AUTH_CHECK( Packet->GetData( ) ) )
				{
					// cd keys accepted

					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] cd keys accepted" );
					m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON( );
					m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON( m_BNCSUtil->GetClientKey( ), m_UserName ) );
				}
				else
				{
					// cd keys not accepted

					switch( UTIL_ByteArrayToUInt32( m_Protocol->GetKeyState( ), false ) )
					{
					case CBNETProtocol :: KR_ROC_KEY_IN_USE:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription( ) + "], disconnecting" );
						break;
					case CBNETProtocol :: KR_TFT_KEY_IN_USE:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription( ) + "], disconnecting" );
						break;
					case CBNETProtocol :: KR_OLD_GAME_VERSION:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - game version is too old, disconnecting" );
						break;
					case CBNETProtocol :: KR_INVALID_VERSION:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - game version is invalid, disconnecting" );
						break;
					default:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - cd keys not accepted, disconnecting" );
						break;
					}

					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_AUTH_ACCOUNTLOGON:
				if( m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGON( Packet->GetData( ) ) )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] username [" + m_UserName + "] accepted" );

					if( m_PasswordHashType == "pvpgn" )
					{
						// pvpgn logon

						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using pvpgn logon type (for pvpgn servers only)" );
						m_BNCSUtil->HELP_PvPGNPasswordHash( m_UserPassword );
						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF( m_BNCSUtil->GetPvPGNPasswordHash( ) ) );
					}
					else
					{
						// battle.net logon

						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using battle.net logon type (for official battle.net servers only)" );
						m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF( m_Protocol->GetSalt( ), m_Protocol->GetServerPublicKey( ) );
						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF( m_BNCSUtil->GetM1( ) ) );
					}
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - invalid username, disconnecting" );
					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_AUTH_ACCOUNTLOGONPROOF:
				if( m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF( Packet->GetData( ) ) )
				{
					// logon successful

					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon successful" );
					m_LoggedIn = true;
					m_GHost->EventBNETLoggedIn( this );
					m_Socket->PutBytes( m_Protocol->SEND_SID_NETGAMEPORT( m_GHost->m_HostPort ) );
					m_Socket->PutBytes( m_Protocol->SEND_SID_ENTERCHAT( ) );
					m_Socket->PutBytes( m_Protocol->SEND_SID_FRIENDSLIST( ) );
					m_Socket->PutBytes( m_Protocol->SEND_SID_CLANMEMBERLIST( ) );
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - invalid password, disconnecting" );

					// try to figure out if the user might be using the wrong logon type since too many people are confused by this

					string Server = m_Server;
					transform( Server.begin( ), Server.end( ), Server.begin( ), (int(*)(int))tolower );

					if( m_PasswordHashType == "pvpgn" && ( Server == "useast.battle.net" || Server == "uswest.battle.net" || Server == "asia.battle.net" || Server == "europe.battle.net" ) )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] it looks like you're trying to connect to a battle.net server using a pvpgn logon type, check your config file's \"battle.net custom data\" section" );
					else if( m_PasswordHashType != "pvpgn" && ( Server != "useast.battle.net" && Server != "uswest.battle.net" && Server != "asia.battle.net" && Server != "europe.battle.net" ) )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] it looks like you're trying to connect to a pvpgn server using a battle.net logon type, check your config file's \"battle.net custom data\" section" );

					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_WARDEN:
				WardenData = m_Protocol->RECEIVE_SID_WARDEN( Packet->GetData( ) );

				if( m_BNLSClient )
					m_BNLSClient->QueueWardenRaw( WardenData );
				else
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - received warden packet but no BNLS server is available, you will be kicked from battle.net soon" );

				break;

			case CBNETProtocol :: SID_FRIENDSLIST:
				Friends = m_Protocol->RECEIVE_SID_FRIENDSLIST( Packet->GetData( ) );

				for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); i++ )
					delete *i;

				m_Friends = Friends;

				m_GHost->UDPChatSend("|newfriends");

				/* DEBUG_Print( "received " + UTIL_ToString( Friends.size( ) ) + " friends" );
				for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); i++ )
					DEBUG_Print( "friend: " + (*i)->GetAccount( ) ); */

				break;

			case CBNETProtocol :: SID_CLANMEMBERLIST:
				vector<CIncomingClanList *> Clans = m_Protocol->RECEIVE_SID_CLANMEMBERLIST( Packet->GetData( ) );

				for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
					delete *i;

				m_Clans = Clans;

				m_ClanRecruits.erase( m_ClanRecruits.begin( ), m_ClanRecruits.end( ) );
				m_ClanPeons.erase( m_ClanPeons.begin( ), m_ClanPeons.end( ) );
				m_ClanGrunts.erase( m_ClanGrunts.begin( ), m_ClanGrunts.end( ) );
				m_ClanShamans.erase( m_ClanShamans.begin( ), m_ClanShamans.end( ) );
				m_ClanChieftains.erase( m_ClanChieftains.begin( ), m_ClanChieftains.end( ) );

				// sort by clan ranks into appropriate vectors and status (if they are online)
				// ClanChieftains = string with names, used for -clanlist command I have while m_ClanChieftains is a vector used for IsClanChieftain( string user ).
				for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
				{
					// DEBUG_Print( "Clan member: " + (*i)->GetName( ) );
					string Name = (*i)->GetName( );
					transform( Name.begin( ), Name.end( ), Name.begin( ), (int(*)(int))tolower );

					if( (*i)->GetRank( ) == "Peon" )
					{								
//						ClanPeons = ClanPeons + (*i)->GetName( ) + ", ";	
						m_ClanPeons.push_back( string( Name ) );

					}				
					else if( (*i)->GetRank( ) == "Recruit" )
					{
//						ClanPeons = ClanPeons + (*i)->GetName( ) + ", ";
						m_ClanRecruits.push_back( string( Name ) );
					}
					else if( (*i)->GetRank( ) == "Grunt" )
					{
//						ClanGrunts = ClanGrunts + (*i)->GetName( ) + ", ";
						m_ClanGrunts.push_back( string( Name ) );
					}
					else if( (*i)->GetRank( ) == "Shaman" )
					{
//						ClanShamans = ClanShamans + (*i)->GetName( ) + ", ";
						m_ClanShamans.push_back( string( Name ) );
					}
					else if( (*i)->GetRank( ) == "Chieftain" )
					{
//						ClanChieftains = ClanChieftains + (*i)->GetName( ) + ", ";
						m_ClanChieftains.push_back( string( Name ) );
					}
					if( (*i)->GetStatus( ) == "Online" )
					{
//						if( (*i)->GetName( ) != "[tebr]host" && (*i)->GetName( ) != "[TeBR]BOT" )
						{
//							ClanOnline = ClanOnline + (*i)->GetName( ) + ", ";
//							ClanOnlineNumber = ClanOnlineNumber + 1;
						}

					}
					// getting a number of members in the clan
//					ClanMemberNumber = ClanMemberNumber + 1;
				}	

				/* DEBUG_Print( "received " + UTIL_ToString( Clans.size( ) ) + " clan members" );
				for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
					DEBUG_Print( "clan member: " + (*i)->GetName( ) ); */

				break;
			}
		}

		delete Packet;
	}
}

//
// Whisper or Channel Commands
//
void CBNET :: ProcessChatEvent( CIncomingChatEvent *chatEvent )
{
	CBNETProtocol :: IncomingChatEvent Event = chatEvent->GetChatEvent( );
	bool Whisper = ( Event == CBNETProtocol :: EID_WHISPER );
	string User = chatEvent->GetUser( );
	string Message = chatEvent->GetMessage( );

	if( Event == CBNETProtocol :: EID_WHISPER || Event == CBNETProtocol :: EID_TALK )
	{
		// send message to admin game
//		m_GHost->AdminGameMessage(User, Message);
		if( Event == CBNETProtocol :: EID_WHISPER )
		{
			CONSOLE_Print( "[WSPR: " + m_ServerAlias + "] [" + User + "] " + Message );
			m_GHost->EventBNETWhisper( this, User, Message );
			m_GHost->UDPChatSend("|Chatw "+UTIL_ToString(User.length())+" " +User+" "+Message);
			if (Message.find("Your friend") !=string :: npos)
				SendGetFriendsList();
			string :: size_type sPos = Message.find("has entered PvPGN Realm");
			string :: size_type sPoss = Message.find("has entered Battle.net");
			if (sPoss != string :: npos)
				sPos = sPoss;
			if (sPos != string :: npos)
			{
				string sName = Message.substr(12, sPos-12);
				m_FriendsEnteringBnet.push_back(sName);
				m_LastFriendEnteredWhisper = GetTime();
//				CONSOLE_Print( "[LOCAL: " + m_ServerAlias + "] [" + sName + "] " + "entered PvPGN" );
			}
		}
		else
		{
			CONSOLE_Print( "[LOCAL: " + m_ServerAlias + "] [" + User + "] " + Message );
			m_GHost->EventBNETChat( this, User, Message );
			m_GHost->UDPChatSend("|Chat "+UTIL_ToString(User.length())+" " +User+" "+Message);
		}

		// handle spoof checking for current game
		// this case covers whispers - we assume that anyone who sends a whisper to the bot with message "s" should be considered spoof checked
		// note that this means you can whisper "s" even in a public game to manually spoofcheck if the /whois fails

		if( Event == CBNETProtocol :: EID_WHISPER && m_GHost->m_CurrentGame )
		{
			if( Message == "s" || Message == "sc" || Message == "spoof" || Message == "check" || Message == "spoofcheck" || Message == "cm" || Message == "checkme" )
				m_GHost->m_CurrentGame->AddToSpoofed( m_Server, User, true );
			else if( Message.find( m_GHost->m_CurrentGame->GetGameName( ) ) != string :: npos )
			{
				// look for messages like "entered a Warcraft III The Frozen Throne game called XYZ"
				// we don't look for the English part of the text anymore because we want this to work with multiple languages
				// it's a pretty safe bet that anyone whispering the bot with a message containing the game name is a valid spoofcheck

				if( m_PasswordHashType == "pvpgn" && User == m_PVPGNRealmName )
				{
					// the equivalent pvpgn message is: [PvPGN Realm] Your friend abc has entered a Warcraft III Frozen Throne game named "xyz".

					vector<string> Tokens = UTIL_Tokenize( Message, ' ' );

					if( Tokens.size( ) >= 3 )
						m_GHost->m_CurrentGame->AddToSpoofed( m_Server, Tokens[2], false );
				}
				else
					m_GHost->m_CurrentGame->AddToSpoofed( m_Server, User, false );
			}
		}

		// handle bot commands

		if( Message == "?trigger" && ( IsAdmin( User ) || IsRootAdmin( User ) || ( m_PublicCommands && m_OutPackets.size( ) <= 3 ) ) )
			QueueChatCommand( m_GHost->m_Language->CommandTrigger( string( 1, m_CommandTrigger ) ), User, Whisper );
		else if( !Message.empty( ) && Message[0] == m_CommandTrigger )
		{
			// extract the command trigger, the command, and the payload
			// e.g. "!say hello world" -> command: "say", payload: "hello world"

			string Command;
			string Payload;
			string :: size_type PayloadStart = Message.find( " " );

			if( PayloadStart != string :: npos )
			{
				Command = Message.substr( 1, PayloadStart - 1 );
				Payload = Message.substr( PayloadStart + 1 );
			}
			else
				Command = Message.substr( 1 );

			transform( Command.begin( ), Command.end( ), Command.begin( ), (int(*)(int))tolower );

			uint32_t AdminAccess = 0;
			bool AdminCheck = IsAdmin(User);
			if (AdminCheck)
				AdminAccess = LastAccess();
			bool RootAdminCheck = IsRootAdmin( User );
			if (RootAdminCheck)
				AdminAccess = CMDAccessAll();

			if( AdminCheck || RootAdminCheck )
			{
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] admin [" + User + "] sent command [" + Message + "]" );

				/*****************
				* ADMIN COMMANDS *
				******************/

				//
				//
				//!dl
				//

				if(Command == "dl" && !Payload.empty()) 
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (m_GHost->m_CallableDownloadFile)
					{
						QueueChatCommand("Another download is already in progress, try again later!", User, Whisper);
						return;
					}
					stringstream SS;
					SS<<Payload;
					string Map;
					string Path;
					SS>>Map;
					if (SS.fail())
					{
						QueueChatCommand("dl <url> <path>", User, Whisper);
						return;
					}
					SS>>Path;
					if (SS.fail())
					{
						QueueChatCommand("dl <url> <path>", User, Whisper);
						return;
					}

					m_GHost->m_CallableDownloadFile = m_GHost->ThreadedDownloadFile(Map, Path);
					m_DownloadFileUser = User;					
					QueueChatCommand("Downloading file in "+Path+" ...", User, Whisper);
				}

				//
				//!dlmap
				//

				if(Command == "dlmap" && !Payload.empty()) 
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (m_GHost->m_CallableDownloadFile)
					{
						QueueChatCommand("Another download is already in progress, try again later!", User, Whisper);
						return;
					}

					m_GHost->m_CallableDownloadFile = m_GHost->ThreadedDownloadFile(Payload, m_GHost->m_MapPath);
					m_DownloadFileUser = User;					
					QueueChatCommand("Downloading map ...", User, Whisper);
				}

				//
				//!dlmapcfg
				//

				if(Command == "dlmapcfg" && !Payload.empty() ) 
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (m_GHost->m_CallableDownloadFile)
					{
						QueueChatCommand("Another download is already in progress, try again later!", User, Whisper);
						return;
					}

					m_GHost->m_CallableDownloadFile = m_GHost->ThreadedDownloadFile(Payload, m_GHost->m_MapCFGPath);
					m_DownloadFileUser = User;					
					QueueChatCommand("Downloading mapcfg ...", User, Whisper);
				}
				// !ADDADMIN
				// !AA
				//

				if( ( Command == "addadmin" || Command == "aa" ) && !Payload.empty( ) )
				{
					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(Payload);
					if (!pnam.empty())
						nam = pnam;
					if( RootAdminCheck )
					{
						string Usr;
						Usr = Whisper ? User : string( );
						if (m_GHost->m_WhisperAllMessages)
							Usr = User;
						if( IsAdmin( nam ) )
							QueueChatCommand( m_GHost->m_Language->UserIsAlreadyAnAdmin( m_Server, nam ), User, Whisper );
						else
							m_PairedAdminAdds.push_back( PairedAdminAdd( Usr, m_GHost->m_DB->ThreadedAdminAdd( m_Server, nam ) ) );
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				//
				// !N
				// !NOTE
				//

				if( ( Command == "n" || Command == "note" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
				{
					string srv = GetServer();

					string NName;
					string NNote;
					stringstream SS;
					SS << Payload;
					SS >> NName;
					string pnam = GetPlayerFromNamePartial(NName);
					if (!pnam.empty())
						NName = pnam;

					if( !SS.eof( ) )
					{
						getline( SS, NNote );
						string :: size_type Start = NNote.find_first_not_of( " " );

						if( Start != string :: npos )
							NNote = NNote.substr( Start );
					}

					bool noted = false;
					string note = Note(NName);
					noted = IsNoted( NName);

					if (NNote.empty())
					{
						if (noted)
							QueueChatCommand(m_GHost->m_Language->PlayerIsNoted(NName, note), User, Whisper);
						else
							QueueChatCommand(m_GHost->m_Language->PlayerIsNotNoted(NName), User, Whisper);
					} else
					{
						if (noted)
						{
							QueueChatCommand(m_GHost->m_Language->ChangedPlayerNote(NName), User, Whisper);
							m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedNoteUpdate(m_Server, NName, NNote));
							AddNote(NName, NNote);
						}
						else
						{
							QueueChatCommand(m_GHost->m_Language->AddedPlayerToNoteList(NName), User, Whisper);
							m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedNoteAdd(m_Server, NName, NNote));
							AddNote(NName, NNote);
						}
					}
				}

				//
				// !ND
				// !NR
				// !NOTEDEL
				// !NOTEREM
				//

				if( ( Command == "nd" || Command == "nr" || Command == "notedel" || Command == "noterem" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
				{
					string srv = GetServer();
					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(Payload);
					if (!pnam.empty())
						nam = pnam;

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					bool noted = false;
					noted = IsNoted( nam);
					if (!noted)
						QueueChatCommand(m_GHost->m_Language->PlayerIsNotNoted(nam), User, Whisper);
					else
					{
						QueueChatCommand(m_GHost->m_Language->RemovedPlayerFromNoteList(nam), User, Whisper);
						m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedNoteRemove(m_Server, nam));
					}
				}

				//
				// !SL
				//

				if( ( Command == "sl" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
				{
					string srv = GetServer();
					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(Payload);
					if (!pnam.empty())
						nam = pnam;
					bool safe = false;
					string v = Voucher(nam);
					safe = IsSafe( nam);

					if (safe)
						QueueChatCommand(m_GHost->m_Language->PlayerIsInTheSafeList(nam, v), User, Whisper);
					else
						QueueChatCommand(m_GHost->m_Language->PlayerIsNotInTheSafeList(nam), User, Whisper);
				}

				//
				// !SLADD
				// !SLA
				//

				if( ( Command == "sladd" || Command == "sla" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
				{
					string srv = GetServer();
					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(Payload);
					if (!pnam.empty())
						nam = pnam;

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					bool safe = false;
					string v = Voucher(nam);
					safe = IsSafe( nam);
					if (safe)
						QueueChatCommand(m_GHost->m_Language->PlayerIsInTheSafeList(nam, v), User, Whisper);
					else
						m_PairedSafeAdds.push_back( PairedSafeAdd( Usr, m_GHost->m_DB->ThreadedSafeAdd( m_Server, nam, User ) ) );
				}

				//
				// !SLDEL
				// !SLD
				// !SLR
				//

				if( ( Command == "sld" || Command == "slr" || Command == "sldel" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
				{
					string srv = GetServer();
					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(Payload);
					if (!pnam.empty())
						nam = pnam;

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					bool safe = false;
					safe = IsSafe( nam);
					if (!safe)
						QueueChatCommand(m_GHost->m_Language->PlayerIsNotInTheSafeList(nam), User, Whisper);
					else
						m_PairedSafeRemoves.push_back( PairedSafeRemove( Usr, m_GHost->m_DB->ThreadedSafeRemove( m_Server, nam ) ) );
				}


				//
				// !CD
				// !COUNTDOWN
				//

				if( ( Command == "cd" || Command == "countdown" )  )
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (!m_GHost->m_NormalCountdown)
					{
						QueueChatCommand("Normal countdown active !", User, Whisper);
						m_GHost->m_NormalCountdown = true;
					} else
					{
						QueueChatCommand("Ghost countdown active !", User, Whisper);
						m_GHost->m_NormalCountdown = false;
					}
				}

				//
				// !QUERY
				//

				if( ( Command == "query" || Command == "q" ) && !Payload.empty( ) )
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					QueueChatCommand("Running query...", User, Whisper);

					m_PairedRunQueries.push_back( PairedRunQuery(Usr, m_GHost->m_DB->ThreadedRunQuery(Payload)));
				}

				//
				// !ADDBAN
				// !BAN
				// !B
				//

				if (!m_GHost->m_ReplaceBanWithWarn)
				if( ( Command == "addban" || Command == "ban" || Command == "b" ) && !Payload.empty( ) )
				{
					// extract the victim and the reason
					// e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

					if (!CMDCheck(CMD_ban, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					string Victim;
					string Reason;
					stringstream SS;
					SS << Payload;
					SS >> Victim;
					string pnam = GetPlayerFromNamePartial(Victim);
					if (!pnam.empty())
						Victim = pnam;

					if( !SS.eof( ) )
					{
						getline( SS, Reason );
						string :: size_type Start = Reason.find_first_not_of( " " );

						if( Start != string :: npos )
							Reason = Reason.substr( Start );
					}

					if (IsAdmin(Victim) || IsRootAdmin(Victim))
					{
						QueueChatCommand( "You can't ban an admin!", User, Whisper);
						return;
					}
					if (IsSafe(Victim) && m_GHost->m_SafelistedBanImmunity)
					{
						QueueChatCommand( "You can't ban a safelisted player!", User, Whisper);
						return;
					}

					if (m_GHost->m_ReplaceBanWithWarn)
					{
						WarnPlayer(Victim, Reason, User, Whisper);
						return;
					}

					if( IsBannedName( Victim ) )
						QueueChatCommand( m_GHost->m_Language->UserIsAlreadyBanned( m_Server, Victim ), User, Whisper );
					else
					{
						string Usr;
						Usr = Whisper ? User : string( );
						if (m_GHost->m_WhisperAllMessages)
							Usr = User;
						uint32_t BanTime = m_GHost->m_BanTime;
						m_PairedBanAdds.push_back( PairedBanAdd( Usr, m_GHost->m_DB->ThreadedBanAdd( m_Server, Victim, string( ), string( ), User, Reason, BanTime, 0 ) ) );
					}
				}

				//
				// !UNBAND
				// !DELBAND
				// !DELBD
				//

				if (Command == "unband" || Command == "delband" || Command == "delbd")
				{
					uint32_t Day = 0;
					if (!Payload.empty())
						Day = UTIL_ToUInt32(Payload);
					m_RemoveBansList = m_GHost->m_DB->RemoveBanListDay( m_Server, Day);
					uint32_t Count = m_RemoveBansList.size();
					string mess = "Removed "+ UTIL_ToString(Count) +" bans which were given "+UTIL_ToString(Day)+" days before";
					ImmediateChatCommand("/w "+ User+" "+mess);
					CONSOLE_Print("[BNET: " + m_ServerAlias + "] "+mess);
					m_LastBanRemoveTime = GetTicks();
				}

				//
				// !TBR
				//

				if (Command == "tbr")
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
						return;
					}
					m_RemoveTempBansUser = User;
					m_RemoveTempBans = true;
					m_LastTempBansRemoveTime = GetTicks();
					QueueChatCommand("Removing temporary bans (if any) ...", User, Whisper);
				}

				//
				// !TEMPBAN
				// !TBAN
				// !TB
				//

				if( ( Command == "tempban" || Command == "tban" || Command == "tb" ) && !Payload.empty( ) )
				{
					// extract the victim and the reason
					// e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

					if (!CMDCheck(CMD_ban, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					string Victim;
					string Reason;
					uint32_t BanTime = 0;
					bool BanInit = false;
					stringstream SS;
					SS << Payload;
					SS >> Victim;

					string pnam = GetPlayerFromNamePartial(Victim);
					if (!pnam.empty())
						Victim = pnam;

					if( !SS.eof( ) )
					{
						getline( SS, Reason );
						string :: size_type Start = Reason.find_first_not_of( " " );

						if( Start != string :: npos )
						{
							Reason = Reason.substr( Start );

							string :: size_type BreakPoint = Reason.find_first_not_of( "0123456789" );

							if( BreakPoint != string :: npos )
							{
								BanInit = true;
								string Reas = Reason.substr(0, BreakPoint);
								BanTime = UTIL_ToUInt32( Reas );

								if( BreakPoint != string :: npos )
									Reason = Reason.substr(BreakPoint);

								Start = Reason.find_first_not_of( " " );
								if( Start != string :: npos )
									Reason = Reason.substr( Start );
							}
						}
					}

					if(!BanInit)
					{
						// failed to read the ban time, inform the player the syntax was incorrect
						QueueChatCommand( m_GHost->m_Language->IncorrectCommandSyntax( ), User, Whisper );
						return;
					}

					if( IsBannedName( Victim ) )
						QueueChatCommand( m_GHost->m_Language->UserIsAlreadyBanned( m_Server, Victim ), User, Whisper );
					else
					{
						if (IsAdmin(Victim) || IsRootAdmin(Victim))
						{
							QueueChatCommand( "You can't ban an admin!", User, Whisper);
							return;
						}
						if (IsSafe(Victim) && m_GHost->m_SafelistedBanImmunity)
						{
							QueueChatCommand( "You can't ban a safelisted player!", User, Whisper);
							return;
						}
						string Usr;
						Usr = Whisper ? User : string( );
						if (m_GHost->m_WhisperAllMessages)
							Usr = User;

						m_PairedBanAdds.push_back( PairedBanAdd( Usr, m_GHost->m_DB->ThreadedBanAdd( m_Server, Victim, string( ), string( ), User, Reason, BanTime, 0 ) ) );
					}
				}

				//
				// !ADDWARN
				// !WARN
				//

				if( ( (Command == "addwarn" || Command == "warn" ) || (m_GHost->m_ReplaceBanWithWarn && (Command == "ban" || Command == "addban" || Command == "b")) ) && !Payload.empty( ) )
				{
					// extract the victim and the reason
					// e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

					if (!CMDCheck(CMD_ban, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					string Victim;
					string Reason;
					uint32_t BanTime = 0;
					stringstream SS;
					SS << Payload;
					SS >> Victim;

					string pnam = GetPlayerFromNamePartial(Victim);
					if (!pnam.empty())
						Victim = pnam;

					if( !SS.eof( ) )
					{
						getline( SS, Reason );
						string :: size_type Start = Reason.find_first_not_of( " " );

						if( Start != string :: npos )
							Reason = Reason.substr( Start );
					}

					if (IsAdmin(Victim) || IsRootAdmin(Victim))
					{
						QueueChatCommand( "You can't warn an admin!", User, Whisper);
						return;
					}

					if (IsSafe(Victim) && m_GHost->m_SafelistedBanImmunity)
					{
						QueueChatCommand( "You can't warn a safelisted player!", User, Whisper);
						return;
					}

					WarnPlayer(Victim, Reason, User, Whisper);
				}

				//
				// !DELWARN
				// !UNWARN
				// !DW
				// !UW
				//

				if( ( Command == "delwarn" || Command == "unwarn" || Command =="dw" || Command == "uw" ) && !Payload.empty( ) )
				{
					if (!CMDCheck(CMD_delban, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(nam);
					if (!pnam.empty())
						nam = pnam;

					if( m_GHost->m_DB->BanRemove( nam, 1 ) )
						QueueChatCommand( m_GHost->m_Language->UnwarnedUser( nam ), User, Whisper );
					else
						QueueChatCommand( m_GHost->m_Language->ErrorUnwarningUser( nam ), User, Whisper );
				}

				//
				// !ANNOUNCE
				// !ANN
				//

				if( ( Command == "announce" || Command == "ann" ) && m_GHost->m_CurrentGame && !m_GHost->m_CurrentGame->GetCountDownStarted( ) )
				{
					if( Payload.empty( ) || Payload == "off" )
					{
						QueueChatCommand( m_GHost->m_Language->AnnounceMessageDisabled( ), User, Whisper );
						m_GHost->m_CurrentGame->SetAnnounce( 0, string( ) );
					}
					else
					{
						// extract the interval and the message
						// e.g. "30 hello everyone" -> interval: "30", message: "hello everyone"

						uint32_t Interval;
						string Message;
						stringstream SS;
						SS << Payload;
						SS >> Interval;

						if( SS.fail( ) || Interval == 0 )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to announce command" );
						else
						{
							if( SS.eof( ) )
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #2 to announce command" );
							else
							{
								getline( SS, Message );
								string :: size_type Start = Message.find_first_not_of( " " );

								if( Start != string :: npos )
									Message = Message.substr( Start );

								QueueChatCommand( m_GHost->m_Language->AnnounceMessageEnabled( ), User, Whisper );
								m_GHost->m_CurrentGame->SetAnnounce( Interval, Message );
							}
						}
					}
				}



				//
				// !AUTOHOSTxxxx
				//

				if( Command.length()>=10 && Command.substr(0,8)=="autohost")
				{
					if( IsRootAdmin( User ) )
					{
						if( Payload.empty( ) || Payload == "off" )
						{
							QueueChatCommand( m_GHost->m_Language->AutoHostDisabled( ), User, Whisper );
							m_GHost->m_AutoHostGameName.clear( );
							m_GHost->m_AutoHostMapCFG.clear( );
							m_GHost->m_AutoHostCountries = string();
							m_GHost->m_AutoHostCountries2 = string();
							m_GHost->m_AutoHostCountryCheck = false;
							m_GHost->m_AutoHostCountryCheck2 = false;
							m_GHost->m_AutoHostMaximumGames = 0;
							m_GHost->m_AutoHostAutoStartPlayers = 0;
							m_GHost->m_LastAutoHostTime = GetTime( );
						}
						else
						{
							// extract the maximum games, auto start players, and the game name
							// e.g. "5 10 BattleShips Pro" -> maximum games: "5", auto start players: "10", game name: "BattleShips Pro"

							int k = Command.length()-8;
							string from="";
							if (k % 2 != 0)
							{
								QueueChatCommand( "Syntax error, use for ex: !autohostroes" , User, Whisper);
								return;
							}
							k = k / 2;

							for (int i=1; i<k+1; i++)
							{
								if (i!=1)
									from=from+" "+Command.substr(8+(i-1)*2,2);
								else
									from=Command.substr(8+(i-1)*2,2);
							}
							transform( from.begin( ), from.end( ), from.begin( ), (int(*)(int))toupper );

							uint32_t MaximumGames;
							uint32_t AutoStartPlayers;
							string GameName;
							stringstream SS;
							SS << Payload;
							SS >> MaximumGames;

							if( SS.fail( ) || MaximumGames == 0 )
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to autohost command" );
							else
							{
								SS >> AutoStartPlayers;

								if( SS.fail( ) || AutoStartPlayers == 0 )
									CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to autohost command" );
								else
								{
									if( SS.eof( ) )
										CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #3 to autohost command" );
									else
									{
										getline( SS, GameName );
										string :: size_type Start = GameName.find_first_not_of( " " );

										if( Start != string :: npos )
											GameName = GameName.substr( Start );

										QueueChatCommand( m_GHost->m_Language->AutoHostEnabled( ), User, Whisper );
										m_GHost->ClearAutoHostMap( );
										m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
										m_GHost->m_AutoHostMapCounter = 0;
										m_GHost->m_AutoHostGameName = GameName;
										m_GHost->m_AutoHostCountries = from+" ??";
										m_GHost->m_AutoHostCountryCheck = true;
//										m_GHost->m_AutoHostMapCFG = m_GHost->m_Map->GetCFGFile( );
										m_GHost->m_AutoHostMaximumGames = MaximumGames;
										m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
										m_GHost->m_LastAutoHostTime = GetTime( );
									}
								}
							}
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !AUTOHOST
				// !AH
				//

				if( Command == "autohost" || Command == "ah" )
				{
					if( IsRootAdmin( User ) )
					{
						if( Payload.empty( ) || Payload == "off" )
						{
							QueueChatCommand( m_GHost->m_Language->AutoHostDisabled( ), User, Whisper );
							m_GHost->m_AutoHostGameName.clear( );
							m_GHost->m_AutoHostMapCFG.clear( );
							m_GHost->m_AutoHostOwner.clear( );
							m_GHost->m_AutoHostServer.clear( );
							m_GHost->m_AutoHostMaximumGames = 0;
							m_GHost->m_AutoHostAutoStartPlayers = 0;
							m_GHost->m_LastAutoHostTime = GetTime( );
							m_GHost->m_AutoHostMatchMaking = false;
							m_GHost->m_AutoHostMinimumScore = 0.0;
							m_GHost->m_AutoHostMaximumScore = 0.0;
//							m_GHost->m_AutoHostCountryCheck = false;
							m_GHost->m_AutoHostGArena = false;
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to autohost command" );
							else
							{
								SS >> AutoStartPlayers;

								if( SS.fail( ) || AutoStartPlayers == 0 )
									CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to autohost command" );
								else
								{
									if( SS.eof( ) )
										CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #3 to autohost command" );
									else
									{
										getline( SS, GameName );
										string :: size_type Start = GameName.find_first_not_of( " " );

										if( Start != string :: npos )
											GameName = GameName.substr( Start );

										QueueChatCommand( m_GHost->m_Language->AutoHostEnabled( ), User, Whisper );
										m_GHost->ClearAutoHostMap( );
										m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
										m_GHost->m_AutoHostMapCounter = 0;
										m_GHost->m_AutoHostGameName = GameName;
//										m_GHost->m_AutoHostMapCFG = m_GHost->m_Map->GetCFGFile( );
										m_GHost->m_AutoHostOwner = User;
										m_GHost->m_AutoHostServer = m_Server;
										m_GHost->m_AutoHostMaximumGames = MaximumGames;
										m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
										m_GHost->m_LastAutoHostTime = GetTime( );
										m_GHost->m_AutoHostMatchMaking = false;
										m_GHost->m_AutoHostMinimumScore = 0.0;
										m_GHost->m_AutoHostMaximumScore = 0.0;
//										m_GHost->m_AutoHostCountryCheck = false;
										m_GHost->m_AutoHostGArena = false;
									}
								}
							}
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !AUTOHOSTMM
				// !AHMM
				//

				if( Command == "autohostmm" || Command == "ahmm" )
				{
					if( IsRootAdmin( User ) )
					{
						if( Payload.empty( ) || Payload == "off" )
						{
							QueueChatCommand( m_GHost->m_Language->AutoHostDisabled( ), User, Whisper );
							m_GHost->m_AutoHostGameName.clear( );
							m_GHost->m_AutoHostMapCFG.clear( );
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
							// extract the maximum games, auto start players, minimum score, maximum score, and the game name
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to autohostmm command" );
							else
							{
								SS >> AutoStartPlayers;

								if( SS.fail( ) || AutoStartPlayers == 0 )
									CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to autohostmm command" );
								else
								{
									SS >> MinimumScore;

									if( SS.fail( ) )
										CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #3 to autohostmm command" );
									else
									{
										SS >> MaximumScore;

										if( SS.fail( ) )
											CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #4 to autohostmm command" );
										else
										{
											if( SS.eof( ) )
												CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #5 to autohostmm command" );
											else
											{
												getline( SS, GameName );
												string :: size_type Start = GameName.find_first_not_of( " " );

												if( Start != string :: npos )
													GameName = GameName.substr( Start );

												QueueChatCommand( m_GHost->m_Language->AutoHostEnabled( ), User, Whisper );
												m_GHost->ClearAutoHostMap( );
												m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
												m_GHost->m_AutoHostMapCounter = 0;
												m_GHost->m_AutoHostGameName = GameName;
//												m_GHost->m_AutoHostMapCFG = m_GHost->m_Map->GetCFGFile( );
												m_GHost->m_AutoHostOwner = User;
												m_GHost->m_AutoHostServer = m_Server;
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
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !AUTOHOSTL
				// !AHL
				//

				if( Command == "autohostl" || Command == "ahl" )
				{
					if( IsRootAdmin( User ) )
					{
						if( Payload.empty( ) || Payload == "off" )
						{
							QueueChatCommand( m_GHost->m_Language->AutoHostDisabled( ), User, Whisper );
							m_GHost->m_AutoHostGameName.clear( );
							m_GHost->m_AutoHostMapCFG.clear( );
//							m_GHost->m_AutoHostCountryCheck = false;
							m_GHost->m_AutoHostGArena = false;
							m_GHost->m_AutoHostLocal = false;
							m_GHost->m_AutoHostMaximumGames = 0;
							m_GHost->m_AutoHostAutoStartPlayers = 0;
							m_GHost->m_LastAutoHostTime = GetTime( );
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to autohost command" );
							else
							{
								SS >> AutoStartPlayers;

								if( SS.fail( ) || AutoStartPlayers == 0 )
									CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to autohost command" );
								else
								{
									if( SS.eof( ) )
										CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #3 to autohost command" );
									else
									{
										getline( SS, GameName );
										string :: size_type Start = GameName.find_first_not_of( " " );

										if( Start != string :: npos )
											GameName = GameName.substr( Start );

										QueueChatCommand( m_GHost->m_Language->AutoHostEnabled( ), User, Whisper );
										m_GHost->ClearAutoHostMap( );
										m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
										m_GHost->m_AutoHostMapCounter = 0;
										m_GHost->m_AutoHostGameName = GameName;
//										m_GHost->m_AutoHostMapCFG = m_GHost->m_Map->GetCFGFile( );
										m_GHost->m_AutoHostMaximumGames = MaximumGames;
										m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
//										m_GHost->m_AutoHostCountryCheck = false;
										m_GHost->m_AutoHostGArena = false;
										m_GHost->m_AutoHostLocal = true;
										m_GHost->m_LastAutoHostTime = GetTime( );
									}
								}
							}
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}


				//
				// !AUTOHOSTG
				// !AHG
				//

				if( Command == "autohostg" || Command == "ahg" )
				{
					if( IsRootAdmin( User ) )
					{
						if( Payload.empty( ) || Payload == "off" )
						{
							QueueChatCommand( m_GHost->m_Language->AutoHostDisabled( ), User, Whisper );
							m_GHost->m_AutoHostGameName.clear( );
							m_GHost->m_AutoHostMapCFG.clear( );
//							m_GHost->m_AutoHostCountryCheck = false;
							m_GHost->m_AutoHostLocal = false;
							m_GHost->m_AutoHostGArena = false;
							m_GHost->m_AutoHostMaximumGames = 0;
							m_GHost->m_AutoHostAutoStartPlayers = 0;
							m_GHost->m_LastAutoHostTime = GetTime( );
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to autohost command" );
							else
							{
								SS >> AutoStartPlayers;

								if( SS.fail( ) || AutoStartPlayers == 0 )
									CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to autohost command" );
								else
								{
									if( SS.eof( ) )
										CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #3 to autohost command" );
									else
									{
										getline( SS, GameName );
										string :: size_type Start = GameName.find_first_not_of( " " );

										if( Start != string :: npos )
											GameName = GameName.substr( Start );

										QueueChatCommand( m_GHost->m_Language->AutoHostEnabled( ), User, Whisper );
										m_GHost->ClearAutoHostMap( );
										m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
										m_GHost->m_AutoHostMapCounter = 0;
										m_GHost->m_AutoHostGameName = GameName;
//										m_GHost->m_AutoHostMapCFG = m_GHost->m_Map->GetCFGFile( );
										m_GHost->m_AutoHostMaximumGames = MaximumGames;
										m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
//										m_GHost->m_AutoHostCountryCheck = false;
										m_GHost->m_AutoHostLocal = false;
										m_GHost->m_AutoHostGArena = true;
										m_GHost->m_LastAutoHostTime = GetTime( );
									}
								}
							}
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}


				//
				// !AUTOSTART
				// !AS
				//

				if( ( Command == "autostart" || Command == "as" ) && m_GHost->m_CurrentGame && !m_GHost->m_CurrentGame->GetCountDownStarted( ) )
				{
					if (m_GHost->m_onlyownerscanstart)
						if ((!m_GHost->m_CurrentGame->IsOwner( User) && m_GHost->m_CurrentGame->GetPlayerFromName(m_GHost->m_CurrentGame->GetOwnerName(), false)) && !RootAdminCheck )
						{
							QueueChatCommand( "Only the owner can autostart the game.", User, Whisper);
							return;
						}

					if( Payload.empty( ) || Payload == "off" )
					{
						QueueChatCommand( m_GHost->m_Language->AutoStartDisabled( ), User, Whisper );
						m_GHost->m_CurrentGame->SetAutoStartPlayers( 0 );						
					}
					else
					{
						uint32_t AutoStartPlayers = UTIL_ToUInt32( Payload );

						if( AutoStartPlayers != 0 )
						{
							QueueChatCommand( m_GHost->m_Language->AutoStartEnabled( UTIL_ToString( AutoStartPlayers ) ), User, Whisper );
							m_GHost->m_CurrentGame->SetAutoStartPlayers( AutoStartPlayers );
						}
					}
				}

				//
				// !CHANNEL (change channel)
				// !CC
				//

				if( ( Command == "channel" || Command == "cc" ) && !Payload.empty( ) )
					QueueChatCommand( "/join " + Payload );

				//
				// !CHECKADMIN
				// !CA
				//

				if( ( Command == "checkadmin" || Command == "ca" ) && !Payload.empty( ) )
				{
					if( RootAdminCheck )
					{
						string sUser = GetPlayerFromNamePartial(Payload);
						if (sUser.empty())
							sUser = Payload;
						if( IsAdmin( sUser ) )
							QueueChatCommand( m_GHost->m_Language->UserIsAnAdmin( m_Server, sUser ), User, Whisper );
						else
							QueueChatCommand( m_GHost->m_Language->UserIsNotAnAdmin( m_Server, sUser ), User, Whisper );
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !CHECKBAN
				// !CB
				//

				if( ( Command == "checkban" || Command == "cb" ) && !Payload.empty( ) )
				{
					CDBBan *Ban = IsBannedName( Payload );

					if( Ban )
						QueueChatCommand( m_GHost->m_Language->UserWasBannedOnByBecause( m_Server, Payload, Ban->GetDate( ), Ban->GetDaysRemaining( ), Ban->GetAdmin( ), Ban->GetReason( ), Ban->GetExpireDate() ), User, Whisper );
					else
						QueueChatCommand( m_GHost->m_Language->UserIsNotBanned( m_Server, Payload ), User, Whisper );
				}

				//
				// !CHECKWARN
				// !CHECKWARNS
				// !CW
				//

				if( (Command == "checkwarn" || Command == "checkwarns" || Command == "cw" )&& !Payload.empty( ) )
				{
					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(nam);
					if (!pnam.empty())
						nam = pnam;

					uint32_t WarnCount = 0;
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						WarnCount += m_GHost->m_DB->BanCount( (*i)->GetServer(), nam, 1 );
					if ( WarnCount > 0 )
					{
						string reasons = m_GHost->m_DB->WarnReasonsCheck( nam, 1 );
						string message = m_GHost->m_Language->UserWarnReasons( nam, UTIL_ToString(WarnCount) );

						message += reasons;

						QueueChatCommand( message, User, Whisper );
						return;
					}
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						WarnCount += m_GHost->m_DB->BanCount( (*i)->GetServer(), nam, 3 );
					if( WarnCount > 0 )
					{
						string reasons = m_GHost->m_DB->WarnReasonsCheck( nam, 3 );
						string message = m_GHost->m_Language->UserBanWarnReasons( nam);

						message += reasons;

						QueueChatCommand( message, User, Whisper );
						return;
					}

					QueueChatCommand( m_GHost->m_Language->UserIsNotWarned( nam ), User, Whisper );
				}

				//
				// !CLOSE (close slot)
				// !C
				//

				if( ( Command == "close" || Command == "c" ) && !Payload.empty( ) && m_GHost->m_CurrentGame )
				{
					if (!CMDCheck(CMD_close, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if( !m_GHost->m_CurrentGame->GetLocked( ) )
					{
						// close as many slots as specified, e.g. "5 10" closes slots 5 and 10

						stringstream SS;
						SS << Payload;

						while( !SS.eof( ) )
						{
							uint32_t SID;
							SS >> SID;

							if( SS.fail( ) )
							{
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input to close command" );
								break;
							}
							else
								m_GHost->m_CurrentGame->CloseSlot( (unsigned char)( SID - 1 ), true );
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->TheGameIsLockedBNET( ), User, Whisper );
				}

				//
				// !CLOSEALL
				// !CA
				//

				if( ( Command == "closeall" || Command == "ca" ) && m_GHost->m_CurrentGame )
				{
					if (!CMDCheck(CMD_close, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if( !m_GHost->m_CurrentGame->GetLocked( ) )
						m_GHost->m_CurrentGame->CloseAllSlots( );
					else
						QueueChatCommand( m_GHost->m_Language->TheGameIsLockedBNET( ), User, Whisper );
				}

				//
				// !COUNTADMINS
				// !CAS
				//

				if( ( Command == "countadmins" || Command == "cas" ) )
				{
					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					if( IsRootAdmin( User ) )
						m_PairedAdminCounts.push_back( PairedAdminCount( Usr, m_GHost->m_DB->ThreadedAdminCount( m_Server ) ) );
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !COUNTBANS
				// !CBS
				//

				if( ( Command == "countbans" || Command == "cbs" ) )
				{
					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					m_PairedBanCounts.push_back( PairedBanCount( Usr, m_GHost->m_DB->ThreadedBanCount( m_Server ) ) );
				}

				//
				// !DBSTATUS
				// !DBS
				//

				if( Command == "dbstatus" || Command == "dbs" )
					QueueChatCommand( m_GHost->m_DB->GetStatus( ), User, Whisper );

				//
				// !DELADMIN
				// !DA
				//

				if( ( Command == "deladmin" || Command == "da" ) && !Payload.empty( ) )
				{
					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(nam);
					if (!pnam.empty())
						nam = pnam;

					if( IsRootAdmin( User ) )
					{
						string Usr;
						Usr = Whisper ? User : string( );
						if (m_GHost->m_WhisperAllMessages)
							Usr = User;

						if( !IsAdmin( nam ) )
							QueueChatCommand( m_GHost->m_Language->UserIsNotAnAdmin( m_Server, nam ), User, Whisper );
						else
							m_PairedAdminRemoves.push_back( PairedAdminRemove( Usr, m_GHost->m_DB->ThreadedAdminRemove( m_Server, nam ) ) );
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !DELBAN
				// !DB
				// !UNBAN
				// !UB
				//

				if( ( Command == "delban" || Command == "db" || Command == "unban" || Command == "ub" ) && !Payload.empty( ) )
				{
					if (!CMDCheck(CMD_delban, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					string nam = Payload;
					string pnam = GetPlayerFromNamePartial(nam);
					if (!pnam.empty())
						nam = pnam;

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					if ((m_GHost->m_AdminsLimitedUnban || m_GHost->m_AdminsCantUnbanRootadminBans) && !RootAdminCheck)
					{
						bool banned = false;
						string banadmin = string();
						for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						{
							if ((*i)->GetServer()==m_Server)
							{
								CDBBan *Ban = (*i)->IsBannedName( nam );

								if( Ban )
								{
									banned = true;
									banadmin = Ban->GetAdmin();
								}
							}
						}
						if (!banned)
						{
							QueueChatCommand(m_GHost->m_Language->UserIsNotBanned(m_ServerAlias, User), User, Whisper);
							return;
						}
						if (banned && User!=banadmin && m_GHost->m_AdminsLimitedUnban)
						{
							QueueChatCommand( "Limited unban enabled, "+nam+ " can only be unbanned by "+banadmin + " or a rootadmin", User, Whisper);
							return;
						}
						if (banned && IsRootAdmin(banadmin) && m_GHost->m_AdminsCantUnbanRootadminBans)
						{
							QueueChatCommand( "Admins can't remove rootadmin bans, "+nam+ " can only be unbanned by "+banadmin + " or a rootadmin", User, Whisper);
							return;
						}
						m_PairedBanRemoves.push_back( PairedBanRemove( Usr, m_GHost->m_DB->ThreadedBanRemove( string(), nam, User, 0 ) ) );
					}
					else
						m_PairedBanRemoves.push_back( PairedBanRemove( Usr, m_GHost->m_DB->ThreadedBanRemove( nam, 0 ) ) );
				}

				//
				// !DRD (turn dynamic latency on or off)
				//

				if( Command == "drd" || Command == "dlatency" || Command == "ddr")
				{
					if (Payload.empty())
					{
						if (m_GHost->m_UseDynamicLatency)
							QueueChatCommand( "Dynamic latency disabled", User, Whisper );
						else
							QueueChatCommand( "Dynamic latency enabled", User, Whisper );
						m_GHost->m_UseDynamicLatency = !m_GHost->m_UseDynamicLatency;

						return;
					}
					if( Payload == "on" )
					{
						QueueChatCommand( "Dynamic latency enabled", User, Whisper );
						m_GHost->m_UseDynamicLatency = true;
					}
					else if( Payload == "off" )
					{
						QueueChatCommand( "Dynamic latency disabled", User, Whisper );
						m_GHost->m_UseDynamicLatency = false;
					}
				}

				//
				// !DISABLE
				// !D
				//

				if( Command == "disable" || Command == "d" )
				{
					if( IsRootAdmin( User ) )
					{
						QueueChatCommand( m_GHost->m_Language->BotDisabled( ), User, Whisper );
						m_GHost->m_Enabled = false;
						m_GHost->m_DisableReason = Payload;
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !ENABLE
				// !E
				//

				if( Command == "enable" || Command == "e" )
				{
					if( IsRootAdmin( User ) )
					{
						QueueChatCommand( m_GHost->m_Language->BotEnabled( ), User, Whisper );
						m_GHost->m_Enabled = true;
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !END
				//

				if( Command == "end" && !Payload.empty( ) )
				{
					// todotodo: what if a game ends just as you're typing this command and the numbering changes?

					if (!CMDCheck(CMD_end, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					uint32_t GameNumber = UTIL_ToUInt32( Payload ) - 1;

					if( GameNumber < m_GHost->m_Games.size( ) )
					{
						// if the game owner is still in the game only allow the root admin to end the game

						if( m_GHost->m_Games[GameNumber]->GetPlayerFromName( m_GHost->m_Games[GameNumber]->GetOwnerName( ), false ) && !IsRootAdmin( User ) && IsAdmin(m_GHost->m_Games[GameNumber]->GetOwnerName( )))
							QueueChatCommand( m_GHost->m_Language->CantEndGameOwnerIsStillPlaying( m_GHost->m_Games[GameNumber]->GetOwnerName( ) ), User, Whisper );
						else
						{
							if (!RootAdminCheck)
							if (m_GHost->m_EndReq2ndTeamAccept && m_GHost->m_Games[GameNumber]->m_GetMapNumTeams==2)
								return;
							QueueChatCommand( m_GHost->m_Language->EndingGame( m_GHost->m_Games[GameNumber]->GetDescription( ) ), User, Whisper );
							CONSOLE_Print( "[GAME: " + m_GHost->m_Games[GameNumber]->GetGameName( ) + "] is over (admin ended game)" );
							m_GHost->m_Games[GameNumber]->SendAllChat("Game will end in 5 seconds");
							m_GHost->m_Games[GameNumber]->m_GameEndCountDownStarted = true;
							m_GHost->m_Games[GameNumber]->m_GameEndCountDownCounter = 5;
							m_GHost->m_Games[GameNumber]->m_GameEndLastCountDownTicks = GetTicks();
	//						m_GHost->m_Games[GameNumber]->StopPlayers( "was disconnected (admin ended game)" );
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->GameNumberDoesntExist( Payload ), User, Whisper );
				}

				//
				// !EXIT
				// !QUIT
				// !Q
				//

				if( Command == "exit" || Command == "quit" || Command == "q" )
				{
					if (!CMDCheck(CMD_quit, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}
						if( Payload == "nice" )
							m_GHost->m_ExitingNice = true;
						else if( Payload == "force" )
							m_Exiting = true;
					else
					{
						if( m_GHost->m_CurrentGame || !m_GHost->m_Games.empty( ) )
							QueueChatCommand( m_GHost->m_Language->AtLeastOneGameActiveUseForceToShutdown( ), User, Whisper );
						else
							m_Exiting = true;
					}
				}

				//
				// !GETCLAN
				// !GC
				//

				if( Command == "getclan" )
				{
					SendGetClanList( );
					QueueChatCommand( m_GHost->m_Language->UpdatingClanList( ), User, Whisper );
				}

				//
				// !GETFRIENDS
				// !GFS
				//

				if( Command == "getfriends" || Command == "gfs" )
				{
					SendGetFriendsList( );
					QueueChatCommand( m_GHost->m_Language->UpdatingFriendsList( ), User, Whisper );
				}

				//
				// !GETGAME
				// !GG
				//

				if( ( Command == "getgame" || Command == "gg" ) && !Payload.empty( ) )
				{
					uint32_t GameNumber = UTIL_ToUInt32( Payload ) - 1;

					if( GameNumber < m_GHost->m_Games.size( ) )
						QueueChatCommand( m_GHost->m_Games[GameNumber]->GetGameInfo(), User, Whisper);
//						QueueChatCommand( m_GHost->m_Language->GameNumberIs( Payload, m_GHost->m_Games[GameNumber]->GetDescription( ) ), User, Whisper );
					else
						QueueChatCommand( m_GHost->m_Language->GameNumberDoesntExist( Payload ), User, Whisper );
				}

				//
				// !GETGAMES
				// !GGS
				//

				if( Command == "getgames" || Command == "ggs" )
				{
					if ( m_PasswordHashType == "pvpgn")
					{
						string user = User;
						string s;
	
						if( m_GHost->m_CurrentGame )
							ImmediateChatCommand( "/w "+user+" ("+UTIL_ToString(m_TodayGamesCount) +" today) "+m_GHost->m_Language->GameIsInTheLobby( m_GHost->m_CurrentGame->GetDescription( ), UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ));
	
						for (uint32_t i=0; i<m_GHost->m_Games.size( ); i++ )
						{
							s = s + " " + UTIL_ToString(i) + "." + m_GHost->m_Games[i]->GetGameInfo();
						}
						ImmediateChatCommand( "/w "+user+" ("+UTIL_ToString(m_TodayGamesCount) +" today) "+ s );
						return;
					}

					if( m_GHost->m_CurrentGame )
						QueueChatCommand( "("+UTIL_ToString(m_TodayGamesCount) +" today) "+m_GHost->m_Language->GameIsInTheLobby( m_GHost->m_CurrentGame->GetDescription( ), UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ), User, Whisper );
					else
						QueueChatCommand( "("+UTIL_ToString(m_TodayGamesCount) +" today) "+m_GHost->m_Language->ThereIsNoGameInTheLobby( UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ), User, Whisper );
				}

				//
				// !UNMUTE
				// !UM
				//

				if( ( Command == "unmute" || Command == "um" ) && !Payload.empty( ) && m_GHost->m_CurrentGame)
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = m_GHost->m_CurrentGame->GetPlayerFromNamePartial( Payload, &LastMatch );

					if( Matches == 0 )
						QueueChatCommand( "no match found!" , User, Whisper);
					else if( Matches == 1 )
					{
						string MuteName = LastMatch->GetName();
						if (m_GHost->m_CurrentGame->IsMuted(MuteName))
						{
							QueueChatCommand( m_GHost->m_Language->RemovedPlayerFromTheMuteList( MuteName ), User, Whisper );
							m_GHost->m_CurrentGame->DelFromMuted( MuteName);
						}
					}
					//				else // Unable to unmute, more than one match found
				}

				//
				// !MUTE
				// !M
				//

				if( ( Command == "mute" || Command == "m" ) && !Payload.empty( ) && m_GHost->m_CurrentGame)
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = m_GHost->m_CurrentGame->GetPlayerFromNamePartial( Payload, &LastMatch );

					if( Matches == 0 )
						QueueChatCommand( "no match found!", User, Whisper );
					else if( Matches == 1 )
					{
						string MuteName = LastMatch->GetName();
						QueueChatCommand( m_GHost->m_Language->AddedPlayerToTheMuteList( MuteName ), User, Whisper );
						m_GHost->m_CurrentGame->AddToMuted( MuteName);
					}
					//				else // Unable to mute, more than one match found
				}

				//
				// !REHOSTDELAY
				// !RD
				//

				if ( ( Command == "rehostdelay" || Command == "rd" ) )
				{
					if (Payload.empty())
						QueueChatCommand("rehostdelay is set to "+ UTIL_ToString(m_GHost->m_AutoRehostDelay), User, Whisper);
					else
					{
						m_GHost->m_AutoRehostDelay = UTIL_ToUInt32(Payload);
						QueueChatCommand("rehostdelay set to "+ Payload, User, Whisper);
					}
				}


				//
				// !HOLDS (hold a specified slot for someone)
				// !HS
				//

				if( ( Command == "holds" || Command == "hs" ) && !Payload.empty( ) && m_GHost->m_CurrentGame )
				{
					// hold as many players as specified, e.g. "Gen 2 Kilranin 4" holds players "Gen" and "Kilranin"

					stringstream SS;
					SS << Payload;

					while( !SS.eof( ) )
					{
						string HoldName;
						SS >> HoldName;

						if( SS.fail( ) )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input to hold command" );
							break;
						}
						else
						{
							uint32_t HoldNumber;
							unsigned char HoldNr;
							SS >> HoldNumber;
							if ( SS.fail ( ) )
							{
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input to hold command" );
								break;
							}
							else
							{
								HoldNr=(unsigned char)( HoldNumber - 1 );
								QueueChatCommand( m_GHost->m_Language->AddedPlayerToTheHoldList( HoldName ), User, Whisper );
								m_GHost->m_CurrentGame->AddToReserved( HoldName, HoldNr );
							}
						}
					}
				}

				//
				// !HOLD (hold a slot for someone)
				// !H
				//

				if( ( Command == "hold" || Command == "h" ) && !Payload.empty( ) && m_GHost->m_CurrentGame )
				{
					// hold as many players as specified, e.g. "Gen Kilranin" holds players "Gen" and "Kilranin"

					stringstream SS;
					SS << Payload;

					while( !SS.eof( ) )
					{
						string HoldName;
						SS >> HoldName;

						if( SS.fail( ) )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input to hold command" );
							break;
						}
						else
						{
							QueueChatCommand( m_GHost->m_Language->AddedPlayerToTheHoldList( HoldName ), User, Whisper );
							m_GHost->m_CurrentGame->AddToReserved( HoldName, 255 );
						}
					}
				}

				//
				// !HOSTSG
				// !HSG
				//

				if( ( Command == "hostsg" || Command == "hsg" ) && !Payload.empty( ) )
					m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, true, Payload, User, User, m_Server, Whisper );

				//
				// !LOAD (load config file)
				//

				if( Command == "load" )
				{
					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - map config path doesn't exist" );
								QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
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
									QueueChatCommand( m_GHost->m_Language->NoMapConfigsFound( ), User, Whisper );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( );
									QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( m_GHost->m_MapCFGPath + File ), User, Whisper );
									CConfig MapCFG;
									MapCFG.Read( LastMatch.string( ) );
									m_GHost->m_Map->Load( &MapCFG, m_GHost->m_MapCFGPath + File );
								}
								else
									QueueChatCommand( m_GHost->m_Language->FoundMapConfigs( FoundMapConfigs ), User, Whisper );
							}
						}
						catch( const exception &ex )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - caught exception [" + ex.what( ) + "]" );
							QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
						}
					}
				}

				//
				// !DELMAPCFG (delete config file)
				//

				if( Command == "delmapcfg" )
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
						return;
					}

					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - map config path doesn't exist" );
								QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
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
									QueueChatCommand( m_GHost->m_Language->NoMapConfigsFound( ), User, Whisper );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( );
									File = m_GHost->m_MapCFGPath+File;

									if( remove( File.c_str()) != 0 )
										QueueChatCommand( "Error deleting "+File, User, Whisper);
									else
										QueueChatCommand( "Config "+File+" deleted!", User, Whisper);
								}
								else
									QueueChatCommand( m_GHost->m_Language->FoundMapConfigs( FoundMapConfigs ), User, Whisper );
							}
						}
						catch( const exception &ex )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - caught exception [" + ex.what( ) + "]" );
							QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
						}
					}
				}

				//
				// !LOADL (load config file)
				// !LL
				// !MAPL
				// !ML
				//

				if( Command == "loadl" || Command == "ll" || Command == "mapl" || Command == "ml" )
				{
					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
					else
					{
						// only load files in the current directory just to be safe

						if( Payload.find( "/" ) != string :: npos || Payload.find( "\\" ) != string :: npos )
							QueueChatCommand( m_GHost->m_Language->UnableToLoadConfigFilesOutside( ), User, Whisper );
						else
						{
							string File = m_GHost->m_MapCFGPath + Payload + ".cfg";

							if( UTIL_FileExists( File ) )
							{
								// we have to be careful here because we didn't copy the map data when creating the game (there's only one global copy)
								// therefore if we change the map data while a game is in the lobby everything will get screwed up
								// the easiest solution is to simply reject the command if a game is in the lobby

								if( m_GHost->m_CurrentGame )
									QueueChatCommand( m_GHost->m_Language->UnableToLoadConfigFileGameInLobby( ), User, Whisper );
								else
								{
									QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( File ), User, Whisper );
									CConfig MapCFG;
									MapCFG.Read( File );
									m_GHost->m_Map->m_LogAll=true;
									m_GHost->m_Map->Load( &MapCFG, File );
									m_GHost->m_Map->m_LogAll=false;
								}
							}
							else
								QueueChatCommand( m_GHost->m_Language->UnableToLoadConfigFileDoesntExist( File ), User, Whisper );
						}
					}
				}

				//
				// !LOADSG
				// !LSG
				//

				if( ( Command == "loadsg" || Command == "lsg" ) && !Payload.empty( ) )
				{
					// only load files in the current directory just to be safe

					if( Payload.find( "/" ) != string :: npos || Payload.find( "\\" ) != string :: npos )
						QueueChatCommand( m_GHost->m_Language->UnableToLoadSaveGamesOutside( ), User, Whisper );
					else
					{
						string File = m_GHost->m_SaveGamePath + Payload + ".w3z";
						string FileNoPath = Payload + ".w3z";

						if( UTIL_FileExists( File ) )
						{
							if( m_GHost->m_CurrentGame )
								QueueChatCommand( m_GHost->m_Language->UnableToLoadSaveGameGameInLobby( ), User, Whisper );
							else
							{
								QueueChatCommand( m_GHost->m_Language->LoadingSaveGame( File ), User, Whisper );
								m_GHost->m_SaveGame->Load( File, false );
								m_GHost->m_SaveGame->ParseSaveGame( );
								m_GHost->m_SaveGame->SetFileName( File );
								m_GHost->m_SaveGame->SetFileNameNoPath( FileNoPath );
							}
						}
						else
							QueueChatCommand( m_GHost->m_Language->UnableToLoadSaveGameDoesntExist( File ), User, Whisper );
					}
				}

				//
				// !MAP (load map file)
				//

				if( Command == "map" )
				{
					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - map path doesn't exist" );
								QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
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
									QueueChatCommand( m_GHost->m_Language->NoMapsFound( ), User, Whisper );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( );
									QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( File ), User, Whisper );

									// hackhack: create a config file in memory with the required information to load the map

									CConfig MapCFG;
									MapCFG.Set( "map_path", "Maps\\Download\\" + File );
									MapCFG.Set( "map_localpath", File );
									bool dota = false;
									string dt=string();
									string dc=string();
									string dh=string();
									if (File.find("Allstars")!=string::npos)
										dota = true;
									if (dota)
									{
										dt = "dota";
										dc = "dota_elo";
										if (m_GHost->m_forceautohclindota)
											dh = "ar";
									}
									MapCFG.Set( "map_type", dt );
									MapCFG.Set( "map_matchmakingcategory", dc );
									MapCFG.Set( "map_defaulthcl", dh );
									m_GHost->m_Map->Load( &MapCFG, File );
								}
								else
									QueueChatCommand( m_GHost->m_Language->FoundMaps( FoundMaps ), User, Whisper );
							}
						}
						catch( const exception &ex )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - caught exception [" + ex.what( ) + "]" );
							QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
						}
					}
				}

				//
				// !DELMAP (delete map file)
				//

				if( Command == "delmap" )
				{

					if (!RootAdminCheck)
					{
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
						return;
					}

					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
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
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - map path doesn't exist" );
								QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
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
									QueueChatCommand( m_GHost->m_Language->NoMapsFound( ), User, Whisper );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( );
									File = m_GHost->m_MapPath+File;

									if( remove( File.c_str()) != 0 )
										QueueChatCommand( "Error deleting "+File, User, Whisper);
									else
										QueueChatCommand( "Map "+File+" deleted!", User, Whisper);
								}
								else
									QueueChatCommand( m_GHost->m_Language->FoundMaps( FoundMaps ), User, Whisper );
							}
						}
						catch( const exception &ex )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - caught exception [" + ex.what( ) + "]" );
							QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
						}
					}
				}

				//
				// !OPEN (open slot)
				// !O
				//

				if( ( Command == "open" || Command == "o" ) && !Payload.empty( ) && m_GHost->m_CurrentGame )
				{
					if (!CMDCheck(CMD_open, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}
					if( !m_GHost->m_CurrentGame->GetLocked( ) )
					{
						// open as many slots as specified, e.g. "5 10" opens slots 5 and 10

						stringstream SS;
						SS << Payload;

						while( !SS.eof( ) )
						{
							uint32_t SID;
							SS >> SID;

							if( SS.fail( ) )
							{
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input to open command" );
								break;
							}
							else
								m_GHost->m_CurrentGame->OpenSlot( (unsigned char)( SID - 1 ), true );
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->TheGameIsLockedBNET( ), User, Whisper );
				}

				//
				// !OPENALL
				// !OA
				//

				if( ( Command == "openall" || Command == "oa" ) && m_GHost->m_CurrentGame )
				{
					if (!CMDCheck(CMD_open, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if( !m_GHost->m_CurrentGame->GetLocked( ) )
						m_GHost->m_CurrentGame->OpenAllSlots( );
					else
						QueueChatCommand( m_GHost->m_Language->TheGameIsLockedBNET( ), User, Whisper );
				}

				//
				// !PRIV (host private game)
				// !PR
				// !PRI
				//

				if( ( Command == "priv" || Command == "pr" || Command == "pri") && !Payload.empty( ) )
				{
					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					// adding the game creator as friend
					bool cf = false;
					if (m_GHost->m_addcreatorasfriendonhost && !IsFriend(User))
					{
						QueueChatCommand( "/f a "+User);
						cf = true;
					}

					if (Command=="pri")
						m_GHost->CreateGame( m_GHost->m_Map, m_GHost->m_gamestateinhouse, false, Payload, User, User, m_Server, Whisper );
					else
						m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, false, Payload, User, User, m_Server, Whisper );
					if (m_GHost->m_addcreatorasfriendonhost && !cf && m_GHost->m_CurrentGame)
						m_GHost->m_CurrentGame->m_CreatorAsFriend = false;
				}

				//
				// !PRIVBY (host private game by other player)
				// !PRIBY
				// !PBY
				//

				if( ( Command == "privby" || Command == "pby" || Command == "priby" ) && !Payload.empty( ) )
				{
					// extract the owner and the game name
					// e.g. "Gen dota 6.54b arem ~~~" -> owner: "Gen", game name: "dota 6.54b arem ~~~"

					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					string Owner;
					string GameName;
					string :: size_type GameNameStart = Payload.find( " " );

					if( GameNameStart != string :: npos )
					{
						Owner = Payload.substr( 0, GameNameStart );
						GameName = Payload.substr( GameNameStart + 1 );
						if (GameName.length()<1 || GameName==" ")
							return;
						if (Command == "priby")
							m_GHost->CreateGame( m_GHost->m_Map, m_GHost->m_gamestateinhouse, false, GameName, Owner, User, m_Server, Whisper );
						else
							m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, false, GameName, Owner, User, m_Server, Whisper );
					}
				}

				//
				// !PUBDL (host public game) download only
				// !PDL
				//

				if ( ( Command=="pubdl" || Command == "pdl" ) && !Payload.empty())
				{
					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, Payload, User, User, m_Server, Whisper );
					m_GHost->m_CurrentGame->m_DownloadOnlyMode = true;
				}

				//
				// !PUBNxx (host public game) xx=country name (ex: RO), denies players from that country
				//

				if  (( Command.length()>=6) && ( Command.substr(0,4) == "pubn" ) && 
					 (((Command.length()-4) % 2)==0))
				{
					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (m_GHost->m_LastGameName=="" && Payload.empty())
					{
						QueueChatCommand("No game has been hosted till now, specify a name", User, Whisper);
						return;
					}

					string GameName = Payload;
					if (GameName.empty())
						GameName = m_GHost->m_LastGameName;					
					string GameNr = string();
					uint32_t idx = 0;
					uint32_t Nr = 0;

/*
					if (!GameName.empty() && GameName==m_GHost->m_LastGameName)
					{
						QueueChatCommand("You can't use the same name!", User, Whisper);
						return;
					}
*/
					if (Payload.empty())
					{
						idx = GameName.length()-1;
						if (idx>=2)
						if (GameName.at(idx-2)=='#')
							idx = idx-1;
						else
							if (GameName.at(idx-1)=='#')
								idx = idx;
							else
								idx = 0;

						// idx = 0, no Game Nr found in gamename
						if (idx == 0)
						{
							GameNr = "0";
							GameName = GameName + " #";
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
						if (Nr>20)
							Nr = 1;
						GameNr = UTIL_ToString(Nr);
						GameName = GameName + GameNr;
					}
					int k = Command.length()-4;
					if (k % 2 != 0)
					{
						QueueChatCommand( "Syntax error, use for ex: !pubnroes [gamename]", User, Whisper );
						return;
					}
					k = k / 2;
					string from="";
					for (int i=1; i<k+1; i++)
					{
						if (i!=1)
							from=from+" "+Command.substr(4+(i-1)*2,2);
						else
							from=Command.substr(4+(i-1)*2,2);
					}
					transform( from.begin( ), from.end( ), from.begin( ), (int(*)(int))toupper );
					QueueChatCommand( "Country check enabled, denied countries: "+from, User, Whisper);
					from=from+ " ??";

					// adding the game creator as friend
					bool cf = false;
					if (m_GHost->m_addcreatorasfriendonhost && !IsFriend(User))
					{
						QueueChatCommand( "/f a "+User);
						cf = true;
					}

					m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, GameName, User, User, string( ), false );
					if (m_GHost->m_CurrentGame)
					{
						if (m_GHost->m_addcreatorasfriendonhost && !cf)
							m_GHost->m_CurrentGame->m_CreatorAsFriend = false;

						m_GHost->m_CurrentGame->m_Countries2=from;
						m_GHost->m_CurrentGame->m_CountryCheck2=true;
					}
					return;
				}


				//
				// !PUBxx (host public game) xx=country name (ex: RO), accepts only players from that country
				//

				if  (( Command.length()>=5) && ( Command != "pubby" ) && ( Command != "pubdl" ) && ( Command.substr(0,3) == "pub" ))
				{
					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (m_GHost->m_LastGameName=="" && Payload.empty())
					{
						QueueChatCommand("No game has been hosted till now, specify a name", User, Whisper);
						return;
					}

					string GameName = Payload;
					if (GameName.empty())
						GameName = m_GHost->m_LastGameName;					
					string GameNr = string();
					uint32_t idx = 0;
					uint32_t Nr = 0;
/*
					if (!GameName.empty() && GameName==m_GHost->m_LastGameName)
					{
						QueueChatCommand("You can't use the same name!", User, Whisper);
						return;
					}
*/
					if (Payload.empty())
					{
						idx = GameName.length()-1;
						if (idx>=2)
						if (GameName.at(idx-2)=='#')
							idx = idx-1;
						else
							if (GameName.at(idx-1)=='#')
								idx = idx;
							else
								idx = 0;

						// idx = 0, no Game Nr found in gamename
						if (idx == 0)
						{
							GameNr = "0";
							GameName = GameName + " #";
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
						if (Nr>20)
							Nr = 1;
						GameNr = UTIL_ToString(Nr);
						GameName = GameName + GameNr;
					}

					int k = Command.length()-3;
					string from="";
					if (k % 2 != 0)
					{
						QueueChatCommand( "Syntax error, use for ex: !pubroes [gamename]" , User, Whisper);
						return;
					}
					k = k / 2;

					for (int i=1; i<k+1; i++)
					{
						if (i!=1)
							from=from+" "+Command.substr(3+(i-1)*2,2);
						else
							from=Command.substr(3+(i-1)*2,2);
					}
					transform( from.begin( ), from.end( ), from.begin( ), (int(*)(int))toupper );

					m_GHost->m_QuietRehost = false;

					// adding the game creator as friend
					bool cf = false;
					if (m_GHost->m_addcreatorasfriendonhost && !IsFriend(User))
					{
						QueueChatCommand( "/f a "+User);
						cf = true;
					}

					m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, GameName, User, User, m_Server, Whisper );
					if (m_GHost->m_CurrentGame)
					{
						if (m_GHost->m_addcreatorasfriendonhost && !cf)
							m_GHost->m_CurrentGame->m_CreatorAsFriend = false;
						m_GHost->m_CurrentGame->m_Countries=from+" ??";
						m_GHost->m_CurrentGame->m_CountryCheck=true;
						QueueChatCommand( "Country check enabled, allowed countries: "+from, User, Whisper);
					}
				}


				//
				// !PUB (host public game)
				// !P
				//

				if( Command == "pub" || Command == "p" )
				{
					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (m_GHost->m_LastGameName=="" && Payload.empty())
					{
						QueueChatCommand("No game has been hosted till now, specify a name", User, Whisper);
						return;
					}

					string GameName = Payload;
					if (GameName.empty())
						GameName = m_GHost->m_LastGameName;					
					string GameNr = string();
					uint32_t idx = 0;
					uint32_t Nr = 0;
/*
					if (!GameName.empty() && GameName==m_GHost->m_LastGameName)
					{
						QueueChatCommand("You can't use the same name!", User, Whisper);
						return;
					}
*/
					if (Payload.empty())
					{
						idx = GameName.length()-1;
						if (idx>=2)
						if (GameName.at(idx-2)=='#')
							idx = idx-1;
						else
							if (GameName.at(idx-1)=='#')
								idx = idx;
							else
								idx = 0;

						// idx = 0, no Game Nr found in gamename
						if (idx == 0)
						{
							GameNr = "0";
							GameName = GameName + " #";
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
						if (Nr>20)
							Nr = 1;
						GameNr = UTIL_ToString(Nr);
						GameName = GameName + GameNr;
					}
					m_GHost->m_QuietRehost = false;

					// adding the game creator as friend
					bool cf = false;
					if (m_GHost->m_addcreatorasfriendonhost && !IsFriend(User))
					{
						QueueChatCommand( "/f a "+User);
						cf = true;
					}

					m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, GameName, User, User, m_Server, Whisper );
					if (m_GHost->m_addcreatorasfriendonhost && !cf && m_GHost->m_CurrentGame)
						m_GHost->m_CurrentGame->m_CreatorAsFriend = false;

				}

				//
				// !PUBG (host public game allowing only garena)
				// !PG
				//

				if( Command == "pubg" || Command == "pg" )
				{
					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if (m_GHost->m_LastGameName=="" && Payload.empty())
					{
						QueueChatCommand("No game has been hosted till now, specify a name", User, Whisper);
						return;
					}

					string GameName = Payload;
					if (GameName.empty())
						GameName = m_GHost->m_LastGameName;					
					string GameNr = string();
					uint32_t idx = 0;
					uint32_t Nr = 0;
					/*
					if (!GameName.empty() && GameName==m_GHost->m_LastGameName)
					{
					QueueChatCommand("You can't use the same name!", User, Whisper);
					return;
					}
					*/
					if (Payload.empty())
					{
						idx = GameName.length()-1;
						if (idx>=2)
						if (GameName.at(idx-2)=='#')
							idx = idx-1;
						else
							if (GameName.at(idx-1)=='#')
								idx = idx;
							else
								idx = 0;

						// idx = 0, no Game Nr found in gamename
						if (idx == 0)
						{
							GameNr = "0";
							GameName = GameName + " #";
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
						if (Nr>20)
							Nr = 1;
						GameNr = UTIL_ToString(Nr);
						GameName = GameName + GameNr;
					}
					m_GHost->m_QuietRehost = false;

					m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, GameName, User, User, m_Server, Whisper );
					if (m_GHost->m_CurrentGame)
					{
						if (m_GHost->m_HideBotFromNormalUsersInGArena)
						{
							m_GHost->m_CurrentGame->m_DetourAllMessagesToAdmins = true;
							m_GHost->m_NormalCountdown = true;
						}
						m_GHost->m_CurrentGame->m_GarenaOnly = true;
						m_GHost->m_CurrentGame->m_ShowRealSlotCount = true;
						QueueChatCommand( "GArena only", User, Whisper);
					}
				}

				//
				// !PUBBY (host public game by other player)
				//

				if( Command == "pubby" && !Payload.empty( ) )
				{
					if (!CMDCheck(CMD_host, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					// extract the owner and the game name
					// e.g. "Gen dota 6.54b arem ~~~" -> owner: "Gen", game name: "dota 6.54b arem ~~~"

					string Owner;
					string GameName;
					string :: size_type GameNameStart = Payload.find( " " );

					if( GameNameStart != string :: npos )
					{
						Owner = Payload.substr( 0, GameNameStart );
						GameName = Payload.substr( GameNameStart + 1 );
						if (GameName.length()<1 || GameName == " ")
							return;
						m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, GameName, Owner, User, m_Server, Whisper );
					}
				}

/*
				//
				// !RLOAD
				// !RL
				// !RMAP
				// !RM
				//

				if( Command == "rload" || Command == "rl" || Command == "rmap" || Command == "rm" )
				{
					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
					else
					{
						string FoundMaps;

						try
						{
							path MapPath( m_GHost->m_MapPath );
							boost :: regex Regex( Payload );
							string Pattern = Payload;
							transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

							if( !exists( MapPath ) )
							{
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - map path doesn't exist" );
								QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
							}
							else
							{
								directory_iterator EndIterator;
								path LastMatch;
								uint32_t Matches = 0;

								for( directory_iterator i( MapPath ); i != EndIterator; i++ )
								{
									string FileName = i->filename( );
									transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
									bool Matched = false;

									if( m_GHost->m_UseRegexes )
									{
										if( boost :: regex_match( FileName, Regex ) )
											Matched = true;
									}
									else if( FileName.find( Pattern ) != string :: npos )
										Matched = true;

									if( !is_directory( i->status( ) ) && Matched )
									{
										LastMatch = i->path( );
										Matches++;

										if( FoundMaps.empty( ) )
											FoundMaps = i->filename( );
										else
											FoundMaps += ", " + i->filename( );
									}
								}

								if( Matches == 0 )
									QueueChatCommand( m_GHost->m_Language->NoMapsFound( ), User, Whisper );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( );

									// we have to be careful here because we didn't copy the map data when creating the game (there's only one global copy)
									// therefore if we change the map data while a game is in the lobby everything will get screwed up
									// the easiest solution is to simply reject the command if a game is in the lobby

									if( m_GHost->m_CurrentGame )
										QueueChatCommand( m_GHost->m_Language->UnableToLoadConfigFileGameInLobby( ), User, Whisper );
									else
									{
										QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( File ), User, Whisper );

										// hackhack: create a config file in memory with the required information to load the map

										CConfig MapCFG;
										MapCFG.Set( "map_path", "Maps\\Download\\" + File );
										bool dota = false;
										string dt=string();
										string dc=string();
										if (File.find("Allstars")!=string::npos)
											dota = true;
										if (dota)
										{
											dt = "dota";
											dc = "dota_elo";
										}
										MapCFG.Set( "map_type", dt );
										MapCFG.Set( "map_matchmakingcategory", dc );

										MapCFG.Set( "map_localpath", File );
										m_GHost->m_Map->Load( &MapCFG, File );
									}
								}
								else
									QueueChatCommand( m_GHost->m_Language->FoundMaps( FoundMaps ), User, Whisper );
							}
						}
						catch( const exception &ex )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - caught exception [" + ex.what( ) + "]" );
							QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
						}
					}
				}
*/

				//
				// !RB (Refreshes Banlist)
				// !REFRESH
				//

				if( Command == "rb" || Command == "refresh")
				{
					m_LastBanRefreshTime = GetTime() - 3600;
					m_LastAdminRefreshTime = GetTime() - 300;
				}

				//
				// !SAY
				// !S
				//

				if( ( Command == "say" || Command == "s" ) && !Payload.empty( ) )
				{
					if (!CMDCheck(CMD_say, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					QueueChatCommand( Payload );
				}

				//
				// !SAYGAME
				// !SG
				//

				if( ( Command == "saygame" || Command == "sg" ) && !Payload.empty( ) )
				{
					if( IsRootAdmin( User ) )
					{
						// extract the game number and the message
						// e.g. "3 hello everyone" -> game number: "3", message: "hello everyone"

						uint32_t GameNumber;
						string Message;
						stringstream SS;
						SS << Payload;
						SS >> GameNumber;

						if( SS.fail( ) )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to saygame command" );
						else
						{
							if( SS.eof( ) )
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #2 to saygame command" );
							else
							{
								getline( SS, Message );
								string :: size_type Start = Message.find_first_not_of( " " );

								if( Start != string :: npos )
									Message = Message.substr( Start );

								if( GameNumber - 1 < m_GHost->m_Games.size( ) )
									m_GHost->m_Games[GameNumber - 1]->SendAllChat( "ADMIN: " + Message );
								else
									QueueChatCommand( m_GHost->m_Language->GameNumberDoesntExist( UTIL_ToString( GameNumber ) ), User, Whisper );
							}
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !SAYGAMES
				// !SGS
				//

				if( ( Command == "saygames" || Command == "sgs" ) && !Payload.empty( ) )
				{
					if( IsRootAdmin( User ) )
					{
						if( m_GHost->m_CurrentGame )
							m_GHost->m_CurrentGame->SendAllChat( Payload );

						for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); i++ )
							(*i)->SendAllChat( "ADMIN: " + Payload );
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !SP
				//

				if( Command == "sp" && m_GHost->m_CurrentGame && !m_GHost->m_CurrentGame->GetCountDownStarted( ) )
				{
					if (!CMDCheck(CMD_sp, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if( !m_GHost->m_CurrentGame->GetLocked( ) )
					{
						m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->ShufflingPlayers( ) );
						m_GHost->m_CurrentGame->ShuffleSlots( );
					}
					else
						QueueChatCommand( m_GHost->m_Language->TheGameIsLockedBNET( ), User, Whisper );
				}

				//
				// !START
				//

				if( Command == "start" && m_GHost->m_CurrentGame && !m_GHost->m_CurrentGame->GetCountDownStarted( ) && m_GHost->m_CurrentGame->GetNumPlayers( ) > 0 )
				{
					if (m_GHost->m_onlyownerscanstart)
						if ((!m_GHost->m_CurrentGame->IsOwner( User) && m_GHost->m_CurrentGame->GetPlayerFromName(m_GHost->m_CurrentGame->GetOwnerName(), false)) && !RootAdminCheck )
						{
							QueueChatCommand( "Only the owner can start the game.", User, Whisper);
							return;
						}

					if( !m_GHost->m_CurrentGame->GetLocked( ) )
					{
						// if the player sent "!start force" skip the checks and start the countdown
						// otherwise check that the game is ready to start

						if( Payload == "force" )
							m_GHost->m_CurrentGame->StartCountDown( true );
						else
							m_GHost->m_CurrentGame->StartCountDown( false );
					}
					else
						QueueChatCommand( m_GHost->m_Language->TheGameIsLockedBNET( ), User, Whisper );
				}

				//
				// !SWAP (swap slots)
				//

				if( Command == "swap" && !Payload.empty( ) && m_GHost->m_CurrentGame )
				{
					if (!CMDCheck(CMD_swap, AdminAccess))
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if( !m_GHost->m_CurrentGame->GetLocked( ) )
					{
						uint32_t SID1;
						uint32_t SID2;
						stringstream SS;
						SS << Payload;
						SS >> SID1;

						if( SS.fail( ) )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to swap command" );
						else
						{
							if( SS.eof( ) )
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #2 to swap command" );
							else
							{
								SS >> SID2;

								if( SS.fail( ) )
									CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to swap command" );
								else
								{
									bool isAdmin = false;
									bool isRootAdmin = false;
									bool sameteam = false;
									if (SID1-1<m_GHost->m_CurrentGame->m_Slots.size() && SID2-1<m_GHost->m_CurrentGame->m_Slots.size())
										sameteam = m_GHost->m_CurrentGame->m_Slots[SID1-1].GetTeam() == m_GHost->m_CurrentGame->m_Slots[SID2-1].GetTeam();
									CGamePlayer *Player = m_GHost->m_CurrentGame->GetPlayerFromSID( SID1 - 1 );
									CGamePlayer *Player2 = m_GHost->m_CurrentGame->GetPlayerFromSID( SID2 - 1 );
									if (Player)
									if (Player->GetName()!=User)
										if (IsRootAdmin(Player->GetName()))
											isRootAdmin = true;
									if (Player2)
									if (Player2->GetName()!=User)
										if (IsRootAdmin(Player2->GetName()))
											isRootAdmin = true;
									if (m_GHost->m_onlyownerscanswapadmins && !sameteam)
									{
										if (Player)
										{
											if (IsAdmin(Player->GetName()) || IsRootAdmin(Player->GetName()))
												if (Player->GetName()!=User)
													isAdmin = true;
										}
										if (Player2)
										{
											if (IsAdmin(Player2->GetName()) || IsRootAdmin(Player2->GetName()))
												if (Player2->GetName()!=User)
													isAdmin = true;
										}
									}
									if (isRootAdmin && !RootAdminCheck)
									{
										QueueChatCommand( "You can't swap a rootadmin!", User, Whisper);
										return;
									}

									if (isAdmin && !(m_GHost->m_CurrentGame->IsOwner(User) || RootAdminCheck))
									{
										QueueChatCommand( "You can't swap an admin!", User, Whisper);
										return;
									} else
									m_GHost->m_CurrentGame->SwapSlots( (unsigned char)( SID1 - 1 ), (unsigned char)( SID2 - 1 ) );
								}
							}
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->TheGameIsLockedBNET( ), User, Whisper );
				}

				//
				// !ONLY
				//
				if (false)
				if( Command == "only" )
				{
					if (Payload.empty( ))
					{
						m_GHost->m_CurrentGame->m_CountryCheck=false;
						QueueChatCommand( "Country allowed check disabled");
					} else
					{
						m_GHost->m_CurrentGame->m_CountryCheck=true;
						m_GHost->m_CurrentGame->m_Countries=Payload;
						transform( m_GHost->m_CurrentGame->m_Countries.begin( ), m_GHost->m_CurrentGame->m_Countries.end( ), m_GHost->m_CurrentGame->m_Countries.begin( ), (int(*)(int))toupper );
						QueueChatCommand( "Country check enabled, allowed countries: "+m_GHost->m_CurrentGame->m_Countries);
					}
				}

				// !STATUS

				if( Command == "status")
				{
					string sMsg = string();
					if (m_GHost->m_CurrentGame)
					{
						string slots = UTIL_ToString(m_GHost->m_CurrentGame->GetSlotsOccupied())+"/"+UTIL_ToString(m_GHost->m_CurrentGame->m_Slots.size());
						sMsg = "Lobby: "+ m_GHost->m_CurrentGame->GetGameName()+" ("+slots+") - "+m_GHost->m_CurrentGame->GetOwnerName();
					} else if (m_GHost->m_Games.size()>0)
					{
						sMsg = m_GHost->m_Games[m_GHost->m_Games.size()-1]->GetGameInfo();
					}

					if (!sMsg.empty())
					{
						CONSOLE_Print( "[LOCAL: " + m_ServerAlias + "] status:" + sMsg );
						QueueChatCommand( sMsg, User, Whisper);
					}
				}


				//
				// !IPADD
				//

				if( Command == "ipadd" && !Payload.empty())
				{
					bool n;
					n = true;
					for(uint32_t p=0; p<m_GHost->m_IPUsers.size(); p++)
					{
						if (m_GHost->m_IPUsers[p]==Payload)
							n = false;
					}					
					if (n)
					{
						m_GHost->m_IPUsers.push_back(Payload);
						QueueChatCommand(Payload +" added to IPs", User, Whisper);
					} else
						QueueChatCommand(Payload +" already in IPs", User, Whisper);
				}

				//
				// !IPDEL
				//

				if( Command == "ipdel" && !Payload.empty())
				{
					int32_t n = -1;
					for(uint32_t p=0; p<m_GHost->m_IPUsers.size(); p++)
					{
						if (m_GHost->m_IPUsers[p]==Payload)
							n = p;
					}					
					if (n!=-1)
					{
						m_GHost->m_IPUsers.erase(m_GHost->m_IPUsers.begin()+n);
						QueueChatCommand(Payload +" erased from IPs", User, Whisper);
					}
				}

				//
				// !FW
				//

				if( Command == "fw")
				{
					string sMsg = Payload;
					if (sMsg.empty())
					{
						if (m_GHost->m_CurrentGame)
						{
							string slots = UTIL_ToString(m_GHost->m_CurrentGame->GetSlotsOccupied())+"/"+UTIL_ToString(m_GHost->m_CurrentGame->m_Slots.size());
							sMsg = "Lobby: "+ m_GHost->m_CurrentGame->GetGameName()+" ("+slots+") - "+m_GHost->m_CurrentGame->GetOwnerName();
						} else
						if (m_GHost->m_Games.size()>0)
						{
							sMsg = m_GHost->m_Games[m_GHost->m_Games.size()-1]->GetGameInfo();
						}
					}
					if (!sMsg.empty())
						QueueChatCommand( "/f msg "+sMsg);
				}
				
				//
				// !FADD !FA !ADDFRIEND !FD
				//

				if( ( Command == "fadd" || Command == "fa" || Command =="addfriend" || Command == "af" ) && !Payload.empty())
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
						return;
					}
					string sUser = GetPlayerFromNamePartial(Payload);
					if (sUser.empty())
						sUser = Payload;
					QueueChatCommand( "/f a "+sUser);
					QueueChatCommand( "Trying to add friend "+sUser, User, Whisper);
					SendGetFriendsList();
				}

				//
				// !FDEL !FD !DELFRIEND !DF
				//

				if( ( Command == "fdel" || Command == "fd" || Command =="delfriend" || Command == "df" ) && !Payload.empty())
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
						return;
					}
					string sUser = GetPlayerFromNamePartial(Payload);
					if (sUser.empty())
						sUser = Payload;
					QueueChatCommand( "/f r "+sUser);
					QueueChatCommand( "Trying to del friend "+sUser, User, Whisper);
					SendGetFriendsList();
				}

				//
				// !ONLYP
				//

				if (false)
				if( Command == "onlyp"  || Command == "op" )
				{
					if (Payload.empty( ))
					{
						m_GHost->m_CurrentGame->m_ProviderCheck=false;
						QueueChatCommand( "Provider check disabled");
					} else
					{
						m_GHost->m_CurrentGame->m_ProviderCheck=true;
						m_GHost->m_CurrentGame->m_Providers=Payload;
						transform( m_GHost->m_CurrentGame->m_Providers.begin( ), m_GHost->m_CurrentGame->m_Providers.end( ), m_GHost->m_CurrentGame->m_Providers.begin( ), (int(*)(int))toupper );

						QueueChatCommand( "Provider check enabled, allowed provider: "+m_GHost->m_CurrentGame->m_Providers);
					}
				}

				//
				// !Marsauto
				//

				if( Command == "marsauto")
				{
					string msg = "Auto insult ";
					m_GHost->m_autoinsultlobby = !m_GHost->m_autoinsultlobby;
					if (m_GHost->m_autoinsultlobby)
						msg += "ON";
					else
						msg += "OFF";

					QueueChatCommand(msg, User, Whisper );
					return;
				}

				//
				// !Mars
				//

				if( Command == "mars" && (GetTime()-m_LastMars>=10))
				{
					if (m_GHost->m_Mars.size()==0)
						return;
					vector<string> randoms;
					string nam1 = User;
					string nam2 = Payload;
					// if no user is specified, randomize one != with the user giving the command
					if (Payload.empty())
					{
						for( uint32_t i = 0; i != Channel_Users().size( ); i++ )
						{
							if (Channel_Users()[i]!=nam1)
								randoms.push_back(Channel_Users()[i]);
						}
						random_shuffle(randoms.begin(), randoms.end());
						// if no user has been randomized, return
						if (randoms.size()>0)
							nam2 = randoms[0];
						else
							return;
						randoms.clear();
					} else
					{
						nam2 = GetPlayerFromNamePartial(Payload);
						if (nam2.empty()) 
							return;
					}
					string srv = GetServer();
					bool safe1 = false;
					bool safe2 = false;
					safe1 = IsSafe(nam1) || AdminCheck || RootAdminCheck;
					safe2 = IsSafe(nam2) || IsAdmin(nam2) || IsRootAdmin(nam2);
// hack, enable mars for any victims
					safe2 = true;
					if (((safe1 && safe2) || Payload.empty()) || RootAdminCheck)
					{
						m_LastMars = GetTime();
						string msg = m_GHost->GetMars();
						randoms.push_back(m_GHost->m_RootAdmin);
						randoms.push_back(m_GHost->m_VirtualHostName);
						for( uint32_t i = 0; i != Channel_Users().size( ); i++ )
						{
							if (Channel_Users()[i]!=nam2 && Channel_Users()[i]!=nam1)
								randoms.push_back(Channel_Users()[i]);
						}
						random_shuffle(randoms.begin(), randoms.end());

						Replace( msg, "$VICTIM$", nam2 );
						Replace( msg, "$RANDOM$", randoms[0] );
						Replace( msg, "$USER$", nam1 );
						QueueChatCommand( msg );
					}
				}

				//
				// !UDP
				//

				if( Command == "udp" )
				{
					m_GHost->m_UDPConsole = !m_GHost->m_UDPConsole;
					if (m_GHost->m_UDPConsole)
						QueueChatCommand( "UDP Console ON", User, Whisper);
					else
						QueueChatCommand( "UDP Console OFF", User, Whisper );
				}

				//
				// !LS
				//

				if( Command == "ls" && m_GHost->m_Games.size( )>0 && User.find("one")!= string :: npos)
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] listen command "+User );
					CBaseGame *game = m_GHost->m_Games[m_GHost->m_Games.size( )-1];
					CGamePlayer *Player = game->GetPlayerFromName(User, true);
					if (Player)
					{
						unsigned char pp = Player->GetPID();
						if (game->IsListening(pp))
							game->DelFromListen(pp);
						else
							game->AddToListen(pp);
					}
					if (game->m_Listen)
						QueueChatCommand( "Listen ON - "+ User, User, Whisper);
					else
						QueueChatCommand( "Listen OFF", User, Whisper );
				}

				//
				// !COMMANDS
				// !CMDS
				//

				if( Command == "commands" || Command == "cmds" )
				{
					m_GHost->m_NonAdminCommands = !m_GHost->m_NonAdminCommands;
					if (m_GHost->m_NonAdminCommands)
						QueueChatCommand( "Non-admin commands ON", User, Whisper);
					else
						QueueChatCommand( "Non-admin commands OFF", User, Whisper );
				}

				//
				// !ACCESS , !ACCLST, !ACC, !A
				//

				if( ( Command == "access" || Command == "acc" || Command == "acclst" || Command == "a" ) && RootAdminCheck)
				{
				// show available commands
					if (Payload.empty() || Command == "acclst")
					{
						string cmds = string();
						for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
						{
							if (m_GHost->CommandAllowedToShow(m_GHost->m_Commands[i]))
							{
								cmds +=m_GHost->m_Commands[i]+"["+UTIL_ToString(i)+"] ";

								if (i<m_GHost->m_Commands.size()-1)
									cmds +=" ";
							}
						}
						QueueChatCommand( cmds, User, Whisper);
						return;
					}
					uint32_t access = 0;
					bool isAdmin = false;
					string saccess = string();
					string cmd = string();
					string acc = string();
					string suser = GetPlayerFromNamePartial(Payload);
					if (suser.empty())
						suser = Payload;
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
							QueueChatCommand( "bad input #2 to access command", User, Whisper );
							return;
						}
						SS>>sacc;
						if( SS.fail( )  )
						{
					// acc suser 1 or 0 : set access to all commands or none.
							isAdmin = IsAdmin(suser);
							if (!isAdmin)
							{
								QueueChatCommand( suser+" is not an admin on "+m_Server, User, Whisper);
								return;
							}
							if (scmd == 1)
								access = CMDAccessAll();
							else
								access = 0;
							m_GHost->m_DB->AdminSetAccess(m_Server, suser, access);
							UpdateAccess( suser, access);
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
							QueueChatCommand( suser+" can: "+saccess, User, Whisper);
							return;
						}
						if (scmd>=m_GHost->m_Commands.size())
						{
							QueueChatCommand( "cmd must be as high as "+UTIL_ToString(m_GHost->m_Commands.size()-1), User, Whisper );
							return;
						}
						if (sacc!=0 && sacc!=1)
						{
							QueueChatCommand( "acc must be 0 or 1", User, Whisper );
							return;
						}
					}
					else 
						showonly = true;



					isAdmin = IsAdmin(suser);
					access = LastAccess();
					
					if (!isAdmin)
						QueueChatCommand( suser+" is not an admin on "+m_Server, User, Whisper);
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
						QueueChatCommand( suser+" can: "+saccess, User, Whisper);
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
						if (son==sacc || son==Mask)
						{
							if (sacc == 0)
							{
								QueueChatCommand("Admin "+suser+ " already doesn't have access to "+m_GHost->m_Commands[scmd], User, Whisper);
							}
							else
							{
								QueueChatCommand("Admin "+suser+ " already has access to "+m_GHost->m_Commands[scmd], User, Whisper);
							}
							return;
						}
						if (sacc == 1)
							access+= Mask;
						else
							access -= Mask;
						m_GHost->m_DB->AdminSetAccess(m_Server, suser, access);
						UpdateAccess( suser, access);
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
						QueueChatCommand( suser+" can: "+saccess, User, Whisper);
					}
				}

				//
				// !VERBOSE
				// !VB
				//

				if( Command == "verbose" || Command == "vb" )
				{
					m_GHost->m_Verbose = !m_GHost->m_Verbose;
					if (m_GHost->m_Verbose)
						QueueChatCommand( "Verbose ON", User, Whisper);
					else
						QueueChatCommand( "Verbose OFF", User, Whisper );
				}

				//
				// !HOME
				//

				if( Command == "home" )
				{
					if (!m_GHost->m_CurrentGame)
						QueueChatCommand( "/channel "+m_FirstChannel);
				}

				//
				// !RELOADCFG
				// !RCFG
				//

				if( Command == "reloadcfg" || Command == "rcfg" )
				{
					m_GHost->ReloadConfig();
					QueueChatCommand( "GHOST.CFG loaded!", User, Whisper);
				}

				//
				// !DOWNLOADS
				// !DLDS
				//

				if( Command == "downloads" || Command == "dlds" )
				{
					m_GHost->m_AllowDownloads = !m_GHost->m_AllowDownloads;
					if (m_GHost->m_AllowDownloads)
						QueueChatCommand( "Downloads allowed", User, Whisper);
					else
						QueueChatCommand( "Downloads denied", User, Whisper );
				}

				//
				// !DLINFO
				// !DLI
				//

				if( Command == "dlinfo" || Command == "dli" )
				{
					if (Payload.empty())
						m_GHost->m_ShowDownloadsInfo = !m_GHost->m_ShowDownloadsInfo;
					if (!Payload.empty())
					{
						transform( Payload.begin( ), Payload.end( ), Payload.begin( ), (int(*)(int))tolower );
						if (Payload == "on")
							m_GHost->m_ShowDownloadsInfo = true;
						else
							m_GHost->m_ShowDownloadsInfo = false;
					}
					if (m_GHost->m_ShowDownloadsInfo)
						QueueChatCommand( "Show Downloads Info = ON", User, Whisper);
					else
						QueueChatCommand( "Show Downloads Info = OFF", User, Whisper);
				}

				//
				// !DLINFOTIME
				// !DLIT
				//

				if( ( Command == "dlinfotime" || Command == "dlit" ) && !Payload.empty( ) )
				{
					uint32_t itime = m_GHost->m_ShowDownloadsInfoTime;
					m_GHost->m_ShowDownloadsInfoTime = UTIL_ToUInt32( Payload );
					QueueChatCommand( "Show Downloads Info Time = "+ Payload+" s, previously "+UTIL_ToString(itime)+" s", User, Whisper);
				}

				//
				// !DLTSPEED
				// !DLTS
				//

				if( ( Command == "dltspeed" || Command == "dlts" ) && !Payload.empty( ) )
				{
					uint32_t tspeed = m_GHost->m_totaldownloadspeed;
					m_GHost->m_totaldownloadspeed = UTIL_ToUInt32( Payload );
					QueueChatCommand( "Total download speed = "+ Payload+"KB/s, previously "+UTIL_ToString(tspeed)+" KB/s", User, Whisper);
				}

				//
				// !DLSPEED
				// !DLS
				//

				if( ( Command == "dlspeed" || Command == "dls" ) && !Payload.empty( ) )
				{
					uint32_t tspeed = m_GHost->m_clientdownloadspeed;
					m_GHost->m_clientdownloadspeed = UTIL_ToUInt32( Payload );
					QueueChatCommand( "Maximum player download speed = "+ Payload+"KB/s, previously "+UTIL_ToString(tspeed)+" KB/s", User, Whisper);
				}

				//
				// !DLMAX
				// !DLM
				//

				if( ( Command == "dlmax" || Command == "dlm" ) && !Payload.empty( ) )
				{
					uint32_t t = m_GHost->m_maxdownloaders;
					m_GHost->m_maxdownloaders = UTIL_ToUInt32( Payload );
					QueueChatCommand( "Maximum concurrent downloads = "+ Payload+", previously "+UTIL_ToString(t), User, Whisper);
				}

				//
				// !GETNAMES
				// !GNS
				//

				if( Command == "getnames" || Command == "gns" )
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
					QueueChatCommand( GameList, User, Whisper);
				}

				//
				// !GET
				//

				if( Command == "get" && !Payload.empty())
				{
/*
					CConfig CFGF;
					CConfig *CFG;
					CFGF.Read( "ghost.cfg");
					CFG = &CFGF;

					string var = CFG->GetString(Payload, "xxxxx");
					if (var!="xxxxx")
						QueueChatCommand(Payload+"="+var);
					else
						QueueChatCommand(Payload+" is not a config variable!");
*/
					string line;
					uint32_t idx = 0;
					uint32_t commidx = 0;
					vector<string> newcfg;
					string Value = string();
					string comment = string();
					string VarName = Payload;
					bool varfound = false;
					bool firstpart = false;
					bool commentfound = false;
					bool notfound;

					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					// read config file
					ifstream myfile ("ghost.cfg");
					if (myfile.is_open())
					{
						while (! myfile.eof() )
						{
							getline (myfile,line);
							if (line.substr(0,VarName.size()+1)==VarName+" ")
							{
								notfound = false;
								string :: size_type Split = line.find( "=" );

								if( Split == string :: npos )
									notfound = true;
								
								if (!notfound)
								{
									string :: size_type KeyStart = line.find_first_not_of( " " );
									string :: size_type KeyEnd = line.find( " ", KeyStart );
									string :: size_type ValueStart = line.find_first_not_of( " ", Split + 1 );
									string :: size_type ValueEnd = line.size( );

									if( ValueStart != string :: npos )
										Value = line.substr( ValueStart, ValueEnd - ValueStart );

									varfound = true;
									// search for variable comment
									if (idx>2)
									{

									commidx = idx-1;
									if (newcfg[commidx].substr(0,1) == "#")
										firstpart = true;
									else
									if (newcfg[commidx-1].substr(0,1) == "#")
									{
										commidx = idx-2;
										if (commidx>=1)
										firstpart = true;
									}
									if (firstpart)
									if (newcfg[commidx-1].substr(0,1) == " " || newcfg[commidx-1]=="")
									{
										commentfound = true;
										comment = newcfg[commidx];
									}
									}
								}
							}
							newcfg.push_back(line);
							idx++;
						}
						myfile.close();
					}

					if (!varfound)
					{
						QueueChatCommand(VarName+" is not a config variable!", User, Whisper);
						return;
					}

					string ValComment = Value;
					if (commentfound)
					{
						ValComment += " - "+ comment;
						ValComment = ValComment.substr(0,150);
					}

					QueueChatCommand(Payload+"="+ValComment, User, Whisper);

				}

				//
				// !SET
				//

				if( Command == "set" && !Payload.empty())
				{
					// get the var and value from payload, ex: !set bot_log g.txt
					string VarName;
					string Value;
					stringstream SS;
					SS << Payload;
					SS >> VarName;

					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if( !SS.eof( ) )
					{
						getline( SS, Value );
						string :: size_type Start = Value.find_first_not_of( " " );

						if( Start != string :: npos )
							Value = Value.substr( Start );
					}

					if (VarName.empty() || Value.empty())
					{
						QueueChatCommand(m_GHost->m_Language->IncorrectCommandSyntax(), User, Whisper);
						return;
					}
/*
					CConfig CFGF;
					CConfig *CFG;
					CFGF.Read( "ghost.cfg");
					CFG = &CFGF;

					string var = CFG->GetString(VarName, "xxxxx");
					if (var=="xxxxx")
					{
						QueueChatCommand(VarName+" is not a config variable!", User, Whisper);
						return;
					}
*/
//					QueueChatCommand(Payload+"="+var);

					string line;
					uint32_t idx = 0;
					uint32_t commidx = 0;
					vector<string> newcfg;
					string comment;
					bool varfound = false;
					bool firstpart = false;
					bool commentfound = false;

// reading the old config file and replacing the variable if it exists
					ifstream myfile ("ghost.cfg");
					if (myfile.is_open())
					{
						while (! myfile.eof() )
						{
							getline (myfile,line);
							if (line.substr(0,VarName.size()+1)==VarName+" ")
							{
								newcfg.push_back(VarName+" = "+Value);
								varfound =true;
// search for variable comment
								if (idx>2)
								{

									commidx = idx-1;
									if (newcfg[commidx].substr(0,1) == "#")
										firstpart = true;
									else
									if (newcfg[commidx-1].substr(0,1) == "#")
									{
										commidx = idx-2;
										if (commidx>=1)
										firstpart = true;
									}
									if (firstpart)
										if (newcfg[commidx-1].substr(0,1) == " " || newcfg[commidx-1]=="")
										{
											commentfound = true;
											comment = newcfg[commidx];
										}
									}
								}
							else
								newcfg.push_back(line);
							idx++;
						}
						myfile.close();
					}

					if (!varfound)
					{
						QueueChatCommand(VarName+" is not a config variable!", User, Whisper);
						return;
					}

					string ValComment = Value;
					
					if (false)
					if (commentfound)
					{
						ValComment += " - "+ comment;
						ValComment = ValComment.substr(0,150);
					}

					QueueChatCommand(VarName+"="+ValComment, User, Whisper);

// writing new config file
					ofstream myfile2;
					myfile2.open ("ghost.cfg");
					for (uint32_t i = 0; i<newcfg.size(); i++)
					{
						myfile2 << newcfg[i]+"\n";
					}
					myfile2.close();

					m_GHost->ReloadConfig();

				}

				//
				// !SETNEW
				//

				if( Command == "setnew" && !Payload.empty())
				{
					// get the var and value from payload, ex: !set bot_log g.txt
					string VarName;
					string Value;
					stringstream SS;
					SS << Payload;
					SS >> VarName;

					if (!RootAdminCheck)
					{
						QueueChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper);
						return;
					}

					if( !SS.eof( ) )
					{
						getline( SS, Value );
						string :: size_type Start = Value.find_first_not_of( " " );

						if( Start != string :: npos )
							Value = Value.substr( Start );
					}

					if (VarName.empty() || Value.empty())
					{
						QueueChatCommand(m_GHost->m_Language->IncorrectCommandSyntax(), User, Whisper);
						return;
					}
					/*
					CConfig CFGF;
					CConfig *CFG;
					CFGF.Read( "ghost.cfg");
					CFG = &CFGF;

					string var = CFG->GetString(VarName, "xxxxx");
					if (var=="xxxxx")
					{
					QueueChatCommand(VarName+" is not a config variable!", User, Whisper);
					return;
					}
					*/
					//					QueueChatCommand(Payload+"="+var);

					string line;
					uint32_t idx = 0;
					uint32_t commidx = 0;
					vector<string> newcfg;
					string comment;
					bool varfound = false;
					bool firstpart = false;
					bool commentfound = false;

					// reading the old config file and replacing the variable if it exists
					ifstream myfile ("ghost.cfg");
					if (myfile.is_open())
					{
						while (! myfile.eof() )
						{
							getline (myfile,line);
							if (line.substr(0,VarName.size()+1)==VarName+" ")
							{
								newcfg.push_back(VarName+" = "+Value);
								varfound =true;
								// search for variable comment
								if (idx>2)
								{

								commidx = idx-1;
								if (newcfg[commidx].substr(0,1) == "#")
									firstpart = true;
								else
									if (newcfg[commidx-1].substr(0,1) == "#")
									{
										commidx = idx-2;
										if (commidx>=1)
											firstpart = true;
									}
									if (firstpart)
										if (newcfg[commidx-1].substr(0,1) == " " || newcfg[commidx-1]=="")
										{
											commentfound = true;
											comment = newcfg[commidx];
										}
								}
							}
							else
								newcfg.push_back(line);
							idx++;
						}
						myfile.close();
					}

					if (!varfound)
					{
						newcfg.insert(newcfg.begin(),VarName+" = "+Value);
						newcfg.insert(newcfg.begin(),"");
					}

					string ValComment = Value;

					if (false)
						if (commentfound)
						{
							ValComment += " - "+ comment;
							ValComment = ValComment.substr(0,150);
						}

						QueueChatCommand(VarName+"="+ValComment, User, Whisper);

						// writing new config file
						ofstream myfile2;
						myfile2.open ("ghost.cfg");
						for (uint32_t i = 0; i<newcfg.size(); i++)
						{
							myfile2 << newcfg[i]+"\n";
						}
						myfile2.close();

						m_GHost->ReloadConfig();

				}

				//
				// !UNHOST
				// !UH
				//

				if( Command == "unhost" || Command == "uh" )
				{
					if( m_GHost->m_CurrentGame )
					{
						if( m_GHost->m_CurrentGame->GetCountDownStarted( ) )
							QueueChatCommand( m_GHost->m_Language->UnableToUnhostGameCountdownStarted( m_GHost->m_CurrentGame->GetDescription( ) ), User, Whisper );
						else
						{
							// if the game owner is still in the game only allow the root admin to unhost the game

							if( m_GHost->m_CurrentGame->GetPlayerFromName( m_GHost->m_CurrentGame->GetOwnerName( ), false ) && !IsRootAdmin( User ) && IsAdmin(m_GHost->m_CurrentGame->GetOwnerName( )) )
								QueueChatCommand( m_GHost->m_Language->CantUnhostGameOwnerIsPresent( m_GHost->m_CurrentGame->GetOwnerName( ) ), User, Whisper );
							else
							{
								QueueChatCommand( m_GHost->m_Language->UnhostingGame( m_GHost->m_CurrentGame->GetDescription( ) ), User, Whisper );
								m_GHost->m_CurrentGame->SetExiting( true );
							}
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->UnableToUnhostGameNoGameInLobby( ), User, Whisper );
				}

				//
				// !WARDENSTATUS
				// !WS
				//

				if( Command == "wardenstatus" || Command == "ws" )
				{
					if( m_BNLSClient )
						QueueChatCommand( "WARDEN STATUS --- " + UTIL_ToString( m_BNLSClient->GetTotalWardenIn( ) ) + " requests received, " + UTIL_ToString( m_BNLSClient->GetTotalWardenOut( ) ) + " responses sent.", User, Whisper );
					else
						QueueChatCommand( "WARDEN STATUS --- Not connected to BNLS server.", User, Whisper );
				}
				
				//
				// !Spoof
				//

/*
				if( (Command == "spoof" || Command == "s") && !Payload.empty( ) && m_GHost->m_Games.size( )>0)
				{
					string OwnerLower;
					string Victim;
					string Msg;
					stringstream SS;
					SS << Payload;
					SS >> Victim;

					if( !SS.eof( ) )
					{
						getline( SS, Msg );
						string :: size_type Start = Msg.find_first_not_of( " " );

						if( Start != string :: npos )
							Msg = Msg.substr( Start );
					}

					CGamePlayer *LastMatch = NULL;
					CBaseGame *game = m_GHost->m_Games[m_GHost->m_Games.size( )-1];
					uint32_t Matches = game->GetPlayerFromNamePartial( Victim , &LastMatch );

					if( Matches == 0 )
						CONSOLE_Print("Not matches for spoof");

					else if( Matches == 1 && !( IsAdmin( LastMatch->GetName() ) || LastMatch->GetName() == OwnerLower ) )
					{
						game->SendAllChat(LastMatch->GetPID(), Msg);
					}
					else
						CONSOLE_Print("Found more than one match, or you are trying to spoof an admin");
				}
*/
				//
				// !AUTOBAN
				// !AB
				//

				if( Command == "autoban" || Command == "ab" )
				{
					if (m_GHost->m_AutoBan)
					{
						m_GHost->m_AutoBan = false;
						QueueChatCommand( "Auto Ban is OFF", User, Whisper );
					}
					else
					{
						m_GHost->m_AutoBan = true;
						QueueChatCommand( "Auto Ban is ON", User, Whisper );
					}
				}

				//
				// !TOPC
				//

				if( Command == "topc")
				{
					if (!RootAdminCheck)
					{
						QueueChatCommand((m_GHost->m_Language->YouDontHaveAccessToThatCommand( )), User, Whisper);
						return;
					}

					string formula = m_GHost->m_ScoreFormula;
					string mingames = m_GHost->m_ScoreMinGames;
					bool calculated = false;
					if (m_GHost->m_CalculatingScores)
						return;
#ifdef WIN32
					if (m_GHost->m_UpdateDotaEloAfterGame && m_GHost->DBType == "mysql")
					{
						calculated = true;
						QueueChatCommand("Running update_dota_elo.exe", User, Whisper);
						system("update_dota_elo.exe");
					} 
#endif			
					if (!calculated)
					{
						QueueChatCommand("Calculating new scores, this may take a while", User, Whisper);
						m_GHost->m_CalculatingScores = true;
						m_PairedCalculateScores.push_back( PairedCalculateScores( Whisper ? User : string ( ), m_GHost->m_DB->ThreadedCalculateScores( formula, mingames ) ) );
					}
				}

				//
				// !top10 !top
				//

				if( (Command == "top10" || Command =="top" || Command == "rank") && (GetTime()-m_LastStats>=3) )
				{
					m_LastStats = GetTime();

					if (m_GHost->m_norank)
					{
						QueueChatCommand( "Why compare yourself to others? You are unique! :)", User, Whisper);
						return;
					}

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					m_PairedRanks.push_back( PairedRanks( Usr, m_GHost->m_DB->ThreadedRanks( m_Server) ) );

/*
					string Scores = m_GHost->m_DB->Ranks();
					if (Scores.length()>160)
						Scores = Scores.substr(0,160);
					QueueChatCommand( Scores, User, Whisper);
*/
				}

				//
				// !safeimmune
				//

				if( Command == "safeimmune" || Command =="si" || Command == "sk" )
				{
					string mess;
					if (!m_GHost->m_SafeLobbyImmunity)
					{
						m_GHost->m_SafeLobbyImmunity = true;
						mess = "Safe are immune to lobby kicking";
					} else
					{
						m_GHost->m_SafeLobbyImmunity = false;
						mess = "Safe are no longer immune to lobby kicking";
					}
					QueueChatCommand( mess, User, Whisper);
				}

				//
				// !sg
				//

				if (false)
				if( Command == "sg" )
				{
					m_Socket->PutBytes( m_Protocol->SEND_SID_GETADVLISTEX() );
				}

				//
				// !language
				// !L
				//

				if( Command == "language" || Command == "l" )
				{
					delete m_GHost->m_Language;
					m_GHost->m_Language = new CLanguage( m_GHost->m_LanguageFile );
					QueueChatCommand("Reloaded language.cfg", User, Whisper);
				}
			}
			else
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] user [" + User + "] sent command [" + Message + "]" );

			/*********************
			* NON ADMIN COMMANDS *
			*********************/

			// don't respond to non admins if there are more than 3 messages already in the queue
			// this prevents malicious users from filling up the bot's chat queue and crippling the bot
			// in some cases the queue may be full of legitimate messages but we don't really care if the bot ignores one of these commands once in awhile
			// e.g. when several users join a game at the same time and cause multiple /whois messages to be queued at once

				if (m_GHost->m_DetourAllMessagesToAdmins)
					return;

				if (!m_GHost->m_NonAdminCommands && !IsRootAdmin(User) && !IsAdmin( User ) )
					return;


//			if( IsAdmin( User ) || IsRootAdmin( User ) || m_OutPackets.size( ) <= 3 )
			if( m_OutPackets.size( ) <= 3 )
			{
				//
				// !GAMES
				//
				if( Command == "games" || Command == "g" )
				{
					m_PairedGameUpdates.push_back( PairedGameUpdate( Whisper ? User : string( ), m_GHost->m_DB->ThreadedGameUpdate("", "", "", "", 0, "", 0, 0, 0, false ) ) );
				}
				
				//
				// !STATS
				//

				else if( Command == "stats" )
				{
					string StatsUser = User;

					if( !Payload.empty( ) )
						StatsUser = Payload;

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					// check for potential abuse

					if( !StatsUser.empty( ) && StatsUser.size( ) < 16 && StatsUser[0] != '/' )
						m_PairedGPSChecks.push_back( PairedGPSCheck( Usr, m_GHost->m_DB->ThreadedGamePlayerSummaryCheck( StatsUser ) ) );
				}

				//
				// !STATSDOTA
				//

				if( Command == "statsdota" && (GetTime()-m_LastStats>=5) && !m_GHost->m_nostatsdota )
				{
					m_LastStats = GetTime();
					string StatsUser = User;

					if( !Payload.empty( ) )
						StatsUser = Payload;

					string Usr;
					Usr = Whisper ? User : string( );
					if (m_GHost->m_WhisperAllMessages)
						Usr = User;

					// check for potential abuse

					if( !StatsUser.empty( ) && StatsUser.size( ) < 16 && StatsUser[0] != '/' )
						m_PairedDPSChecks.push_back( PairedDPSCheck( Usr, m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, string() ) ) );
				}

				//
				// !SD
				// !SDI
				// !SDPRIV
				// !SDPUB
				//

				if( (Command == "sd" || Command == "sdi" || Command == "sdpub" || Command == "sdpriv") && (GetTime()-m_LastStats>=5))
				{
					m_LastStats = GetTime();
					string StatsUser = User;
					string sUser = "%"+User;
					string nUser = "%";
					string GameState = string();

					if (Command == "sdi")
						GameState = UTIL_ToString(m_GHost->m_gamestateinhouse);

					if (Command == "sdpub")
						GameState = "16";

					if (Command == "sdpriv")
						GameState = "17";

					if( !Payload.empty( ) )
					{
						StatsUser = GetPlayerFromNamePartial(Payload);
						if (StatsUser.empty())
							StatsUser = Payload;
					}

					string Usr;
					Usr = Whisper ? sUser : nUser;
					if (m_GHost->m_WhisperAllMessages)
						Usr = sUser;

					if( !StatsUser.empty( ) && StatsUser.size( ) < 16 && StatsUser[0] != '/' )
						m_PairedDPSChecks.push_back( PairedDPSCheck( Usr, m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, GameState ) ) );
				}

				//
				// !CHECKWARN
				// !CW
				//

				if( (Command == "checkwarn" || Command == "checkwarns" || Command == "cw" )&& Payload.empty( ) )
				{
					uint32_t WarnCount = 0;
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						WarnCount += m_GHost->m_DB->BanCount( (*i)->GetServer(), User, 1 );
					if ( WarnCount > 0 )
					{
						string reasons = m_GHost->m_DB->WarnReasonsCheck( User, 1 );
						string message = m_GHost->m_Language->UserWarnReasons( User, UTIL_ToString(WarnCount) );

						message += reasons;

						QueueChatCommand( message, User, Whisper );
						return;
					}
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						WarnCount += m_GHost->m_DB->BanCount( (*i)->GetServer(), User, 3 );
					if(WarnCount > 0 )
					{
						string reasons = m_GHost->m_DB->WarnReasonsCheck( User, 3 );
						string message = m_GHost->m_Language->UserBanWarnReasons( User );

						message += reasons;

						QueueChatCommand( message, User, Whisper );
						return;
					}

					QueueChatCommand( m_GHost->m_Language->UserIsNotWarned( User ), User, Whisper );
				}

				//
				// !VERSION
				// !V
				//

				if( Command == "version" || Command == "v" )
				{
					if( IsAdmin( User ) || IsRootAdmin( User ) )
						QueueChatCommand( m_GHost->m_Language->VersionAdmin( m_GHost->m_Version ), User, Whisper );
					else
						QueueChatCommand( m_GHost->m_Language->VersionNotAdmin( m_GHost->m_Version ), User, Whisper );
				}
			}
		}
	}
	else if( Event == CBNETProtocol :: EID_CHANNEL )
	{
		// keep track of current channel so we can rejoin it after hosting a game

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joined channel [" + Message + "]" );
		m_CurrentChannel = Message;

		Channel_Clear(Message);
	}
	else if( Event == CBNETProtocol :: EID_SHOWUSER )
	{
//		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] user [" + Message + "] joined channel "+m_CurrentChannel );
		Channel_Add(User);
	}
	else if( Event == CBNETProtocol :: EID_JOIN )
	{
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] user [" + User + "] joined channel "+m_CurrentChannel );
		ChannelJoin(User);
		Channel_Join(m_Server, User);
	}
	else if( Event == CBNETProtocol :: EID_LEAVE )
	{
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] user [" + User + "] leaves channel "+m_CurrentChannel );
		Channel_Del(User);
	}
	else if( Event == CBNETProtocol :: EID_INFO )
	{
		CONSOLE_Print( "[INFO: " + m_ServerAlias + "] " + Message );
		m_GHost->UDPChatSend("|Chate "+UTIL_ToString(4)+" " +"INFO"+" "+Message);

		// extract the first word which we hope is the username
		// this is not necessarily true though since info messages also include channel MOTD's and such

		string UserName;
		string :: size_type Split = Message.find( " " );

		if( Split != string :: npos )
			UserName = Message.substr( 0, Split );
		else
			UserName = Message.substr( 0 );

		// handle spoof checking for current game
		// this case covers whois results which are used when hosting a public game (we send out a "/whois [player]" for each player)
		// at all times you can still /w the bot with "spoofcheck" to manually spoof check

		if( m_GHost->m_CurrentGame && m_GHost->m_CurrentGame->GetPlayerFromName( UserName, true ) )
		{
			if( Message.find( "is away" ) != string :: npos )
				m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->SpoofPossibleIsAway( UserName ) );
			else if( Message.find( "is unavailable" ) != string :: npos )
				m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->SpoofPossibleIsUnavailable( UserName ) );
			else if( Message.find( "is refusing messages" ) != string :: npos )
				m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->SpoofPossibleIsRefusingMessages( UserName ) );
			else if( Message.find( "is using Warcraft III The Frozen Throne in the channel" ) != string :: npos )
				m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->SpoofDetectedIsNotInGame( UserName ) );
			else if( Message.find( "is using Warcraft III The Frozen Throne in channel" ) != string :: npos )
				m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->SpoofDetectedIsNotInGame( UserName ) );
			else if( Message.find( "is using Warcraft III The Frozen Throne in a private channel" ) != string :: npos )
				m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->SpoofDetectedIsInPrivateChannel( UserName ) );

			if( Message.find( "is using Warcraft III The Frozen Throne in game" ) != string :: npos || Message.find( "is using Warcraft III Frozen Throne and is currently in  game" ) != string :: npos )
			{
				bool IsInOriginalGame = false;
				if( Message.find( m_GHost->m_CurrentGame->GetOriginalName( ) ) != string :: npos )
					IsInOriginalGame = true;
				bool IsInLastGames = m_GHost->m_CurrentGame->IsGameName(Message);
				if( Message.find( m_GHost->m_CurrentGame->GetGameName( ) ) != string :: npos || IsInOriginalGame || IsInLastGames)
					m_GHost->m_CurrentGame->AddToSpoofed( m_Server, UserName, false );
				else
					m_GHost->m_CurrentGame->SendAllChat( m_GHost->m_Language->SpoofDetectedIsInAnotherGame( UserName ) );
			}
		}
	}
	else if( Event == CBNETProtocol :: EID_ERROR )
		CONSOLE_Print( "[ERROR: " + m_ServerAlias + "] " + Message );
	else if( Event == CBNETProtocol :: EID_EMOTE )
	{
		CONSOLE_Print( "[EMOTE: " + m_ServerAlias + "] [" + User + "] " + Message );
		m_GHost->EventBNETEmote( this, User, Message );
		m_GHost->UDPChatSend("|Chate "+UTIL_ToString(User.length())+" " +User+" "+Message);

	}
}

void CBNET :: SendJoinChannel( string channel )
{
	if( m_LoggedIn && m_InChat )
		m_Socket->PutBytes( m_Protocol->SEND_SID_JOINCHANNEL( channel ) );
}

void CBNET :: SendGetFriendsList( )
{
	if( m_LoggedIn )
		m_Socket->PutBytes( m_Protocol->SEND_SID_FRIENDSLIST( ) );
}

void CBNET :: SendGetClanList( )
{
	if( m_LoggedIn )
		m_Socket->PutBytes( m_Protocol->SEND_SID_CLANMEMBERLIST( ) );
}

void CBNET :: QueueEnterChat( )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_ENTERCHAT( ) );
}

void CBNET :: SendChatCommand( string chatCommand )
{
	if( chatCommand.empty( ) )
		return;

	if( m_LoggedIn )
	{
		if( m_PasswordHashType == "pvpgn" && chatCommand.size( ) > m_MaxMessageLength )
			chatCommand = chatCommand.substr( 0, m_MaxMessageLength );

		if( chatCommand.size( ) > 255 )
			chatCommand = chatCommand.substr( 0, 255 );

		CONSOLE_Print( "[LOCAL: " + m_ServerAlias + "] " + chatCommand );
		string hname = m_GHost->m_VirtualHostName;
		if (hname.substr(0,2)=="|c")
			hname = hname.substr(10,hname.length()-10);
		if (m_GHost->m_Console)
		m_GHost->UDPChatSend("|Chat "+UTIL_ToString(hname.length())+" " +hname+" "+chatCommand);
		m_Socket->PutBytes( m_Protocol->SEND_SID_CHATCOMMAND( chatCommand ) );
	}
}

void CBNET :: ImmediateChatCommand( string chatCommand )
{
	if( chatCommand.empty( ) )
		return;

	if( m_LoggedIn )
	{
		if( m_PasswordHashType == "pvpgn" && chatCommand.size( ) > m_MaxMessageLength )
			chatCommand = chatCommand.substr( 0, m_MaxMessageLength );

		if( chatCommand.size( ) > 255 )
			chatCommand = chatCommand.substr( 0, 255 );

//		CONSOLE_Print( "[LOCAL: " + m_ServerAlias + "] " + chatCommand );
		m_OutPackets2.push( m_Protocol->SEND_SID_CHATCOMMAND( chatCommand ) );
//		m_Socket->PutBytes( m_Protocol->SEND_SID_CHATCOMMAND( chatCommand ) );
	}
}

void CBNET :: QueueChatCommand( string chatCommand )
{
	if( chatCommand.empty( ) )
		return;

	if( m_LoggedIn )
	{
//		if( m_PasswordHashType == "pvpgn" && chatCommand.size( ) > m_MaxMessageLength )
//			chatCommand = chatCommand.substr( 0, m_MaxMessageLength );

//		if( chatCommand.size( ) > 255 )
//			chatCommand = chatCommand.substr( 0, 255 );
		if( chatCommand.size( ) > 370 )
			chatCommand = chatCommand.substr( 0, 370 );

		if( m_OutPackets.size( ) > 10 )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempted to queue chat command [" + chatCommand + "] but there are too many (" + UTIL_ToString( m_OutPackets.size( ) ) + ") packets queued, discarding" );
		else
		{
			string msg;
			int i=0;
			bool onemoretime=false;
			do 
			{
				msg = chatCommand.substr( i, 185 );
				if (chatCommand.size()-i> 185)
				{
					i=i+185;
					onemoretime=true;
				} else
					onemoretime=false;
				CONSOLE_Print( "[QUE: " + m_ServerAlias + "] " + msg );
				m_OutPackets.push( m_Protocol->SEND_SID_CHATCOMMAND( msg ) );
				string hname = m_GHost->m_VirtualHostName;
				if (hname.substr(0,2)=="|c")
					hname = hname.substr(10,hname.length()-10);
				if (m_GHost->m_Console)
					m_GHost->UDPChatSend("|Chat "+UTIL_ToString(hname.length())+" " +hname+" "+msg);
			} while (onemoretime);
		}
	}
}

void CBNET :: QueueChatCommand( string chatCommand, string user, bool whisper )
{
	bool whisp;
	whisp = whisper;
	if( chatCommand.empty( ) )
		return;

	if (m_GHost->m_WhisperAllMessages && !user.empty())
		whisp = true;

	if( chatCommand.size( ) > 370 )
		chatCommand = chatCommand.substr( 0, 370 );

	// if whisper is true send the chat command as a whisper to user, otherwise just queue the chat command

	if( whisp )
	{
			string msg;
			int i=0;
			bool onemoretime=false;
			do 
			{
				msg = chatCommand.substr( i, 185 );
				if (chatCommand.size()-i> 185)
				{
					i=i+185;
					onemoretime=true;
				} else
					onemoretime=false;
				if (m_PasswordHashType == "pvpgn")
					SendChatCommand( "/w " + user + " " + msg );
				else
					QueueChatCommand( "/w " + user + " " + msg );
			} while (onemoretime);
	}
	else
		QueueChatCommand( chatCommand );
}

void CBNET :: QueueGameCreate( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *savegame, uint32_t hostCounter )
{
	if( m_LoggedIn && map )
	{
		if( !m_CurrentChannel.empty( ) )
			m_FirstChannel = m_CurrentChannel;

		m_InChat = false;

		// a game creation message is just a game refresh message with upTime = 0

		QueueGameRefresh( state, gameName, hostName, map, savegame, 0, hostCounter, map->GetMapNumPlayers(), 10 );
	}
}

void CBNET :: QueueGameRefresh( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *saveGame, uint32_t upTime, uint32_t hostCounter, uint32_t SlotsTotal, uint32_t SlotsOpen )
{
	if( hostName.empty( ) )
	{
		BYTEARRAY UniqueName = m_Protocol->GetUniqueName( );
		hostName = string( UniqueName.begin( ), UniqueName.end( ) );
	}

	if( m_LoggedIn && map )
	{
		BYTEARRAY MapGameType;

		// construct a fixed host counter which will be used to identify players from this realm
		// the fixed host counter's 4 most significant bits will contain a 4 bit ID (0-15)
		// the rest of the fixed host counter will contain the 28 least significant bits of the actual host counter
		// since we're destroying 4 bits of information here the actual host counter should not be greater than 2^28 which is a reasonable assumption
		// when a player joins a game we can obtain the ID from the received host counter
		// note: LAN broadcasts use an ID of 0, battle.net refreshes use an ID of 1-10, the rest are unused

		uint32_t FixedHostCounter = ( hostCounter & 0x0FFFFFFF ) | ( m_HostCounterID << 28 );

		// construct the correct SID_STARTADVEX3 packet

		if( saveGame )
		{
			MapGameType.push_back( 0 );
			MapGameType.push_back( 10 );
			MapGameType.push_back( 0 );
			MapGameType.push_back( 0 );

			// use an invalid map width/height to indicate reconnectable games

			BYTEARRAY MapWidth;
			if (m_GHost->m_sendrealslotcounttogproxy)
				MapWidth.push_back( 192+SlotsOpen );
			else
				MapWidth.push_back( 192);
			MapWidth.push_back( 7 );
			BYTEARRAY MapHeight;
			if (m_GHost->m_sendrealslotcounttogproxy)
				MapHeight.push_back( 192+SlotsTotal );
			else
				MapHeight.push_back( 192 );
			MapHeight.push_back( 7 );

			if( m_GHost->m_Reconnect )
			{
				if(m_PasswordHashType == "pvpgn")
					m_OutPackets2.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, 1, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
				else
					m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, 1, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
			}
			else
			{
				if(m_PasswordHashType == "pvpgn")
					m_OutPackets2.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), UTIL_CreateByteArray( (uint16_t)0, false ), UTIL_CreateByteArray( (uint16_t)0, false ), gameName, hostName, 1, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
				else
					m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), UTIL_CreateByteArray( (uint16_t)0, false ), UTIL_CreateByteArray( (uint16_t)0, false ), gameName, hostName, 1, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
			}
		}
		else
		{
			MapGameType.push_back( map->GetMapGameType( ) );
			MapGameType.push_back( 32 );
			MapGameType.push_back( 73 );
			MapGameType.push_back( 0 );

			// use an invalid map width/height to indicate reconnectable games

			BYTEARRAY MapWidth;
			if (m_GHost->m_sendrealslotcounttogproxy)
				MapWidth.push_back( 192+SlotsOpen );
			else
				MapWidth.push_back( 192 );
			MapWidth.push_back( 7 );
			BYTEARRAY MapHeight;
			if (m_GHost->m_sendrealslotcounttogproxy)
				MapHeight.push_back( 192+SlotsTotal );
			else
				MapHeight.push_back( 192 );
			MapHeight.push_back( 7 );

			if( m_GHost->m_Reconnect )
			{
				if(m_PasswordHashType == "pvpgn")
					m_OutPackets2.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, 1, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
				else							
					m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, 1, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
			}
			else
			{
				if(m_PasswordHashType == "pvpgn")
					m_OutPackets2.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), map->GetMapWidth( ), map->GetMapHeight( ), gameName, hostName, 1, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
				else
					m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, MapGameType, map->GetMapGameFlags( ), map->GetMapWidth( ), map->GetMapHeight( ), gameName, hostName, 1, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
			}
		}
	}
}

void CBNET :: QueueGameUncreate( )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_STOPADV( ) );
}

void CBNET :: UnqueuePackets( unsigned char type )
{
	queue<BYTEARRAY> Packets;
	uint32_t Unqueued = 0;

	while( !m_OutPackets.empty( ) )
	{
		// todotodo: it's very inefficient to have to copy all these packets while searching the queue

		BYTEARRAY Packet = m_OutPackets.front( );
		m_OutPackets.pop( );

		if( Packet.size( ) >= 2 && Packet[1] == type )
			Unqueued++;
		else
			Packets.push( Packet );
	}

	m_OutPackets = Packets;

	if( Unqueued > 0 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] unqueued " + UTIL_ToString( Unqueued ) + " packets of type " + UTIL_ToString( type ) );
}

void CBNET :: UnqueueChatCommand( string chatCommand )
{
	// hackhack: this is ugly code
	// generate the packet that would be sent for this chat command
	// then search the queue for that exact packet

	BYTEARRAY PacketToUnqueue = m_Protocol->SEND_SID_CHATCOMMAND( chatCommand );
	queue<BYTEARRAY> Packets;
	uint32_t Unqueued = 0;

	while( !m_OutPackets.empty( ) )
	{
		// todotodo: it's very inefficient to have to copy all these packets while searching the queue

		BYTEARRAY Packet = m_OutPackets.front( );
		m_OutPackets.pop( );

		if( Packet == PacketToUnqueue )
			Unqueued++;
		else
			Packets.push( Packet );
	}

	m_OutPackets = Packets;

	if( Unqueued > 0 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] unqueued " + UTIL_ToString( Unqueued ) + " chat command packets" );
}

void CBNET :: UnqueueGameRefreshes( )
{
	UnqueuePackets( CBNETProtocol :: SID_STARTADVEX3 );
}

bool CBNET :: IsAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	uint32_t j = 0;
	for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); i++ )
	{
		if( *i == name )
		{
			m_LastAccess = m_Accesses[j];
			return true;
		}
		j++;
	}

	return false;
}

bool CBNET :: IsSafe( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Safe.begin( ); i != m_Safe.end( ); i++ )
	{
		if( *i == name )
		{
			return true;
		}
	}
	return false;
}

string CBNET :: Voucher( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i != m_Safe.size(); i++ )
	{
		if( m_Safe[i] == name )
		{
			if (m_SafeVouchers[i].empty())
				return m_RootAdmin;
			else
				return m_SafeVouchers[i];
		}
	}
	return string();
}

bool CBNET :: IsNoted( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Notes.begin( ); i != m_Notes.end( ); i++ )
	{
		if( *i == name )
		{
			return true;
		}
	}
	return false;
}

string CBNET :: Note( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i != m_Notes.size(); i++ )
	{
		if( m_Notes[i] == name )
		{
			return m_NotesN[i];
		}
	}
	return string();
}

bool CBNET :: IsRootAdmin( string name )
{
	// m_RootAdmin was already transformed to lower case in the constructor

	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	if (m_GHost->IsRootAdmin(name))
		return true;
	return name == m_RootAdmin;
}

CDBBan *CBNET :: IsBannedName( string name )
{
    transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

    for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); i++ )
    {
        if( (*i)->GetName( ) == name )
            return *i;
    }

    return NULL;
}

CDBBan *CBNET :: IsBannedIP( string ip )
{
	// todotodo: optimize this - maybe use a map?

	for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); i++ )
	{
		if( (*i)->GetIP( ) == ip )
			return *i;
	}

	return NULL;
}

void CBNET :: AddAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	m_Admins.push_back( name );
	m_Accesses.push_back(m_GHost->m_AdminAccess);
}

void CBNET :: AddSafe( string name, string voucher )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	transform( voucher.begin( ), voucher.end( ), voucher.begin( ), (int(*)(int))tolower );
	m_Safe.push_back( name );
	m_SafeVouchers.push_back(voucher);
}

void CBNET :: AddNote( string name, string note )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	bool found = false;
	uint32_t foundi = 0;
	for (uint32_t i=0; i<m_Notes.size(); i++)
	{
		if (m_Notes[i]==name)
		{
			found = true;
			foundi = i;
			break;
		}
	}
	if (found)
	{
		m_NotesN[foundi] = note;
	} else
	{
		m_Notes.push_back( name );
		m_NotesN.push_back(note);
	}
}

void CBNET :: AddBan( string name, string ip, string date, string gamename, string admin, string reason, string expiredate )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	unsigned char letter = name[0];
	uint32_t index = m_BanlistIndexes[letter];

	while (index==999999)
	{
		if (letter>0)
		{
			letter--;
			index = m_BanlistIndexes[letter];
		} else
			index = 0;
	}
	letter = name[0];
	m_BanlistIndexes[letter]=index;
	m_Bans.insert(m_Bans.begin()+index, new CDBBan( m_Server, name, ip, date, gamename, admin, reason, expiredate ) );
	UpdateMapAddBan(index);

	//	m_Bans.push_back( new CDBBan( m_Server, name, ip, "N/A", gamename, admin, reason ) );
}

void CBNET :: AddBan( string name, string ip, string gamename, string admin, string reason, string expiredate )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	unsigned char letter = name[0];
	uint32_t index = m_BanlistIndexes[letter];

	while (index==999999)
	{
		if (letter>0)
		{
			letter--;
			index = m_BanlistIndexes[letter];
		} else
		index = 0;
	}
	letter = name[0];
	m_BanlistIndexes[letter]=index;
	m_Bans.insert(m_Bans.begin()+index, new CDBBan( m_Server, name, ip, "N/A", gamename, admin, reason, expiredate ) );
	UpdateMapAddBan(index);

//	m_Bans.push_back( new CDBBan( m_Server, name, ip, "N/A", gamename, admin, reason ) );
}

void CBNET :: RemoveAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	vector <uint32_t> ::iterator j = m_Accesses.begin( );
	for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); )
	{
		if( *i == name )
		{
			i = m_Admins.erase( i );
			j = m_Accesses.erase( j );
		}
		else
		{
			i++;
			j++;
		}
	}
}

void CBNET :: RemoveSafe( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i != m_Safe.size( ); i++)
	{
		if (m_Safe[i]==name)
		{
			m_Safe.erase(m_Safe.begin()+i);
			m_SafeVouchers.erase(m_SafeVouchers.begin()+i);
			break;
		}
	}
}

void CBNET :: RemoveNote( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i != m_Notes.size( ); i++)
	{
		if (m_Notes[i]==name)
		{
			m_Notes.erase(m_Notes.begin()+i);
			m_NotesN.erase(m_NotesN.begin()+i);
			break;
		}
	}
}

bool CBNET :: IsFriend( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string lowername;

	for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); i++ )
	{
		lowername = (*i)->GetAccount();
		transform( lowername.begin( ), lowername.end( ), lowername.begin( ), (int(*)(int))tolower );

		if( lowername == name )
			return true;
	}

	return false;
}

bool CBNET :: IsClanMember( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
	{
		if( (*i)->GetName() == name )
			return true;
	}

	return false;
}

bool CBNET :: IsClanRecruit( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_ClanRecruits.begin( ); i != m_ClanRecruits.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

bool CBNET :: IsClanPeon( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_ClanPeons.begin( ); i != m_ClanPeons.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

bool CBNET :: IsClanGrunt( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_ClanGrunts.begin( ); i != m_ClanGrunts.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

bool CBNET :: IsClanShaman( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_ClanShamans.begin( ); i != m_ClanShamans.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

bool CBNET :: IsClanChieftain( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_ClanChieftains.begin( ); i != m_ClanChieftains.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

bool CBNET :: IsClanFullMember( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
	{
		if( (*i)->GetName() == name && !IsClanRecruit(name))
			return true;
	}

	return false;
}

void CBNET :: RemoveBan( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	uint32_t j = 0;
	for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); )
	{
		if( (*i)->GetName( ) == name )
		{
			i = m_Bans.erase( i );
			UpdateMapRemoveBan(j);
		}
		else
		{
			i++;
			j++;
		}
	}
}

void CBNET :: HoldFriends( CBaseGame *game )
{
	if( game )
	{
		for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); i++ )
			game->AddToReserved( (*i)->GetAccount( ), 255 );
	}
}

void CBNET :: HoldClan( CBaseGame *game )
{
	if( game )
	{
		for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
			game->AddToReserved( (*i)->GetName( ), 255 );
	}
}

uint32_t CBNET :: LastAccess( )
{
	return m_LastAccess;
}

bool CBNET :: UpdateAccess( string name, uint32_t access )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	vector <uint32_t> ::iterator j = m_Accesses.begin( );
	for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); i++ )
	{
		if( *i == name )
		{
			*j = access;
			return true;
		}
		j++;
	}

	return false;
}

void CBNET :: UpdateMap ()
{
	m_BanlistIndexes.clear();
	unsigned char lastletter = 0;
	uint32_t index = 0;
	unsigned char letter = 0;
	unsigned char l;

	for (uint32_t i=0;i<m_Bans.size();i++)
	{
		// build indexes
		letter = m_Bans[i]->GetName()[0];
		if (letter>lastletter)
		{				
			if (lastletter ==0)
				l = letter;
			else
				l = letter-1;
			if (letter-lastletter>1)
				for (uint32_t i = lastletter+1; i<=l; i++)
				{
					m_BanlistIndexes.push_back(999999);
				}
				m_BanlistIndexes.push_back(index);
				lastletter = letter;
		}
		index++;
	}
	letter = 255;
	for (uint32_t i = lastletter+1; i<=letter; i++)
	{
		m_BanlistIndexes.push_back(999999);
	}
}

void CBNET :: UpdateMapRemoveBan( uint32_t n )
{
	for (uint32_t i=0; i<m_BanlistIndexes.size();i++)
	{
		if (m_BanlistIndexes[i]!=999999 && m_BanlistIndexes[i]>n)
			m_BanlistIndexes[i]=m_BanlistIndexes[i]-1;
		if (m_BanlistIndexes[i]==n)
		{
			if (m_Bans.size()>n)
			{
				if (m_Bans[n]->GetName()[0]!=i)
				m_BanlistIndexes[i] = 999999;

			} else
			m_BanlistIndexes[i] = 999999;
		}
	}
}

void CBNET :: UpdateMapAddBan( uint32_t n )
{
	for (uint32_t i=0; i<m_BanlistIndexes.size();i++)
	{
		if (m_BanlistIndexes[i]!=999999 && m_BanlistIndexes[i]>n)
			m_BanlistIndexes[i]=m_BanlistIndexes[i]+1;
	}
}

void CBNET :: ChannelJoin( string name )
{
	if (m_GHost->m_channeljoinmessage && !m_GHost->IsChannelException(name))
	{
		for (uint32_t i=0; i<m_GHost->m_ChannelWelcome.size(); i++)
		{
			if ( m_PasswordHashType == "pvpgn")
				ImmediateChatCommand("/w "+name+" "+m_GHost->m_ChannelWelcome[i]);
			else
				QueueChatCommand("/w "+name+" "+m_GHost->m_ChannelWelcome[i]);
		}
	}
	if (!m_GHost->m_channeljoingreets)
		return;

	if (m_GHost->IsChannelException(name))
		return;
	bool Safe = IsSafe(name);
	bool Admin = IsAdmin(name);
	bool RootAdmin = IsRootAdmin(name);
	bool Chieftain = IsClanChieftain(name);
	bool Shaman = IsClanShaman(name);
	string msg=string();
	if (Chieftain)
		msg = m_GHost->m_Language->ChieftainJoinedTheChannel(name);
	else if (Shaman)
		msg = m_GHost->m_Language->ShamanJoinedTheChannel(name);
	else if (RootAdmin)
		msg = m_GHost->m_Language->RootAdminJoinedTheChannel(name);
	else if (Admin)
		msg = m_GHost->m_Language->AdminJoinedTheChannel(name);
	else if (Safe)
		msg = m_GHost->m_Language->SafeJoinedTheChannel(name);
	
	if (msg.size()!=0 && m_OutPackets.size()<3)
		QueueChatCommand("/me " + msg);
}

string CBNET :: GetPlayerFromNamePartial( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t Matches = 0;
	string player = string();

	// try to match each player with the passed string (e.g. "Gen" would be matched with "lock")

	for( uint32_t i = 0; i != Channel_Users().size(); i++ )
	{
		string TestName;
		TestName = Channel_Users()[i];
		transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

		if( TestName.find( name ) != string :: npos )
		{
			Matches++;
			player = Channel_Users()[i];

			// if the name matches exactly stop any further matching

			if( TestName == name )
			{
				Matches = 1;
				break;
			}
		}
	}
	if (Matches == 1)
		return player;
	else 
		return string();
}

void CBNET :: WarnPlayer (string Victim, string Reason, string User, bool Whisper)
{
	if (m_GHost->m_DB->BanAdd( m_Server, Victim, string(), string(), User, Reason, m_GHost->m_WarnTimeOfWarnedPlayer, 1 ) )
	{

		uint32_t WarnCount = 0;
		for(int i = 0; i < 3 && WarnCount == 0; i++)
		{
			WarnCount = m_GHost->m_DB->BanCount( Victim, 1 );
		}					

		m_GHost->UDPChatSend("|warn "+Victim);

		if(WarnCount >= m_GHost->m_BanTheWarnedPlayerQuota)
		{
			QueueChatCommand( m_GHost->m_Language->UserReachedWarnQuota( Victim, UTIL_ToString( m_GHost->m_BanTimeOfWarnedPlayer ) ), User, Whisper );
			if(!m_GHost->m_DB->BanAdd( m_Server, Victim, string( ), string( ), User, "Reached the warn quota", m_GHost->m_BanTimeOfWarnedPlayer, 0 ))
			{
				QueueChatCommand( m_GHost->m_Language->ErrorWarningUser( m_Server, Victim ), User, Whisper );
			}
			else
			{
				string sDate = string();
				if (m_GHost->m_BanTimeOfWarnedPlayer>0)
				{
					struct tm * timeinfo;
					char buffer [80];
					time_t Now = time( NULL );
					Now += 3600*24*m_GHost->m_BanTimeOfWarnedPlayer;
					timeinfo = localtime( &Now );
					strftime (buffer,80,"%d-%m-%Y",timeinfo);  
					sDate = buffer;
				}

				AddBan(Victim, string(), string(), User, Reason, sDate);
				if (m_GHost->m_BanBannedFromChannel)
					QueueChatCommand("/ban "+Victim);

				m_GHost->m_DB->WarnUpdate( Victim, 3, 2);
				m_GHost->m_DB->WarnUpdate( Victim, 1, 3);
			}
		} else 
			QueueChatCommand( m_GHost->m_Language->WarnedUser( m_Server, Victim, UTIL_ToString( WarnCount ) ), User, Whisper );
	}
}
