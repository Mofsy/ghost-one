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
#include "gcbiprotocol.h"

#include <cmath>
#include <string.h>
#include <time.h>

#include "next_combination.h"

//
// sorting classes
//

class CGamePlayerSortAscByPing
{
public:
	bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const
	{
		return Player1->GetPing( false ) < Player2->GetPing( false );
	}
};

class CGamePlayerSortDescByPing
{
public:
	bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const
	{
		return Player1->GetPing( false ) > Player2->GetPing( false );
	}
};

//
// CBaseGame
//

CBaseGame :: CBaseGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer ) : m_GHost( nGHost ), m_SaveGame( nSaveGame ), m_Replay( NULL ), m_Exiting( false ), m_Saving( false ), m_HostPort( nHostPort ), m_GameState( nGameState ), m_VirtualHostPID( 255 ), m_GProxyEmptyActions( 0 ), m_GameName( nGameName ), m_LastGameName( nGameName ), m_VirtualHostName( m_GHost->m_VirtualHostName ), m_OwnerName( nOwnerName ), m_CreatorName( nCreatorName ), m_CreatorServer( nCreatorServer ), m_HCLCommandString( nMap->GetMapDefaultHCL( ) ), m_RandomSeed( GetTicks( ) ), m_HostCounter( m_GHost->m_HostCounter++ ), m_EntryKey( rand( ) ), m_Latency( m_GHost->m_Latency ), m_SyncLimit( m_GHost->m_SyncLimit ), m_SyncCounter( 0 ), m_GameTicks( 0 ), m_CreationTime( GetTime( ) ), m_LastPingTime( GetTime( ) ), m_LastRefreshTime( GetTime( ) ), m_LastDownloadTicks( GetTime( ) ), m_DownloadCounter( 0 ), m_LastDownloadCounterResetTicks( GetTime( ) ), m_LastAnnounceTime( 0 ), m_AnnounceInterval( 0 ), m_LastAutoStartTime( GetTime( ) ), m_AutoStartPlayers( 0 ), m_LastCountDownTicks( 0 ), m_CountDownCounter( 0 ), m_StartedLoadingTicks( 0 ), m_StartPlayers( 0 ), m_LastLagScreenResetTime( 0 ), m_LastActionSentTicks( 0 ), m_LastActionLateBy( 0 ), m_StartedLaggingTime( 0 ), m_LastLagScreenTime( 0 ), m_LastReservedSeen( GetTime( ) ), m_StartedKickVoteTime( 0 ), m_GameOverTime( 0 ), m_LastPlayerLeaveTicks( 0 ), m_MinimumScore( 0. ), m_MaximumScore( 0. ), m_SlotInfoChanged( false ), m_Locked( false ), m_RefreshMessages( m_GHost->m_RefreshMessages ), m_RefreshError( false ), m_RefreshRehosted( false ), m_MuteAll( false ), m_MuteLobby( false ), m_CountDownStarted( false ), m_GameLoading( false ), m_GameLoaded( false ), m_LoadInGame( nMap->GetMapLoadInGame( ) ), m_Lagging( false ), m_AutoSave( m_GHost->m_AutoSave ), m_MatchMaking( false ), m_LocalAdminMessages( m_GHost->m_LocalAdminMessages ), m_StartedVoteStartTime( 0 )
{
	m_GHost = nGHost;
	m_Socket = new CTCPServer( );
	m_Protocol = new CGameProtocol( m_GHost );
	m_Map = new CMap( *nMap );
	m_SaveGame = nSaveGame;

	if( m_GHost->m_SaveReplays && !m_SaveGame )
		m_Replay = new CReplay( );
	else
		m_Replay = NULL;

	m_Exiting = false;
	m_Saving = false;
	m_HostPort = nHostPort;
	m_GameState = nGameState;
	m_VirtualHostPID = 255;
	m_WTVPlayerPID = 255;
	// wait time of 1 minute  = 0 empty actions required
	// wait time of 2 minutes = 1 empty action required
	// etc...

	if( m_GHost->m_ReconnectWaitTime == 0 )
		m_GProxyEmptyActions = 0;
	else
	{
		m_GProxyEmptyActions = m_GHost->m_ReconnectWaitTime - 1;

		// clamp to 9 empty actions (10 minutes)

		if( m_GProxyEmptyActions > 9 )
			m_GProxyEmptyActions = 9;
	}
	m_GameName = nGameName;
	m_LastGameName = nGameName;
	wtvprocessid = 0;
	AddGameName(m_GameName);
	m_OriginalGameName = nGameName;
	m_VirtualHostName = m_GHost->m_VirtualHostName;
	m_OwnerName = nOwnerName;
	m_DefaultOwner = m_OwnerName;				
	m_CreatorName = nCreatorName;
	m_CreatorServer = nCreatorServer;
	m_HCLCommandString = m_Map->GetMapDefaultHCL( );
	m_Server = nCreatorServer;
	m_RandomSeed = GetTicks( );
	m_GHost->m_HostCounter++;
	m_GHost->SaveHostCounter();
	if (m_GHost->m_MaxHostCounter>0)
	if (m_GHost->m_HostCounter>m_GHost->m_MaxHostCounter)
		m_GHost->m_HostCounter = 1;
	m_HostCounter = m_GHost->m_HostCounter;
	m_Latency = m_GHost->m_Latency;
	m_UseDynamicLatency = m_GHost->m_UseDynamicLatency;
	m_DynamicLatency = m_Latency;
	m_DetourAllMessagesToAdmins = m_GHost->m_DetourAllMessagesToAdmins;
	m_NormalCountdown = m_GHost->m_DetourAllMessagesToAdmins;
	m_LastDynamicLatencyTicks = 0;
	m_LastAdminJoinAndFullTicks = GetTicks();
	m_MaxSync = 0;
	m_MaxSyncUser = string();
	m_SyncLimit = m_GHost->m_SyncLimit;
	m_SyncCounter = 0;
	m_MaxSyncCounter = 0;
	m_GameTicks = 0;
	m_CreationTime = GetTime( );
	m_LastPingTime = GetTime( );
	m_LastRefreshTime = GetTime( );
	m_LastDownloadTicks = GetTime( );
	m_LastDLCounterTicks = GetTime( );
	m_DownloadCounter = 0;
	m_DLCounter = 0;
	m_LastDownloadCounterResetTicks = GetTicks( );
	m_LastAnnounceTime = 0;
	m_AnnounceInterval = 0;
	m_AutoStartPlayers = 0;
	m_LastCountDownTicks = 0;
	m_CountDownCounter = 0;
	m_StartedLoadingTicks = 0;
	m_StartedLoadingTime = 0;
	m_StartPlayers = 0;
	m_LastAutoStartTime = GetTime( );
	m_LastLagScreenResetTime = 0;
	m_LastActionSentTicks = 0;
	m_LastActionLateBy = 0;
	m_StartedLaggingTime = 0;
	m_LastLagScreenTime = 0;
	m_LastReservedSeen = GetTime( );
	m_LastActionLateTicks = 0;
	m_StartedKickVoteTime = 0;
	m_LagScreenTime = 0;
	m_GameLoadedTime = 0;
	m_GameOverTime = 0;
	m_LastPlayerLeaveTicks = 0;
	m_ActualyPrintPlayerWaitingStartDelay = 0;
	m_MinimumScore = 0.0;
	m_MaximumScore = 0.0;
	m_SlotInfoChanged = false;
	m_Locked = false;
	m_RefreshCompleted = false;
	m_RefreshMessages = m_GHost->m_RefreshMessages;
	m_RefreshError = false;
	m_RefreshRehosted = false;
	m_OwnerJoined = false;
	m_ShowScoreOf = string();
	m_ShowNoteOf = string();
	m_DisableStats = false;
	m_HCL = false;
	m_Listen = false;
	m_RootListen = false;
	m_MuteAll = false;
	m_GameEnded = false;
	m_GameEndedTime = 0;
	m_GameLoadedMessage = false;
	m_AllPlayersWarnChecked = false;
	m_LastWarnCheck = 0;
	m_MuteLobby = false;
	m_CountDownStarted = false;
	m_StartVoteStarted = false;
	m_GameEndCountDownStarted = false;
	m_GameLoading = false;
	m_GameLoaded = false;
	m_EndRequested = false;
	m_EndRequestedTicks = 0;
	if (m_GHost->m_Banning == 1)
		m_Bans = true;
	else
		m_Bans = false;
	m_LoadInGame = m_Map->GetMapLoadInGame( );
	m_Desynced = false;
	m_Lagging = false;
	m_EndGameTime = 0;
	m_ShowRealSlotCount = m_GHost->m_ShowRealSlotCount;
	m_SwitchTime = 0;
	m_SwitchNr = 0;
	m_Switched = false;
	m_CreatorAsFriend = m_GHost->m_addcreatorasfriendonhost;
	m_GarenaOnly = false;
	m_GameOverCanceled = false;
	m_GameOverDiffCanceled = false;
	m_CountryCheck = false;
	m_CountryCheck2 = false;
	m_ProviderCheck = false;
	m_ProviderCheck2 = false;
	m_ScoreCheck = false;
	m_ScoreCheckChecked = false;
	m_ScoreCheckScore = 0;
	m_ScoreCheckRank = 0;
	m_Team1 = 0;
	m_Team2 = 0;
	m_Team3 = 0;
	m_Team4 = 0;
	m_TeamDiff = 0;
	m_EvenPlayeredTeams = false;
	m_BanOn = false;
	m_PlayersLeft = 0;
	m_Rehost = false;
	m_LastPlayerJoined = 255;
	m_LastPlayerJoinedTime = GetTime()+5;
	m_LastPlayerJoiningTime = GetTime()+5;
	m_LastMars = GetTime()-10;
	m_LastPlayerWarningTicks = GetTicks();
	m_AllSlotsOccupied = false;
	m_PingsUpdated = false;
	m_AllSlotsAnnounced = false;
	m_DownloadOnlyMode = false;
	m_AutoSave = m_GHost->m_AutoSave;
	m_DoAutoWarns = false;
	m_MatchMaking = false;
	m_LastInfoShow = GetTime( );
	m_LastGenInfoShow = GetTime( );
	m_LastOwnerInfoShow = GetTime( );
	m_LocalAdminMessages = m_GHost->m_LocalAdminMessages;

			uint32_t slotstotal = m_Slots.size( );
			uint32_t slotsopen = GetSlotsOpen();			
			if (slotsopen<2) slotsopen = 2;
			if(slotstotal > 12) slotstotal = 12;

			if( m_SaveGame )
	{
		m_EnforceSlots = m_SaveGame->GetSlots( );
		m_Slots = m_SaveGame->GetSlots( );

		// the savegame slots contain player entries
		// we really just want the open/closed/computer entries
		// so open all the player slots

		for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
		{
			if( (*i).GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && (*i).GetComputer( ) == 0 )
			{
				(*i).SetPID( 0 );
				(*i).SetDownloadStatus( 255 );
				(*i).SetSlotStatus( SLOTSTATUS_OPEN );
			}
		}
	}
	else
		m_Slots = m_Map->GetSlots( );

	if( !m_GHost->m_IPBlackListFile.empty( ) )
	{
		ifstream in;
		in.open( m_GHost->m_IPBlackListFile.c_str( ) );

		if( in.fail( ) )
			CONSOLE_Print( "[GAME: " + m_GameName + "] error loading IP blacklist file [" + m_GHost->m_IPBlackListFile + "]" );
		else
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] loading IP blacklist file [" + m_GHost->m_IPBlackListFile + "]" );
			string Line;

			while( !in.eof( ) )
			{
				getline( in, Line );

				// ignore blank lines and comments

				if( Line.empty( ) || Line[0] == '#' )
					continue;

				// remove newlines and partial newlines to help fix issues with Windows formatted files on Linux systems

				Line.erase( remove( Line.begin( ), Line.end( ), ' ' ), Line.end( ) );
				Line.erase( remove( Line.begin( ), Line.end( ), '\r' ), Line.end( ) );
				Line.erase( remove( Line.begin( ), Line.end( ), '\n' ), Line.end( ) );

				// ignore lines that don't look like IP addresses

				if( Line.find_first_not_of( "1234567890." ) != string :: npos )
					continue;

				m_IPBlackList.insert( Line );
			}

			in.close( );

			CONSOLE_Print( "[GAME: " + m_GameName + "] loaded " + UTIL_ToString( m_IPBlackList.size( ) ) + " lines from IP blacklist file" );
		}
	}

	m_AutoWarnMarks = m_Map->GetAutoWarnMarks( );

	// start listening for connections

	if( !m_GHost->m_BindAddress.empty( ) )
		CONSOLE_Print( "[GAME: " + m_GameName + "] attempting to bind to address [" + m_GHost->m_BindAddress + "]" );
	else
		CONSOLE_Print( "[GAME: " + m_GameName + "] attempting to bind to all available addresses" );

	if( m_Socket->Listen( m_GHost->m_BindAddress, m_HostPort ) )
		CONSOLE_Print( "[GAME: " + m_GameName + "] listening on port " + UTIL_ToString( m_HostPort ) );
	else
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] error listening on port " + UTIL_ToString( m_HostPort ) );
		m_Exiting = true;
	}

	m_LastReservedSeen = GetTime( );

	m_GetMapType = m_Map->GetMapType();
	m_GetMapPath = m_Map->GetMapPath();
	m_GetMapGameType = m_Map->GetMapGameType();
	m_GetMapNumPlayers = m_Map->GetMapNumPlayers();
	m_GetMapNumTeams = m_Map->GetMapNumTeams();
	m_GetMapOnlyAutoWarnIfMoreThanXPlayers = m_Map->GetMapOnlyAutoWarnIfMoreThanXPlayers();

	if (m_GetMapType == "dota" && m_GHost->m_AutoStartDotaGames)
		m_AutoStartPlayers = 10;
}

CBaseGame :: ~CBaseGame( )
{
	// save replay
	// todotodo: put this in a thread

	if( m_Replay && ( m_GameLoading || m_GameLoaded ) )
	{
		time_t Now = time( NULL );
		char Time[17];
		memset( Time, 0, sizeof( char ) * 17 );
		time_t times = Now+ m_GHost->m_ReplayTimeShift;
		strftime( Time, sizeof( char ) * 17, "%Y-%m-%d %H-%M", localtime( &times ) );
		string MinString = UTIL_ToString( ( m_GameTicks / 1000 ) / 60 );
		string SecString = UTIL_ToString( ( m_GameTicks / 1000 ) % 60 );

		if( MinString.size( ) == 1 )
			MinString.insert( 0, "0" );

		if( SecString.size( ) == 1 )
			SecString.insert( 0, "0" );

		m_Replay->BuildReplay( m_GameName, m_StatString, m_GHost->m_ReplayWar3Version, m_GHost->m_ReplayBuildNumber );
		m_Replay->Save( m_GHost->m_TFT, m_GHost->m_ReplayPath + UTIL_FileSafeName( "GHost++ " + string( Time ) + " " + m_GameName + ".w3g" ) );
	}

	delete m_Socket;
	delete m_Protocol;
	delete m_Map;
	delete m_Replay;

	for( vector<CPotentialPlayer *> :: iterator i = m_Potentials.begin( ); i != m_Potentials.end( ); i++ )
		delete *i;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		delete *i;

	for( vector<CCallableScoreCheck *> :: iterator i = m_ScoreChecks.begin( ); i != m_ScoreChecks.end( ); i++ )
		m_GHost->m_Callables.push_back( *i );

	for( vector<CCallableWarnCount *> :: iterator i = m_WarnCounts.begin( ); i != m_WarnCounts.end( ); i++ )
		m_GHost->m_Callables.push_back( *i );

	for( vector<CCallableBanUpdate *> :: iterator i = m_BanUpdates.begin( ); i != m_BanUpdates.end( ); i++ )
		m_GHost->m_Callables.push_back( *i );

	for( vector<CCallableRunQuery *> :: iterator i = m_RunQueries.begin( ); i != m_RunQueries.end( ); i++ )
		m_GHost->m_Callables.push_back( *i );

	while( !m_Actions.empty( ) )
	{
		delete m_Actions.front( );
		m_Actions.pop( );
	}

	m_GameNames.clear();
}

uint32_t CBaseGame :: GetNextTimedActionTicks( )
{
	// return the number of ticks (ms) until the next "timed action", which for our purposes is the next game update
	// the main GHost++ loop will make sure the next loop update happens at or before this value
	// note: there's no reason this function couldn't take into account the game's other timers too but they're far less critical
	// warning: this function must take into account when actions are not being sent (e.g. during loading or lagging)

	if( !m_GameLoaded || m_Lagging )
		return 50;

	uint32_t TicksSinceLastUpdate = GetTicks( ) - m_LastActionSentTicks;

	if( TicksSinceLastUpdate > m_DynamicLatency - m_LastActionLateBy )
		return 0;
	else
		return m_DynamicLatency - m_LastActionLateBy - TicksSinceLastUpdate;
}

uint32_t CBaseGame :: GetSlotsOccupied( )
{
	uint32_t NumSlotsOccupied = 0;

	for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
	{
		if( (*i).GetSlotStatus( ) == SLOTSTATUS_OCCUPIED )
			NumSlotsOccupied++;
	}

	return NumSlotsOccupied;
}

uint32_t CBaseGame :: GetSlotsOpen( )
{
	uint32_t NumSlotsOpen = 0;

	for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
	{
		if( (*i).GetSlotStatus( ) == SLOTSTATUS_OPEN )
			NumSlotsOpen++;
	}

	return NumSlotsOpen;
}

uint32_t CBaseGame :: GetNumPlayers( )
{
	return GetNumHumanPlayers( ) + m_FakePlayers.size( );
}

uint32_t CBaseGame :: GetNumHumanPlayers( )
{
	uint32_t NumHumanPlayers = 0;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
			NumHumanPlayers++;
	}

	return NumHumanPlayers;
}

string CBaseGame :: GetDescription( )
{
	string Description = m_GameName + " : " + m_OwnerName + " : " + UTIL_ToString( GetNumHumanPlayers( ) ) + "/" + UTIL_ToString( m_GameLoading || m_GameLoaded ? m_StartPlayers : m_Slots.size( ) );

	if( m_GameLoading || m_GameLoaded )
		Description += " : " + UTIL_ToString( ( m_GameTicks / 1000 ) / 60 ) + "m";
	else
		Description += " : " + UTIL_ToString( ( GetTime( ) - m_CreationTime ) / 60 ) + "m";

	return Description;
}

void CBaseGame :: SetAnnounce( uint32_t interval, string message )
{
	m_AnnounceInterval = interval;
	m_AnnounceMessage = message;
	m_LastAnnounceTime = GetTime( );
}

unsigned int CBaseGame :: SetFD( void *fd, void *send_fd, int *nfds )
{
	unsigned int NumFDs = 0;

	if( m_Socket )
	{
		m_Socket->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
		NumFDs++;
	}

	for( vector<CPotentialPlayer *> :: iterator i = m_Potentials.begin( ); i != m_Potentials.end( ); i++ )
	{
		if( (*i)->GetSocket( ) )
		{
			(*i)->GetSocket( )->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
			NumFDs++;
		}
	}

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetSocket( ) )
		{
			(*i)->GetSocket( )->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
			NumFDs++;
		}
	}

	return NumFDs;
}

bool CBaseGame :: Update( void *fd, void *send_fd )
{
	// update callables

	for( vector<CCallableWarnCount *> :: iterator i = m_WarnCounts.begin( ); i != m_WarnCounts.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			uint32_t Count = (*i)->GetResult( );
			CGamePlayer * Player = GetPlayerFromName((*i)->GetName(), false);

			if (Player)
			{
				// count warns
				if (Count > 0)
				{
					SendChat( Player, m_GHost->m_Language->DisplayWarnsCount( UTIL_ToString(Count) ) );
				}
			}

			m_GHost->m_DB->RecoverCallable( *i );
			delete *i;
			i = m_WarnCounts.erase( i );
		}
		else
			i++;
	}

	for( vector<CCallableBanUpdate *> :: iterator i = m_BanUpdates.begin( ); i != m_BanUpdates.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_GHost->m_DB->RecoverCallable( *i );
			delete *i;
			i = m_BanUpdates.erase( i );
		}
		else
			i++;
	}

	for( vector<CCallableRunQuery *> :: iterator i = m_RunQueries.begin( ); i != m_RunQueries.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_GHost->m_DB->RecoverCallable( *i );
			delete *i;
			i = m_RunQueries.erase( i );
		}
		else
			i++;
	}

	for( vector<CCallableScoreCheck *> :: iterator i = m_ScoreChecks.begin( ); i != m_ScoreChecks.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			double Score = (*i)->GetResult( );

			CGamePlayer * Player = GetPlayerFromName((*i)->GetName( ), false);
			if (Player)
			{
				Player->SetScoreS(UTIL_ToString(Score,2));
				Player->SetRankS(UTIL_ToString((*i)->GetRank( )));
				Player->SetScore(Score);
			}

			for( vector<CPotentialPlayer *> :: iterator j = m_Potentials.begin( ); j != m_Potentials.end( ); j++ )
			{
				if( (*j)->GetJoinPlayer( ) && (*j)->GetJoinPlayer( )->GetName( ) == (*i)->GetName( ) )
				{
					if (m_MatchMaking && m_AutoStartPlayers != 0)
						EventPlayerJoinedWithScore( *j, (*j)->GetJoinPlayer( ), Score );
					else
					if (m_ScoreCheck)
					{
						m_ScoreCheckChecked = true;
						m_ScoreCheckScore = Score;
						m_ScoreCheckRank = (*i)->GetRank( );
						EventPlayerJoined(*j, (*j)->GetJoinPlayer( ));
						m_ScoreCheckChecked = false;
					}
				}
			}

			m_GHost->m_DB->RecoverCallable( *i );
			delete *i;
			i = m_ScoreChecks.erase( i );
		}
		else
			i++;
	}

	// update players

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); )
	{
		if( (*i)->Update( fd ) )
		{
			EventPlayerDeleted( *i );
			delete *i;
			i = m_Players.erase( i );
		}
		else
			i++;
	}

	for( vector<CPotentialPlayer *> :: iterator i = m_Potentials.begin( ); i != m_Potentials.end( ); )
	{
		if( (*i)->Update( fd ) )
		{
			// flush the socket (e.g. in case a rejection message is queued)

			if( (*i)->GetSocket( ) )
				(*i)->GetSocket( )->DoSend( (fd_set *)send_fd );

			delete *i;
			i = m_Potentials.erase( i );
		}
		else
			i++;
	}

	// create the virtual host player
//	if( !m_GameLoading && !m_GameLoaded && GetNumPlayers( ) < 12 )
	uint32_t SlotReq = 12;
	if (m_ShowRealSlotCount)
		SlotReq = m_Slots.size();
	if( !m_GameLoading && !m_GameLoaded && GetSlotsOccupied() < SlotReq && !m_GHost->m_DetourAllMessagesToAdmins )
		CreateVirtualHost( );

	// unlock the game

	if( m_Locked && !GetPlayerFromName( m_OwnerName, false ) )
	{
		SendAllChat( m_GHost->m_Language->GameUnlocked( ) );
		m_Locked = false;
	}

	// ping every 4 seconds
	// changed this to ping during game loading as well to hopefully fix some problems with people disconnecting during loading
	// changed this to ping during the game as well

	if( GetTime( ) - m_LastPingTime >= 4 )
	{
		// note: we must send pings to players who are downloading the map because Warcraft III disconnects from the lobby if it doesn't receive a ping every ~90 seconds
		// so if the player takes longer than 90 seconds to download the map they would be disconnected unless we keep sending pings
		// todotodo: ignore pings received from players who have recently finished downloading the map

		SendAll( m_Protocol->SEND_W3GS_PING_FROM_HOST( ) );

		// we also broadcast the game to the local network every 5 seconds so we hijack this timer for our nefarious purposes
		// however we only want to broadcast if the countdown hasn't started
		// see the !sendlan code later in this file for some more information about how this works
		// todotodo: should we send a game cancel message somewhere? we'll need to implement a host counter for it to work

		if( !m_CountDownStarted )
		{
			BYTEARRAY MapGameType;
			// construct a fixed host counter which will be used to identify players from this "realm" (i.e. LAN)
			// the fixed host counter's 4 most significant bits will contain a 4 bit ID (0-15)
			// the rest of the fixed host counter will contain the 28 least significant bits of the actual host counter
			// since we're destroying 4 bits of information here the actual host counter should not be greater than 2^28 which is a reasonable assumption
			// when a player joins a game we can obtain the ID from the received host counter
			// note: LAN broadcasts use an ID of 0, battle.net refreshes use an ID of 1-10, the rest are unused

			uint32_t FixedHostCounter = m_HostCounter & 0x0FFFFFFF;

			// construct the correct W3GS_GAMEINFO packet

			uint32_t slotstotal = m_Slots.size( );
			uint32_t slotsopen = GetSlotsOpen();
			if (slotsopen<2) slotsopen = 2;

			if (!m_ShowRealSlotCount)
			{
				slotsopen = 3;
				slotstotal = m_Slots.size( );
			}

			if( m_SaveGame )
			{
				// note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)
				uint32_t MapGameType = MAPGAMETYPE_SAVEDGAME;
				BYTEARRAY MapWidth;
				MapWidth.push_back( 0 );
				MapWidth.push_back( 0 );
				BYTEARRAY MapHeight;
				MapHeight.push_back( 0 );
				MapHeight.push_back( 0 );
				if (m_GHost->m_broadcastinlan)
				{
					m_GHost->m_UDPSocket->Broadcast( 6112, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), MapWidth, MapHeight, m_GameName, "Gen", GetTime( ) - m_CreationTime, "Save\\Multiplayer\\" + m_SaveGame->GetFileNameNoPath( ), m_SaveGame->GetMagicNumber( ), slotstotal, slotsopen, m_HostPort, FixedHostCounter, m_EntryKey ) );
					m_GHost->m_UDPSocket->SendTo("127.0.0.1", 6112, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), MapWidth, MapHeight, m_GameName, "Gen", GetTime( ) - m_CreationTime, "Save\\Multiplayer\\" + m_SaveGame->GetFileNameNoPath( ), m_SaveGame->GetMagicNumber( ), slotstotal, slotsopen, m_HostPort, FixedHostCounter, m_EntryKey ) );
				}
//				if (m_GarenaOnly)
				{
					//m_GHost->m_UDPSocket->SendTo("127.0.0.1", 6112, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), MapWidth, MapHeight, m_GameName, "Gen", GetTime( ) - m_CreationTime, "Save\\Multiplayer\\" + m_SaveGame->GetFileNameNoPath( ), m_SaveGame->GetMagicNumber( ), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey ) );
				}
				m_GHost->UDPChatSend(m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), MapWidth, MapHeight, m_GameName, "Gen", GetTime( ) - m_CreationTime, "Save\\Multiplayer\\" + m_SaveGame->GetFileNameNoPath( ), m_SaveGame->GetMagicNumber( ), slotstotal, slotsopen, m_HostPort, FixedHostCounter, m_EntryKey ) );
			}
			else
			{
				uint32_t MapGameType = MAPGAMETYPE_UNKNOWN0;				
				if (m_GHost->m_broadcastinlan)
				{
					m_GHost->m_UDPSocket->Broadcast( 6112, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), m_Map->GetMapWidth( ), m_Map->GetMapHeight( ), m_GameName, "Gen", GetTime( ) - m_CreationTime, m_Map->GetMapPath( ), m_Map->GetMapCRC( ), slotstotal, slotsopen, m_HostPort, FixedHostCounter, m_EntryKey ) );
					m_GHost->m_UDPSocket->SendTo( "127.0.0.1", 6112, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), m_Map->GetMapWidth( ), m_Map->GetMapHeight( ), m_GameName, "Gen", GetTime( ) - m_CreationTime, m_Map->GetMapPath( ), m_Map->GetMapCRC( ), slotstotal, slotsopen, m_HostPort, FixedHostCounter, m_EntryKey ) );
				}
//				if (m_GarenaOnly)
				{
//					m_GHost->m_UDPSocket->SendTo( "127.0.0.1", 6112, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), m_Map->GetMapWidth( ), m_Map->GetMapHeight( ), m_GameName, "Gen", GetTime( ) - m_CreationTime, m_Map->GetMapPath( ), m_Map->GetMapCRC( ), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey ) );
//					m_GHost->m_UDPSocket->SendTo( "192.168.1.2", 6112, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), m_Map->GetMapWidth( ), m_Map->GetMapHeight( ), m_GameName, "Gen", GetTime( ) - m_CreationTime, m_Map->GetMapPath( ), m_Map->GetMapCRC( ), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey ) );
				}
				m_GHost->UDPChatSend(m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), m_Map->GetMapWidth( ), m_Map->GetMapHeight( ), m_GameName, "Gen", GetTime( ) - m_CreationTime, m_Map->GetMapPath( ), m_Map->GetMapCRC( ), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey ));
			}
		}

		m_LastPingTime = GetTime( );
	}
	// auto rehost if there was a refresh error in autohosted games

	if( m_RefreshError && !m_CountDownStarted && m_GameState == GAME_PUBLIC && !m_GHost->m_AutoHostGameName.empty( ) && m_GHost->m_AutoHostMaximumGames != 0 && m_GHost->m_AutoHostAutoStartPlayers != 0 && m_AutoStartPlayers != 0 )
	{		
		// there's a slim chance that this isn't actually an autohosted game since there is no explicit autohost flag
		// however, if autohosting is enabled and this game is public and this game is set to autostart, it's probably autohosted
		// so rehost it using the current autohost game name

		string GameName = m_GHost->m_AutoHostGameName + " #" + UTIL_ToString( m_GHost->m_HostCounter );
		CONSOLE_Print( "[GAME: " + m_GameName + "] automatically trying to rehost as public game [" + GameName + "] due to refresh failure" );
		m_LastGameName = m_GameName;
		m_GameName = GameName;
		m_HostCounter = m_GHost->m_HostCounter++;
		m_RefreshError = false;

                for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
		{
			(*i)->QueueGameUncreate( );
			(*i)->QueueEnterChat( );

			// the game creation message will be sent on the next refresh
		}

		m_CreationTime = GetTime( );
		m_LastRefreshTime = GetTime( );
	}
	
	// refresh every 3 seconds

	bool m_AutoHostRefresh = (m_autohosted && m_GHost->m_AutoHostLocal);
	// don't refresh if we're autohosting locally only
	if (!m_AutoHostRefresh )
	if( !m_RefreshError && !m_CountDownStarted && m_GameState == GAME_PUBLIC && GetSlotsOpen( ) > 0 && GetTime( )- m_LastRefreshTime >= 3 )
	{
		// send a game refresh packet to each battle.net connection

		bool Refreshed = false;

		uint32_t slotstotal = m_Slots.size( );
		uint32_t slotsopen = GetSlotsOpen();

		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			// don't queue a game refresh message if the queue contains more than 1 packet because they're very low priority

			if( (*i)->GetOutPacketsQueued( ) <= 1 )
			{
				(*i)->QueueGameRefresh( m_GameState, m_GameName, string( ), m_Map, m_SaveGame, GetTime( ) - m_CreationTime, m_HostCounter, slotstotal, slotsopen );
				Refreshed = true;
			}
		}

		// only print the "game refreshed" message if we actually refreshed on at least one battle.net server

		if( m_RefreshMessages && Refreshed )
			SendAllChat( m_GHost->m_Language->GameRefreshed( ) );

		m_LastRefreshTime = GetTime( );
	}

	// send more map data

	if( !m_GameLoading && !m_GameLoaded && GetTicks( ) - m_LastDownloadCounterResetTicks >= 1000 )
	{
		// hackhack: another timer hijack is in progress here
		// since the download counter is reset once per second it's a great place to update the slot info if necessary

		if( m_SlotInfoChanged )
			SendAllSlotInfo( );

//		m_DownloadCounter = 0;
		m_LastDownloadCounterResetTicks = GetTicks( );
	}

/*
	if( !m_GameLoading && !m_GameLoaded && GetTicks( ) - m_LastDownloadTicks > 100 )
	{
		uint32_t Downloaders = 0;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( (*i)->GetDownloadStarted( ) && !(*i)->GetDownloadFinished( ) )
			{
				Downloaders++;

				if( m_GHost->m_MaxDownloaders > 0 && Downloaders > m_GHost->m_MaxDownloaders )
					break;

				// send up to 100 pieces of the map at once so that the download goes faster
				// if we wait for each MAPPART packet to be acknowledged by the client it'll take a long time to download
				// this is because we would have to wait the round trip time (the ping time) between sending every 1442 bytes of map data
				// doing it this way allows us to send at least 140 KB in each round trip interval which is much more reasonable
				// the theoretical throughput is [140 KB * 1000 / ping] in KB/sec so someone with 100 ping (round trip ping, not LC ping) could download at 1400 KB/sec
				// note: this creates a queue of map data which clogs up the connection when the client is on a slower connection (e.g. dialup)
				// in this case any changes to the lobby are delayed by the amount of time it takes to send the queued data (i.e. 140 KB, which could be 30 seconds or more)
				// for example, players joining and leaving, slot changes, chat messages would all appear to happen much later for the low bandwidth player
				// note: the throughput is also limited by the number of times this code is executed each second
				// e.g. if we send the maximum amount (140 KB) 10 times per second the theoretical throughput is 1400 KB/sec
				// therefore the maximum throughput is 1400 KB/sec regardless of ping and this value slowly diminishes as the player's ping increases
				// in addition to this, the throughput is limited by the configuration value bot_maxdownloadspeed
				// in summary: the actual throughput is MIN( 140 * 1000 / ping, 1400, bot_maxdownloadspeed ) in KB/sec assuming only one player is downloading the map

				uint32_t MapSize = UTIL_ByteArrayToUInt32( m_Map->GetMapSize( ), false );

				while( (*i)->GetLastMapPartSent( ) < (*i)->GetLastMapPartAcked( ) + 1442 * 100 && (*i)->GetLastMapPartSent( ) < MapSize )
				{
					if( (*i)->GetLastMapPartSent( ) == 0 )
					{
						// overwrite the "started download ticks" since this is the first time we've sent any map data to the player
						// prior to this we've only determined if the player needs to download the map but it's possible we could have delayed sending any data due to download limits

						(*i)->SetStartedDownloadingTicks( GetTicks( ) );
					}

					// limit the download speed if we're sending too much data
					// the download counter is the # of map bytes downloaded in the last second (it's reset once per second)

					if( m_GHost->m_MaxDownloadSpeed > 0 && m_DownloadCounter > m_GHost->m_MaxDownloadSpeed * 1024 )
						break;

					Send( *i, m_Protocol->SEND_W3GS_MAPPART( GetHostPID( ), (*i)->GetPID( ), (*i)->GetLastMapPartSent( ), m_Map->GetMapData( ) ) );
					(*i)->SetLastMapPartSent( (*i)->GetLastMapPartSent( ) + 1442 );
					m_DownloadCounter += 1442;
				}
			}
		}

		m_LastDownloadTicks = GetTicks( );
	}
*/
	// announce every m_AnnounceInterval seconds

	if( !m_AnnounceMessage.empty( ) && !m_CountDownStarted && GetTime( ) - m_LastAnnounceTime >= m_AnnounceInterval )
	{
		{
			string msg = m_AnnounceMessage;
			string msgl = string();
			string :: size_type lstart;
			while ((lstart = msg.find("|"))!=string :: npos)
			{
				msgl = msg.substr(0,lstart);
				msg = msg.substr(lstart+1,msg.length()-lstart-1);
				SendAllChat(msgl);
			}
			SendAllChat(msg);
		}
		m_LastAnnounceTime = GetTime( );
	}

	// kick players who don't spoof check within 20 seconds when spoof checks are required and the game is autohosted

	if( !m_CountDownStarted && m_GHost->m_RequireSpoofChecks && m_GameState == GAME_PUBLIC && !m_GHost->m_AutoHostGameName.empty( ) && m_GHost->m_AutoHostMaximumGames != 0 && m_GHost->m_AutoHostAutoStartPlayers != 0 && m_AutoStartPlayers != 0 )
	{
		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetSpoofed( ) && GetTime( ) - (*i)->GetJoinTime( ) >= 20 )
			{
				(*i)->SetDeleteMe( true );
				(*i)->SetLeftReason( m_GHost->m_Language->WasKickedForNotSpoofChecking( ) );
				(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
				OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
			}
		}
	}

	// try to auto start every 10 seconds

	if( !m_CountDownStarted && m_AutoStartPlayers != 0 && GetTime( ) - m_LastAutoStartTime >= 10 )
	{
		StartCountDownAuto( m_GHost->m_RequireSpoofChecks );
		m_LastAutoStartTime = GetTime( );
	}
	// display player count info every 193 seconds, we prefer the prime numbers

	if ( !m_CountDownStarted && GetTime( ) - m_LastInfoShow >= 193 )
	{		
		SendAllChat("If no admin & no owner in lobby, to start the game all should type !go. Tat ca cung bam !go de nhanh vao game.");
		m_LastInfoShow = GetTime( );
	}
	// display thegenmaps.tk forum every 263 seconds
	
	if ( !m_CountDownStarted && GetTime( ) - m_LastGenInfoShow >= 263 )
	{
		SendAllChat("View game list in TheGenMaps.tk for PRO gaming exp. Join our Garena+ GroupID 56934.");		
		m_LastGenInfoShow = GetTime( );
	}
	// display !owner every 313 seconds
	
	if ( !m_CountDownStarted && GetTime( ) - m_LastOwnerInfoShow >= 313 )
	{
		SendAllChat("Type !owner to gain control of lobby: !a, !as, !close, !closeall, !open, !openall, !holds, !hold, !unhold, !startn");		
		m_LastOwnerInfoShow = GetTime( );
	}
	// end game countdown every 1000 ms
	if( m_GameEndCountDownStarted && GetTicks( ) - m_GameEndLastCountDownTicks >= 1000 )
	{
		if( m_GameEndCountDownCounter > 0 )
		{
			// we use a countdown counter rather than a "finish countdown time" here because it might alternately round up or down the count
			// this sometimes resulted in a countdown of e.g. "6 5 3 2 1" during my testing which looks pretty dumb
			// doing it this way ensures it's always "5 4 3 2 1" but each interval might not be *exactly* the same length

			SendAllChat( UTIL_ToString( m_GameEndCountDownCounter ) + ". . ." );
			m_GameEndCountDownCounter--;
		}
		else if( !m_GameLoading && m_GameLoaded )
		{
			m_GameEndCountDownStarted = false;
			StopPlayers( "was disconnected (admin ended game)" );
		}

		m_GameEndLastCountDownTicks = GetTicks( );
	}
	// countdown every 500 ms

	uint32_t waittime = 500;
	if (m_NormalCountdown)
		waittime = 1200;

	if( m_CountDownStarted && GetTicks( ) - m_LastCountDownTicks >= waittime )
	{
		if( m_CountDownCounter > 0 )
		{
			// we use a countdown counter rather than a "finish countdown time" here because it might alternately round up or down the count
			// this sometimes resulted in a countdown of e.g. "6 5 3 2 1" during my testing which looks pretty dumb
			// doing it this way ensures it's always "5 4 3 2 1" but each interval might not be *exactly* the same length

			if (!m_NormalCountdown)
			SendAllChat( UTIL_ToString( m_CountDownCounter ) + ". . ." );
			m_CountDownCounter--;
		}
		else if( !m_GameLoading && !m_GameLoaded )
			EventGameStarted( );

		m_LastCountDownTicks = GetTicks( );
	}

	// update pings in the GUI
	if (GetTime()>m_CreationTime+5 && !m_PingsUpdated)
		if (GetTime()>m_LastPlayerJoinedTime+3 && !m_GameLoading && !m_GameLoaded)
		{
			m_PingsUpdated = true;
			m_GHost->UDPChatSend("|lobbyupdate");
		}

	// check if no one has joined the game for a long time (25 sec default) and rehost
	bool LoggedIn = false;
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if ((*i)->GetServer()==m_Server)
			LoggedIn = (*i)->GetLoggedIn();
		// the game creation message will be sent on the next refresh
	}

	if (!(m_autohosted && m_GHost->m_AutoHostLocal))
	if (!m_DownloadOnlyMode)
	if (m_VirtualHostName!="|cFFC04040Admin")
	if (m_GameState==GAME_PUBLIC)
	if (m_GHost->m_AutoRehostDelay!=0)
//	if (LoggedIn)
	if (GetTime()>m_CreationTime+5)
	if (GetTime()>m_LastPlayerJoiningTime+m_GHost->m_AutoRehostDelay && !m_GameLoading && !m_GameLoaded && !m_AllSlotsOccupied)
	{
/*		if (m_AutoStartPlayers!=0)
			m_GHost->m_HostCounter++;*/
		ReCalculateTeams();
		m_GameName = m_GHost->IncGameNr(m_GameName);
		m_GHost->m_HostCounter++;
		m_GHost->SaveHostCounter();
		if (m_GHost->m_MaxHostCounter>0)
		if (m_GHost->m_HostCounter>m_GHost->m_MaxHostCounter)
			m_GHost->m_HostCounter = 1;
		m_HostCounter = m_GHost->m_HostCounter;
		m_GHost->m_QuietRehost = true;
		m_RefreshError = false;
		m_Rehost = true;
		AddGameName(m_GameName);
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			(*i)->QueueGameUncreate( );
			(*i)->QueueEnterChat( );

			// the game creation message will be sent on the next refresh
		}
		m_CreationTime = GetTime( );
		m_LastRefreshTime = GetTime( );
		m_LastPlayerJoinedTime = GetTime( )+5;
		m_LastPlayerJoiningTime = GetTime( )+5;
	}

	// check if the lobby is "abandoned" and needs to be closed since it will never start

	if(m_VirtualHostName!="|cFFC04040Admin" && !m_GameLoading && !m_GameLoaded && m_AutoStartPlayers == 0 && m_GHost->m_LobbyTimeLimitMax > 0 )
	{
		// check if we've hit the time limit

		if( GetTime( ) - m_CreationTime >= m_GHost->m_LobbyTimeLimitMax * 60 && !m_DownloadOnlyMode )
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] is over (lobby time limit hit)" );
			return true;
		}
	}

	// check if the lobby is "abandoned" and needs to be closed since it will never start

	if( !m_GameLoading && !m_GameLoaded && (m_AutoStartPlayers == 0) && m_GHost->m_LobbyTimeLimit > 0 )
	{
		// check if there's a player with reserved status in the game

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if (m_AutoStartPlayers == 0) {

				if( (*i)->GetReserved( ) )
					m_LastReservedSeen = GetTime( );

			} else {

				// If autostart is on, and there is at least one player, the above for loop will run.
				m_LastReservedSeen = GetTime( );
				break;

			}

//			if( (*i)->GetReserved( ) )
//				m_LastReservedSeen = GetTime( );
		}

		// check if we've hit the time limit

		if( GetTime( ) - m_LastReservedSeen >= m_GHost->m_LobbyTimeLimit * 60 && !m_DownloadOnlyMode )
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] is over (lobby time limit hit)" );
			return true;
		}
	}

	// check if the game is loaded

	if( m_GameLoading )
	{
		bool FinishedLoading = true;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			FinishedLoading = (*i)->GetFinishedLoading( );

			if( !FinishedLoading )
				break;
		}

		if( FinishedLoading )
		{
			m_LastActionSentTicks = GetTicks( );
			m_GameLoading = false;
			m_GameLoaded = true;
			EventGameLoaded( );
		}
		else
		{
			// reset the "lag" screen (the load-in-game screen) every 30 seconds

			if( m_LoadInGame && GetTime( ) - m_LastLagScreenResetTime >= 30 )
			{
				bool UsingGProxy = false;

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					if( (*i)->GetGProxy( ) )
						UsingGProxy = true;
				}

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					if( (*i)->GetFinishedLoading( ) )
					{
						// stop the lag screen

						for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
						{
							if( !(*j)->GetFinishedLoading( ) )
								Send( *i, m_Protocol->SEND_W3GS_STOP_LAG( *j, true ) );
						}

						// send an empty update
						// this resets the lag screen timer but creates a rather annoying problem
						// in order to prevent a desync we must make sure every player receives the exact same "desyncable game data" (updates and player leaves) in the exact same order
						// unfortunately we cannot send updates to players who are still loading the map, so we buffer the updates to those players (see the else clause a few lines down for the code)
						// in addition to this we must ensure any player leave messages are sent in the exact same position relative to these updates so those must be buffered too

						if( UsingGProxy && !(*i)->GetGProxy( ) )
						{
							// we must send empty actions to non-GProxy++ players
							// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
							// empty actions are used to extend the time a player can use when reconnecting

							for( unsigned char j = 0; j < m_GProxyEmptyActions; j++ )
								Send( *i, m_Protocol->SEND_W3GS_INCOMING_ACTION( queue<CIncomingAction *>( ), 0 ) );
						}

						Send( *i, m_Protocol->SEND_W3GS_INCOMING_ACTION( queue<CIncomingAction *>( ), 0 ) );

						// start the lag screen

						Send( *i, m_Protocol->SEND_W3GS_START_LAG( m_Players, true ) );
					}
					else
					{
						// buffer the empty update since the player is still loading the map

						if( UsingGProxy && !(*i)->GetGProxy( ) )
						{
							// we must send empty actions to non-GProxy++ players
							// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
							// empty actions are used to extend the time a player can use when reconnecting

							for( unsigned char j = 0; j < m_GProxyEmptyActions; j++ )
								(*i)->AddLoadInGameData( m_Protocol->SEND_W3GS_INCOMING_ACTION( queue<CIncomingAction *>( ), 0 ) );
						}

						(*i)->AddLoadInGameData( m_Protocol->SEND_W3GS_INCOMING_ACTION( queue<CIncomingAction *>( ), 0 ) );
					}
				}

				// add actions to replay

				if( m_Replay )
				{
					if( UsingGProxy )
					{
						for( unsigned char i = 0; i < m_GProxyEmptyActions; i++ )
							m_Replay->AddTimeSlot( 0, queue<CIncomingAction *>( ) );
					}

					m_Replay->AddTimeSlot( 0, queue<CIncomingAction *>( ) );
				}

				// Warcraft III doesn't seem to respond to empty actions

				/* if( UsingGProxy )
					m_SyncCounter += m_GProxyEmptyActions;

				m_SyncCounter++; */
				m_LastLagScreenResetTime = GetTime( );
			}
		}
	}

	// keep track of the largest sync counter (the number of keepalive packets received by each player)
	// if anyone falls behind by more than m_SyncLimit keepalives we start the lag screen

	if( m_GameLoaded )
	{
		// calculate the largest sync counter

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( (*i)->GetSyncCounter( ) > m_MaxSyncCounter )
				m_MaxSyncCounter = (*i)->GetSyncCounter( );
		}

		// check if anyone has started lagging
		// we consider a player to have started lagging if they're more than m_SyncLimit keepalives behind

		if( !m_Lagging )
		{
			string LaggingString;

			m_MaxSync = 0;
			uint32_t Sync = 0;
			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				Sync = m_SyncCounter - (*i)->GetSyncCounter( );
				if (Sync > m_MaxSync)
				{
					m_MaxSync = Sync;
					m_MaxSyncUser = (*i)->GetName();
				}
				if( Sync > m_SyncLimit )
				{
					(*i)->SetLagging( true );
		// calculate drop vote ticks
					uint32_t d = 5;
					if (m_GHost->m_DropVoteTime<45)
						d = (45 * 1000)-m_GHost->m_DropVoteTime*1000 ;

					(*i)->SetStartedLaggingTicks( GetTicks( )-d );
					m_Lagging = true;
					m_StartedLaggingTime = GetTime( );

					if( LaggingString.empty( ) )
						LaggingString = (*i)->GetName( );
					else
						LaggingString += ", " + (*i)->GetName( );
				}
			}

			if( m_Lagging )
			{
				// start the lag screen

				CONSOLE_Print( "[GAME: " + m_GameName + "] started lagging on [" + LaggingString + "]" );
				SendAll( m_Protocol->SEND_W3GS_START_LAG( m_Players ) );

				// reset everyone's drop vote

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					(*i)->SetDropVote( false );

				m_LastLagScreenResetTime = GetTime( );

				// get current time to use for the drop vote

				m_LagScreenTime = GetTime();
			}
		}

		if( m_Lagging )
		{
			bool UsingGProxy = false;

			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( (*i)->GetGProxy( ) )
					UsingGProxy = true;
			}

			uint32_t WaitTime = 60;

			if( UsingGProxy )
				WaitTime = ( m_GProxyEmptyActions + 1 ) * 60;

			if( GetTime( ) - m_StartedLaggingTime >= WaitTime )
				StopLaggers( m_GHost->m_Language->WasAutomaticallyDroppedAfterSeconds( UTIL_ToString( WaitTime ) ) );

			// we cannot allow the lag screen to stay up for more than ~65 seconds because Warcraft III disconnects if it doesn't receive an action packet at least this often
			// one (easy) solution is to simply drop all the laggers if they lag for more than 60 seconds
			// another solution is to reset the lag screen the same way we reset it when using load-in-game
			// this is required in order to give GProxy++ clients more time to reconnect

			if( GetTime( ) - m_LastLagScreenResetTime >= 60 )
			{
				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					// stop the lag screen

					for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
					{
						if( (*j)->GetLagging( ) )
							Send( *i, m_Protocol->SEND_W3GS_STOP_LAG( *j ) );
					}

					// send an empty update
					// this resets the lag screen timer

					if( UsingGProxy && !(*i)->GetGProxy( ) )

					{
						// we must send additional empty actions to non-GProxy++ players
						// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
						// empty actions are used to extend the time a player can use when reconnecting

						for( unsigned char j = 0; j < m_GProxyEmptyActions; j++ )
							Send( *i, m_Protocol->SEND_W3GS_INCOMING_ACTION( queue<CIncomingAction *>( ), 0 ) );
					}

					Send( *i, m_Protocol->SEND_W3GS_INCOMING_ACTION( queue<CIncomingAction *>( ), 0 ) );

					// start the lag screen

					Send( *i, m_Protocol->SEND_W3GS_START_LAG( m_Players ) );
				}

				// add actions to replay

				if( m_Replay )
				{
					if( UsingGProxy )
					{
						for( unsigned char i = 0; i < m_GProxyEmptyActions; i++ )
							m_Replay->AddTimeSlot( 0, queue<CIncomingAction *>( ) );
					}

					m_Replay->AddTimeSlot( 0, queue<CIncomingAction *>( ) );
				}

				// Warcraft III doesn't seem to respond to empty actions

				/* if( UsingGProxy )
					m_SyncCounter += m_GProxyEmptyActions;

				m_SyncCounter++; */
				m_LastLagScreenResetTime = GetTime( );
			}

			// check if anyone has stopped lagging normally
			// we consider a player to have stopped lagging if they're less than half m_SyncLimit keepalives behind

			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( (*i)->GetLagging( ) && m_SyncCounter - (*i)->GetSyncCounter( ) < m_SyncLimit / 2 )
				{
					// stop the lag screen for this player

					CONSOLE_Print( "[GAME: " + m_GameName + "] stopped lagging on [" + (*i)->GetName( ) + "]" );
					SendAll( m_Protocol->SEND_W3GS_STOP_LAG( *i ) );
					(*i)->SetLagging( false );
					(*i)->SetStartedLaggingTicks( 0 );
				}
			}

			// check if everyone has stopped lagging

			bool Lagging = false;

			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( (*i)->GetLagging( ) )
					Lagging = true;
			}

			m_Lagging = Lagging;

			// reset m_LastActionSentTicks because we want the game to stop running while the lag screen is up

			m_LastActionSentTicks = GetTicks( );

			// keep track of the last lag screen time so we can avoid timing out players

			m_LastLagScreenTime = GetTime( );
		}
	}

	// send actions every m_Latency milliseconds
	// actions are at the heart of every Warcraft 3 game but luckily we don't need to know their contents to relay them
	// we queue player actions in EventPlayerAction then just resend them in batches to all players here

	if (!m_GHost->m_newLatency)
	if( m_GameLoaded && !m_Lagging && GetTicks( ) - m_LastActionSentTicks >= m_DynamicLatency )
		SendAllActions( );

	if (m_GHost->m_newLatency)
	if( m_GameLoaded && !m_Lagging && GetTicks( ) - m_LastActionSentTicks >= m_DynamicLatency - m_LastActionLateBy )
		SendAllActions( );

	// update dynamic latency
	if (m_UseDynamicLatency)
		SetDynamicLatency();
	else
		m_DynamicLatency = m_Latency;

	// expire the end vote

	if( m_EndRequested && GetTicks( ) - m_EndRequestedTicks >= 180*1000 )
	{
		SendAllChat("End request expired." );
		m_EndRequested = false;
		m_EndRequestedTicks = 0;
	}

	// expire the rmk vote

	if( !m_RmkVotePlayer.empty( ) && GetTime( ) - m_StartedRmkVoteTime >= 180 )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] rmk started by player [" + m_RmkVotePlayer + "] expired" );
		SendAllChat("Rmk vote expired." );
		m_RmkVotePlayer.clear( );
		m_StartedRmkVoteTime = 0;
	}

	// expire the votekick

	if( !m_KickVotePlayer.empty( ) && GetTime( ) - m_StartedKickVoteTime >= 60 )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] expired" );
		SendAllChat( m_GHost->m_Language->VoteKickExpired( m_KickVotePlayer ) );
		m_KickVotePlayer.clear( );
		m_StartedKickVoteTime = 0;
	}
	
	// expire the votestart
	
	if( m_StartedVoteStartTime != 0 && GetTime( ) - m_StartedVoteStartTime >= 120 )
	  {
	    CONSOLE_Print( "[GAME: " + m_GameName + "] votestart expired" );
	    SendAllChat( "Votestart expired (120 seconds without pass)." );
	    m_StartedVoteStartTime = 0;
	  }

	// look through each player's warn count and display it to them if > 0

	if (m_GameLoadedTime!=0 && !m_AllPlayersWarnChecked)
		if (GetTime()>= m_GameLoadedTime + m_GHost->m_InformAboutWarnsPrintout + m_LastWarnCheck)
	{
		m_AllPlayersWarnChecked = true;

		CGamePlayer *player = NULL;
		
		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++)
		{
			if( (*i)->GetDeleteMe() == false && (*i)->GetWarnChecked() == false  )
			{
				player = *i;
			}
		}

		if( player != NULL )
		{
			m_LastWarnCheck++;
			m_AllPlayersWarnChecked = false;
			player->SetWarnChecked(true);

			m_WarnCounts.push_back( m_GHost->m_DB->ThreadedWarnCount( player->GetName( ), 1 ) );
		}
	}

	// show game start text
	// read from gameloaded.txt if available

	if (m_GameLoadedTime!=0 && !m_GameLoadedMessage)
	if (GetTime()>=m_GameLoadedTime+m_GHost->m_GameLoadedPrintout)
	{
		CONSOLE_Print("[GAME: " + m_GameName + "] loading gameloaded.txt");
		m_GameLoadedMessage = true;
		ifstream inn;
		inn.open( "gameloaded.txt" );

		if( !inn.fail( ) )
		{
			// don't print more than 8 lines

			uint32_t Count = 0;
			string Line;

			while( !inn.eof( ) && Count < 8 )
			{
				getline( inn, Line );

				if( Line.empty( ) )
					SendAllChat( " " );
				else
					SendAllChat( Line );

				if( inn.eof( ) )
					break;

				Count++;
			}

			inn.close( );
		} else
			CONSOLE_Print("[GAME: " + m_GameName + "] gameloaded.txt load failed");

	}

	// check if switch expired
	if (m_SwitchTime!=0)
		if (GetTime()-m_SwitchTime>60)
		{
			m_SwitchTime = 0;
			CONSOLE_Print("[GAME: " + m_GameName + "] Switch expired");
		}

	bool lessthanminpercent = false;
	bool lessthanminplayers = false;
	float remainingpercent = (float)m_Players.size()*100/(float)m_PlayersatStart;

	if (!m_GameOverCanceled)
	{
		// start the gameover timer if world tree/frozen throne has fallen
		if (m_GHost->m_gameoverbasefallen>0 && m_GameOverTime == 0 && m_GameEnded)
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (a base has fallen)");
			m_GameOverTime = GetTime( )-60+m_GHost->m_gameoverbasefallen;
		}

		// start the gameover timer if we have less than the minimum percent of players remaining.
		if (!m_GameEnded && m_GHost->m_gameoverminpercent!=0 && ( !m_GameLoading && m_GameLoaded ))
			if (remainingpercent<m_GHost->m_gameoverminpercent)
				lessthanminpercent = true;
		if (lessthanminpercent && m_GameOverTime == 0)
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (less than "+UTIL_ToString(m_GHost->m_gameoverminpercent)+"% )"+" "+string(1,m_GHost->m_CommandTrigger)+"override to cancel" );
			SendAllChat("Game over in 60 seconds, "+ UTIL_ToString(remainingpercent)+"% remaining ( < "+UTIL_ToString(m_GHost->m_gameoverminpercent)+"% ) "+string(1,m_GHost->m_CommandTrigger)+"override to cancel");
			m_GameOverTime = GetTime( );
		}

		// start the gameover timer if there's less than m_gameoverminplayers players left (and we had at least 1 leaver)

		if (!m_GameEnded && m_GHost->m_gameoverminplayers!=0)
			if (m_Players.size( ) < m_GHost->m_gameoverminplayers && m_PlayersatStart>m_Players.size() && ( !m_GameLoading && m_GameLoaded ) )
				lessthanminplayers = true;
		if (lessthanminplayers && m_GameOverTime == 0)
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (one player left)"+" "+string(1, m_GHost->m_CommandTrigger)+"override to cancel" );
			SendAllChat("Game over in 12 seconds, "+ UTIL_ToString(m_Players.size())+" remaining ( < "+UTIL_ToString(m_GHost->m_gameoverminplayers)+" ) "+string(1, m_GHost->m_CommandTrigger)+"override to cancel");
			m_GameOverTime = GetTime( );
		}

		if (!m_GameLoading && m_GameLoaded)
		if (!m_GameOverDiffCanceled)
		if (!m_GameEnded)
		if (!lessthanminplayers && !lessthanminpercent)
		if (m_Team1<2 || m_Team2<2)
			m_GameOverDiffCanceled = true;

		// start the gameover timer if there's only one player left

		if( m_Players.size( ) == 1 && m_FakePlayers.size( ) == 0 && m_GameOverTime == 0 && ( m_GameLoading || m_GameLoaded ) )
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (one player left)" );
			m_GameOverTime = GetTime( );
		}
		
		// start the gameover timer if team unbalance is greater than m_gameovermaxteamdifference
		// ex: m_gameovermaxteamdifference = 2, if one team has -3, start game over.
		if (!m_GameLoading && m_GameLoaded)
		if (!m_GameOverDiffCanceled)
		if (!m_GameEnded)
		if (!m_Switched) // disable gameoverteamdifference if someone has switched
		if (!lessthanminplayers && !lessthanminpercent)
		if (m_GetMapNumTeams==2 && m_GHost->m_gameovermaxteamdifference!=0 && m_Players.size()<m_PlayersatStart && m_Players.size()>1)
		{
			bool unbalanced = false;

			if (m_TeamDiff > m_GHost->m_gameovermaxteamdifference)
				unbalanced = true;

			if (m_GameOverTime!=0 && !unbalanced)
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer stoped (rebalanced team)" );
				SendAllChat("Game over averted, team rebalanced");
				m_GameOverTime = 0;
			}


			if (unbalanced && m_GameOverTime==0)
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (unbalanced team)" );
				SendAllChat("Game over in 120 seconds, rebalance quickly!, max team difference is "+UTIL_ToString(m_GHost->m_gameovermaxteamdifference)+" "+string(1, m_GHost->m_CommandTrigger)+"override to cancel");
				m_GameOverTime = GetTime( )+60;				
			}
		}

		// finish the gameover timer

		if( m_GameOverTime != 0 && GetTime( ) - m_GameOverTime >= 12 )
		{
			bool AlreadyStopped = true;

			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( !(*i)->GetDeleteMe( ) )
				{
					AlreadyStopped = false;
					break;
				}
			}

			if( !AlreadyStopped )
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] is over (gameover timer finished)" );
				StopPlayers( "was disconnected (gameover timer finished)" );
			}
		}
	}

	if ( !m_DownloadOnlyMode)
	if( m_GHost->m_LobbyTimeLimit && (m_VirtualHostName!="|cFFC04040Admin") && !m_GameLoading && !m_GameLoaded )	
	{
	// is there a player with reserved status in the game?
		if (m_Players.size()>0)
		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++)
		{
			if( (*i)->GetReserved( ) )
			{
				m_LastReservedSeen = GetTime( );
			}
		}
	//check if we have hit the timelimit
		if ( GetTime( ) > m_LastReservedSeen + m_GHost->m_LobbyTimeLimit*60 )
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] is over (lobby timelimit hit)" );
			for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
			{
				(*i)->QueueChatCommand( "[GAME: " + m_GameName + "] is over (lobby timelimit hit)" );
				
				if( (*i)->GetServer( ) == GetCreatorServer( ) )
					(*i)->QueueChatCommand( "[GAME: " + m_GameName + "] is over (lobby timelimit hit)", GetCreatorName( ), true );
			}
		m_Exiting = true;	
		}
	}

	// censor muted process
	ProcessCensorMuted();

	// check how many slots are unoccupied and announce if needed
	if (m_GHost->m_LobbyAnnounceUnoccupied)
	if (!m_GameLoaded && !m_GameLoading && m_AutoStartPlayers==0)
	if (GetSlotsOpen()!=m_LastSlotsUnoccupied)
	{
		m_LastSlotsUnoccupied = GetSlotsOpen();
		if (m_LastSlotsUnoccupied==1 || m_LastSlotsUnoccupied==2)
			SendAllChat("+"+UTIL_ToString(m_LastSlotsUnoccupied));
	}


	// check if all slots are no longer occupied
	if (!m_GameLoaded && !m_GameLoading)
	if (m_AllSlotsOccupied)
	if (GetSlotsOpen()!=0)
	{
		m_AllSlotsOccupied = false;
		m_AllSlotsAnnounced = false;
	}

	// if all slots occupied for 3 seconds, announce in the lobby
	if (!m_CountDownStarted && !m_GameLoaded && !m_GameLoading)
	if (GetSlotsOpen()==0 && m_AllSlotsOccupied && !m_AllSlotsAnnounced)
	if (GetTime()-m_SlotsOccupiedTime>3)
	{
		m_AllSlotsAnnounced = true;
		string Pings;
		string Pings2;
		uint32_t Ping;
		bool samecountry=true;
		string CN, CNL;

		Pings = "All type !go now. All slots occupied. ";
		Pings2 = "All write !go now. All slots occupied. ";

		// copy the m_Players vector so we can sort by descending ping so it's easier to find players with high pings

		vector<CGamePlayer *> SortedPlayers = m_Players;
		sort( SortedPlayers.begin( ), SortedPlayers.end( ), CGamePlayerSortDescByPing( ) );

		//		string FirstC;

		for( vector<CGamePlayer *> :: iterator i = SortedPlayers.begin( ); i != SortedPlayers.end( ); i++ )
		{
			//Pings += (*i)->GetName( );
			//Pings += ": ";
			bool skipP;

			CN = m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );
			if (CNL=="")
				CNL=CN;
			else 
				if (CN!=CNL)
					samecountry=false;

			if( (*i)->GetNumPings( ) > 0 )
			{
				Ping=(*i)->GetPing( m_GHost->m_LCPings );
				if (Ping>5)
				{
					skipP = false;
					Pings += UTIL_ToString( Ping );
					Pings += "ms (";
					Pings += CN;
					Pings += ")";
					Pings2 += UTIL_ToString( Ping );
					Pings2 += "ms";
				} else
				{
					skipP = true;
				}
			}
			else
			{
				skipP = false;
				Pings += "N/A (";
				Pings += CN;
				Pings += ")";
			}

			if( i != SortedPlayers.end( ) - 1  && !skipP)
			{
				Pings += ", ";
				Pings2 += ", ";
			}
		}
		Pings2 += " are all from ("+CNL+")";

		if (samecountry)
			SendAllChat( Pings2 );
		else
			SendAllChat( Pings );
	}


	// check if we're rehosting

	if (m_EndGameTime>0)
	if (GetTime() - m_EndGameTime>=3)
	{
		m_Exiting = true;
		m_GHost->newGame = true;
	}

	// end the game if there aren't any players left

	if( m_Players.empty( ) && ( m_GameLoading || m_GameLoaded ) )
	{
		if( !m_Saving )
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] is over (no players left)" );
			SaveGameData( );
			m_Saving = true;
		}
		else if( IsGameDataSaved( ) )
			return true;
	}

	// accept new connections

	if( m_Socket )
	{
		CTCPSocket *NewSocket = m_Socket->Accept( (fd_set *)fd );

		if( NewSocket )
		{
			// check the IP blacklist

			if( m_IPBlackList.find( NewSocket->GetIPString( ) ) == m_IPBlackList.end( ) )
			{
				if( m_GHost->m_TCPNoDelay )
					NewSocket->SetNoDelay( true );

				m_Potentials.push_back( new CPotentialPlayer( m_Protocol, this, NewSocket ) );
			}
			else
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] rejected connection from [" + NewSocket->GetIPString( ) + "] due to blacklist" );
				delete NewSocket;
			}
		}

		if( m_Socket->HasError( ) )
			return true;
	}

	return m_Exiting;
}

void CBaseGame :: UpdatePost( void *send_fd )
{
	// we need to manually call DoSend on each player now because CGamePlayer :: Update doesn't do it
	// this is in case player 2 generates a packet for player 1 during the update but it doesn't get sent because player 1 already finished updating
	// in reality since we're queueing actions it might not make a big difference but oh well

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetSocket( ) )
			(*i)->GetSocket( )->DoSend( (fd_set *)send_fd );
	}

	for( vector<CPotentialPlayer *> :: iterator i = m_Potentials.begin( ); i != m_Potentials.end( ); i++ )
	{
		if( (*i)->GetSocket( ) )
			(*i)->GetSocket( )->DoSend( (fd_set *)send_fd );
	}
}

void CBaseGame :: Send( CGamePlayer *player, BYTEARRAY data )
{
	if( player )
		player->Send( data );
}

void CBaseGame :: Send( unsigned char PID, BYTEARRAY data )
{
	Send( GetPlayerFromPID( PID ), data );
}

void CBaseGame :: Send( BYTEARRAY PIDs, BYTEARRAY data )
{
	for( unsigned int i = 0; i < PIDs.size( ); i++ )
		Send( PIDs[i], data );
}

void CBaseGame :: SendAlly( unsigned char PID, BYTEARRAY data )
{
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()==m_Slots[GetSIDFromPID(PID)].GetTeam())
			(*i)->Send( data );
	}
}

void CBaseGame :: SendAdmin( unsigned char PID, BYTEARRAY data )
{
	bool isAdmin = false;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		isAdmin = IsAdmin((*i)->GetName()) || IsRootAdmin((*i)->GetName()) || IsOwner((*i)->GetName());
		if (isAdmin)
			(*i)->Send( data );
	}
}

void CBaseGame :: SendEnemy( unsigned char PID, BYTEARRAY data )
{
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()!=m_Slots[GetSIDFromPID(PID)].GetTeam())
			(*i)->Send( data );
	}
}

void CBaseGame :: SendAll( BYTEARRAY data )
{
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		(*i)->Send( data );
}

void CBaseGame :: SendAdmin( BYTEARRAY data )
{
	bool isAdmin = false;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		isAdmin = IsAdmin((*i)->GetName()) || IsRootAdmin((*i)->GetName()) || IsOwner((*i)->GetName());
		if (isAdmin)
			(*i)->Send( data );
	}
}

void CBaseGame :: SendChat( unsigned char fromPID, CGamePlayer *player, string message )
{
	// send a private message to one player - it'll be marked [Private] in Warcraft 3

	if (m_DetourAllMessagesToAdmins)
		if (player)
		{
			bool isAdmin = IsAdmin(player->GetName()) || IsOwner(player->GetName()) || IsRootAdmin(player->GetName());
			if (!isAdmin)
				return;
		}

	if( player )
	{
		if( !m_GameLoading && !m_GameLoaded )
		{
			if( message.size( ) > 220 )
				message = message.substr( 0, 220 );

			Send( player, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, UTIL_CreateByteArray( player->GetPID( ) ), 16, BYTEARRAY( ), message ) );
		}
		else
		{
			unsigned char ExtraFlags[] = { 3, 0, 0, 0 };

			// based on my limited testing it seems that the extra flags' first byte contains 3 plus the recipient's colour to denote a private message

			unsigned char SID = GetSIDFromPID( player->GetPID( ) );

			if( SID < m_Slots.size( ) )
				ExtraFlags[0] = 3 + m_Slots[SID].GetColour( );

			if( message.size( ) > 120 )
				message = message.substr( 0, 120 );

			Send( player, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, UTIL_CreateByteArray( player->GetPID( ) ), 32, UTIL_CreateByteArray( ExtraFlags, 4 ), message ) );
		}
	}
}

void CBaseGame :: SendChat( unsigned char fromPID, unsigned char toPID, string message )
{
	SendChat( fromPID, GetPlayerFromPID( toPID ), message );
}

void CBaseGame :: SendChat( CGamePlayer *player, string message )
{
	SendChat( GetHostPID( ), player, message );
}

void CBaseGame :: SendChat( unsigned char toPID, string message )
{
	SendChat( GetHostPID( ), toPID, message );
}

void CBaseGame :: SendAllChat( unsigned char fromPID, string message )
{
	// send a public message to all players - it'll be marked [All] in Warcraft 3

	if( GetNumHumanPlayers( ) > 0 )
	{
		if( !m_GameLoading && !m_GameLoaded )
		{
			int i=0;
			string msg;
			bool onemoretime=false;
			
			do 
			{
				msg = message.substr( i, 220 );
				if (message.size()-i> 220)
				{
					i=i+220;
					onemoretime=true;
				} else
					onemoretime=false;
				// this is a lobby ghost chat message
				string hname = m_VirtualHostName;
				if (hname.substr(0,2)=="|c")
					hname = hname.substr(10,hname.length()-10);
				m_GHost->UDPChatSend("|lobby "+hname+" "+msg);
				if (m_DetourAllMessagesToAdmins)
					SendAdmin( m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetAdminPIDs( ), 16, BYTEARRAY( ), msg ) );
				else
					SendAll( m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetPIDs( ), 16, BYTEARRAY( ), msg ) );
			} while (onemoretime);
		}
		else
		{
			int i=0;
			string msg;
			bool onemoretime=false;

			do 
			{
				msg = message.substr( i, 120 );
				if (message.size()-i> 120)
				{
					i=i+120;
					onemoretime=true;
				} else
					onemoretime=false;
				// this is an ingame ghost chat message, print it to the console
				if (m_DetourAllMessagesToAdmins)
					SendAdmin( m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetAdminPIDs( ), 32, UTIL_CreateByteArray( (uint32_t)0, false ), msg ) );
				else
					SendAll( m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetPIDs( ), 32, UTIL_CreateByteArray( (uint32_t)0, false ), msg ) );
			} while (onemoretime);


			uint32_t GameNr = GetGameNr();

			string hname = m_VirtualHostName;
			if (hname.substr(0,2)=="|c")
				hname = hname.substr(10,hname.length()-10);

			m_GHost->UDPChatSend("|gamechat "+UTIL_ToString(GameNr) + " 0 1 "+hname+" "+message);

			CONSOLE_Print( "[GAME: " + m_GameName + "] [Local]: " + message );

			if( m_Replay )
				m_Replay->AddChatMessage( fromPID, 32, 0, message );
		}
	}
}

void CBaseGame :: SendAllyChat( unsigned char fromPID, string message )
{
	// send a public message to ally players - it'll be marked [Ally] in Warcraft 3

	if( GetNumPlayers( ) > 0 )
	{
		if( !m_GameLoading && !m_GameLoaded )
		{
			int i=0;
			string msg;
			bool onemoretime=false;

			do 
			{
				msg = message.substr( i, 220 );
				if (message.size()-i> 220)
				{
					i=i+220;
					onemoretime=true;
				} else
					onemoretime=false;
				// this is a lobby ghost chat message
				unsigned char ExtraFlags[] = { 1, 0, 0, 0 };
				string hname = m_VirtualHostName;
				if (hname.substr(0,2)=="|c")
					hname = hname.substr(10,hname.length()-10);
				m_GHost->UDPChatSend("|lobby "+hname+" "+msg);
				unsigned char fteam = m_Slots[GetSIDFromPID(fromPID)].GetTeam();
				SendAlly( fromPID, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetAllyPIDs( fromPID ), 16, UTIL_CreateByteArray( ExtraFlags, 4 ), msg ) );
			} while (onemoretime);
		}
		else
		{
			int i=0;
			string msg;
			bool onemoretime=false;

			do 
			{
				msg = message.substr( i, 120 );
				if (message.size()-i> 120)
				{
					i=i+120;
					onemoretime=true;
				} else
					onemoretime=false;
				// this is an ingame ghost chat message, print it to the console
				unsigned char fteam = m_Slots[GetSIDFromPID(fromPID)].GetTeam();
				unsigned char ExtraFlags[] = { 1, 0, 0, 0 };
				SendAlly( fromPID, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetAllyPIDs( fromPID ), 32, UTIL_CreateByteArray( ExtraFlags, 4 ), msg ) );
			} while (onemoretime);


			uint32_t GameNr = GetGameNr();

			string hname = m_VirtualHostName;
			if (hname.substr(0,2)=="|c")
				hname = hname.substr(10,hname.length()-10);

			unsigned char fteam = m_Slots[GetSIDFromPID(fromPID)].GetTeam();
			m_GHost->UDPChatSend("|gamechat "+UTIL_ToString(GameNr) + " "+UTIL_ToString(fteam)+" 1 "+hname+" "+message);

			CONSOLE_Print( "[GAME: " + m_GameName + "] [Local]: " + message );

			if( m_Replay )
				m_Replay->AddChatMessage( fromPID, 32, 0, message );
		}
	}
}

void CBaseGame :: SendAdminChat( unsigned char fromPID, string message )
{
	// send a public message to admin players - it'll be marked [Ally] in Warcraft 3

	if( GetNumPlayers( ) > 0 )
	{
		if( !m_GameLoading && !m_GameLoaded )
		{
			int i=0;
			string msg;
			bool onemoretime=false;

			do 
			{
				msg = message.substr( i, 220 );
				if (message.size()-i> 220)
				{
					i=i+220;
					onemoretime=true;
				} else
					onemoretime=false;
				// this is a lobby ghost chat message
				unsigned char ExtraFlags[] = { 1, 0, 0, 0 };
				string hname = m_VirtualHostName;
				if (hname.substr(0,2)=="|c")
					hname = hname.substr(10,hname.length()-10);
				m_GHost->UDPChatSend("|lobby "+hname+" "+msg);
				SendAdmin( fromPID, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetAdminPIDs( ), 16, UTIL_CreateByteArray( ExtraFlags, 4 ), msg ) );
			} while (onemoretime);
		}
		else
		{
			int i=0;
			string msg;
			bool onemoretime=false;

			do 
			{
				msg = message.substr( i, 120 );
				if (message.size()-i> 120)
				{
					i=i+120;
					onemoretime=true;
				} else
					onemoretime=false;
				// this is an ingame ghost chat message, print it to the console
				unsigned char ExtraFlags[] = { 1, 0, 0, 0 };
				SendAdmin( fromPID, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( fromPID, GetAdminPIDs( ), 32, UTIL_CreateByteArray( ExtraFlags, 4 ), msg ) );
			} while (onemoretime);


			uint32_t GameNr = GetGameNr();

			string hname = m_VirtualHostName;
			if (hname.substr(0,2)=="|c")
				hname = hname.substr(10,hname.length()-10);

			unsigned char fteam = m_Slots[GetSIDFromPID(fromPID)].GetTeam();
			m_GHost->UDPChatSend("|gamechat "+UTIL_ToString(GameNr) + " "+UTIL_ToString(fteam)+" 1 "+hname+" "+message);

			CONSOLE_Print( "[GAME: " + m_GameName + "] [Local]: " + message );

			if( m_Replay )
				m_Replay->AddChatMessage( fromPID, 32, 0, message );
		}
	}
}

void CBaseGame :: SendAllChat( string message )
{
	SendAllChat( GetHostPID( ), message );
}

void CBaseGame :: SendAllyChat( string message )
{
	SendAllyChat( GetHostPID( ), message );
}

void CBaseGame :: SendAdminChat( string message )
{
	SendAdminChat( GetHostPID( ), message );
}

void CBaseGame :: SendLocalAdminChat( string message )
{
	if( !m_LocalAdminMessages )
		return;

	// send a message to LAN/local players who are admins
	// at the time of this writing it is only possible for the game owner to meet this criteria because being an admin requires spoof checking
	// this is mainly used for relaying battle.net whispers, chat messages, and emotes to these players

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetSpoofed( ) && IsOwner( (*i)->GetName( ) ) && ( UTIL_IsLanIP( (*i)->GetExternalIP( ) ) || UTIL_IsLocalIP( (*i)->GetExternalIP( ), m_GHost->m_LocalAddresses ) ) )
		{
			if( m_VirtualHostPID != 255 )
				SendChat( m_VirtualHostPID, *i, message );
			else
			{
				// make the chat message originate from the recipient since it's not going to be logged to the replay

				SendChat( (*i)->GetPID( ), *i, message );
			}
		}
	}
}

void CBaseGame :: SendAllSlotInfo( )
{
	if( !m_GameLoading && !m_GameLoaded )
	{
		SendAll( m_Protocol->SEND_W3GS_SLOTINFO( m_Slots, m_RandomSeed, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );
		m_SlotInfoChanged = false;
	}
}

void CBaseGame :: SendVirtualHostPlayerInfo( CGamePlayer *player )
{
	if( m_VirtualHostPID == 255 )
		return;

	BYTEARRAY IP;
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );
	Send( player, m_Protocol->SEND_W3GS_PLAYERINFO( m_VirtualHostPID, m_VirtualHostName, IP, IP ) );
}

void CBaseGame :: SendFakePlayerInfo( CGamePlayer *player )
{
	if( m_FakePlayers.empty( ) )
		return;

	BYTEARRAY IP;
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );

	for( vector<unsigned char> :: iterator i = m_FakePlayers.begin( ); i != m_FakePlayers.end( ); ++i )
	{
		Send( player, m_Protocol->SEND_W3GS_PLAYERINFO( *i, "Troll[" + UTIL_ToString( *i ) + "]", IP, IP ) );
	}	
}

void CBaseGame :: SendWTVPlayerInfo( CGamePlayer *player )
{
	if( m_WTVPlayerPID == 255 )
		return;
	
		BYTEARRAY IP;
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );

	//	if (m_GHost->m_wtv && m_Map->GetMapObservers()>=3)
	Send( player, m_Protocol->SEND_W3GS_PLAYERINFO( m_WTVPlayerPID, m_GHost->m_wtvPlayerName, IP, IP ) );
}

void CBaseGame :: SendAllActions( )
{
	bool UsingGProxy = false;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetGProxy( ) )
			UsingGProxy = true;
	}

	uint32_t Latency = m_DynamicLatency;

	if (!m_GHost->m_newLatency)
	{
		Latency = GetTicks( ) - m_LastActionSentTicks;
		m_GameTicks += Latency;
	} else
	m_GameTicks += m_DynamicLatency;

	if( UsingGProxy )
	{
		// we must send empty actions to non-GProxy++ players
		// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
		// empty actions are used to extend the time a player can use when reconnecting

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetGProxy( ) )
			{
				for( unsigned char j = 0; j < m_GProxyEmptyActions; j++ )
					Send( *i, m_Protocol->SEND_W3GS_INCOMING_ACTION( queue<CIncomingAction *>( ), 0 ) );
			}
		}
	}

	// add actions to replay

	if( m_Replay )
	{
		if( UsingGProxy )
		{
			for( unsigned char i = 0; i < m_GProxyEmptyActions; i++ )
				m_Replay->AddTimeSlot( 0, queue<CIncomingAction *>( ) );
		}

		m_Replay->AddTimeSlot( m_Latency, m_Actions );
	}

	// Warcraft III doesn't seem to respond to empty actions

	/* if( UsingGProxy )
	m_SyncCounter += m_GProxyEmptyActions; */
	m_SyncCounter++;

	// we aren't allowed to send more than 1460 bytes in a single packet but it's possible we might have more than that many bytes waiting in the queue

	if( !m_Actions.empty( ) )
	{
		// we use a "sub actions queue" which we keep adding actions to until we reach the size limit
		// start by adding one action to the sub actions queue

		queue<CIncomingAction *> SubActions;
		CIncomingAction *Action = m_Actions.front( );
		m_Actions.pop( );
		SubActions.push( Action );
		uint32_t SubActionsLength = Action->GetLength( );

		while( !m_Actions.empty( ) )
		{
			Action = m_Actions.front( );
			m_Actions.pop( );

			// check if adding the next action to the sub actions queue would put us over the limit (1452 because the INCOMING_ACTION and INCOMING_ACTION2 packets use an extra 8 bytes)

			if( SubActionsLength + Action->GetLength( ) > 1452 )
			{
				// we'd be over the limit if we added the next action to the sub actions queue
				// so send everything already in the queue and then clear it out
				// the W3GS_INCOMING_ACTION2 packet handles the overflow but it must be sent *before* the corresponding W3GS_INCOMING_ACTION packet

				SendAll( m_Protocol->SEND_W3GS_INCOMING_ACTION2( SubActions ) );

				while( !SubActions.empty( ) )
				{
					delete SubActions.front( );
					SubActions.pop( );
				}

				SubActionsLength = 0;
			}

			SubActions.push( Action );
			SubActionsLength += Action->GetLength( );
		}

		SendAll( m_Protocol->SEND_W3GS_INCOMING_ACTION( SubActions, Latency ) );

		while( !SubActions.empty( ) )
		{
			delete SubActions.front( );
			SubActions.pop( );
		}
	}
	else
		SendAll( m_Protocol->SEND_W3GS_INCOMING_ACTION( m_Actions, Latency ) );

	uint32_t ActualSendInterval = GetTicks( ) - m_LastActionSentTicks;
	uint32_t ExpectedSendInterval = m_DynamicLatency - m_LastActionLateBy;
	m_LastActionLateBy = ActualSendInterval - ExpectedSendInterval;

	if( m_LastActionLateBy > m_DynamicLatency )
	{
		// something is going terribly wrong - GHost++ is probably starved of resources
		// print a message because even though this will take more resources it should provide some information to the administrator for future reference
		// other solutions - dynamically modify the latency, request higher priority, terminate other games, ???

		if (m_GHost->m_newLatency)
		CONSOLE_Print( "[GAME: " + m_GameName + "] warning - the latency is " + UTIL_ToString( m_DynamicLatency ) + "ms but the last update was late by " + UTIL_ToString( m_LastActionLateBy ) + "ms" );
		m_LastActionLateBy = m_DynamicLatency;
		m_LastActionLateTicks = GetTicks();
	}

	m_LastActionSentTicks = GetTicks( );
}

void CBaseGame :: SendWelcomeMessage( CGamePlayer *player )
{
	
	for( vector<string> :: iterator i = m_GHost->m_Welcome.begin( ); i != m_GHost->m_Welcome.end( ); i++ )
	{
		SendChat( player, (*i));
	}
	
	SendChat( player, " " );
	SendChat( player, "=============================================================" );
	
//	SendChat( player, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-" );
	if (m_CountryCheck || m_CountryCheck2 || m_ProviderCheck || m_ProviderCheck2)
	{	
		string C1 = m_Countries;
		string C2 = m_Countries2;
		Replace(C1, "??", string());
		Replace(C2, "??", string());	
		if (m_CountryCheck)
			SendChat( player,"=     Allowed Countries = "+C1 );
		if (m_CountryCheck2 && !m_CountryCheck)
			SendChat( player,"=     Denied Countries  = "+C2 );
		if (m_ProviderCheck)
			SendChat( player,C3+"=     Allowed Providers = "+m_Providers);
		if (m_ProviderCheck2)
			SendChat( player,"=     Denied Providers  = "+m_Providers2);
		}
	}
//	SendChat( player, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-" );
//	SendChat( player, "Owner: "+m_OwnerName+"    Game Name:     " + m_GameName );
	if( !m_HCLCommandString.empty( ) )		
		SendChat( player,"= $" + m_GameName + "$ autoHCL Command String:  " + m_HCLCommandString);
	uint32_t N = m_GHost->m_AutoHostAutoStartPlayers;	
	SendChat( player, "=            $AutoStarT$ once >= "+ UTIL_ToString(N) +" players filled");
	SendChat( player, "=                                                           " );	
	SendChat( player, "= (\\_       No admins here? To FORCE START:                 " );
	SendChat( player, "= (_ \\ ( '>  Type !owner to gain control of lobby           " );
	SendChat( player, "=   ) \\/_)= OR all should write !go (4 votes to startgame)  " );
	SendChat( player, "=   (_(_ )_                                                 " );
	SendChat( player, "=============================================================" );
	SendChat( player, " " );

}

void CBaseGame :: SendEndMessage( )
{
	// read from gameover.txt if available

	ifstream in;
	in.open( "gameover.txt" );

	if( !in.fail( ) )
	{
		// don't print more than 8 lines

		uint32_t Count = 0;
		string Line;

		while( !in.eof( ) && Count < 8 )
		{
			getline( in, Line );

			if( Line.empty( ) )
				SendAllChat( " " );
			else
				SendAllChat( Line );

			if( in.eof( ) )
				break;

			Count++;
		}

		in.close( );
	}
}

void CBaseGame :: EventPlayerDeleted( CGamePlayer *player )
{
	CONSOLE_Print( "[GAME: " + m_GameName + "] deleting player [" + player->GetName( ) + "]: " + player->GetLeftReason( ) );

	// remove any queued spoofcheck messages for this player

	if( player->GetWhoisSent( ) && !player->GetJoinedRealm( ).empty( ) && player->GetSpoofedRealm( ).empty( ) )
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == player->GetJoinedRealm( ) )
			{
				// hackhack: there must be a better way to do this

				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->UnqueueChatCommand( "/whereis " + player->GetName( ) );
				else
					(*i)->UnqueueChatCommand( "/whois " + player->GetName( ) );

				(*i)->UnqueueChatCommand( "/w " + player->GetName( ) + " " + m_GHost->m_Language->SpoofCheckByReplying( ) );
			}
		}
	}

	m_LastPlayerLeaveTicks = GetTicks( );

	
	uint32_t GameNr = GetGameNr();

	if (m_GHost->m_CurrentGame)
		if (m_GHost->m_CurrentGame->GetCreationTime()==GetCreationTime())
			GameNr = 255;

	m_GHost->UDPChatSend("|playerleft "+UTIL_ToString(GameNr)+" "+UTIL_ToString(GetSlotsOpen())+" "+player->GetName( ));

	// in some cases we're forced to send the left message early so don't send it again

	if( player->GetLeftMessageSent( ) )
		return;
	
	player->SetLeftMessageSent(true);

	ReCalculateTeams();

	if( m_GameLoaded )
		SendAllChat( player->GetName( ) + " " + player->GetLeftReason( ) + "." );

	if( player->GetLagging( ) )
		SendAll( m_Protocol->SEND_W3GS_STOP_LAG( player ) );

	// autosave

	if( m_GameLoaded && player->GetLeftCode( ) == PLAYERLEAVE_DISCONNECT && m_AutoSave )
	{
		string SaveGameName = UTIL_FileSafeName( "GHost++ AutoSave " + m_GameName + " (" + player->GetName( ) + ").w3z" );
		CONSOLE_Print( "[GAME: " + m_GameName + "] auto saving [" + SaveGameName + "] before player drop, shortened send interval = " + UTIL_ToString( GetTicks( ) - m_LastActionSentTicks ) );
		BYTEARRAY CRC;
		BYTEARRAY Action;
		Action.push_back( 6 );
		UTIL_AppendByteArray( Action, SaveGameName );
		m_Actions.push( new CIncomingAction( player->GetPID( ), CRC, Action ) );

		// todotodo: with the new latency system there needs to be a way to send a 0-time action

		SendAllActions( );
	}

	if( m_GameLoading && m_LoadInGame )
	{
		// we must buffer player leave messages when using "load in game" to prevent desyncs
		// this ensures the player leave messages are correctly interleaved with the empty updates sent to each player

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( (*i)->GetFinishedLoading( ) )
			{
				if( !player->GetFinishedLoading( ) )
					Send( *i, m_Protocol->SEND_W3GS_STOP_LAG( player ) );

				Send( *i, m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( player->GetPID( ), player->GetLeftCode( ) ) );
			}
			else
				(*i)->AddLoadInGameData( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( player->GetPID( ), player->GetLeftCode( ) ) );
		}
	}
	else
	{
		// tell everyone about the player leaving

		SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( player->GetPID( ), player->GetLeftCode( ) ) );
	}

	// set the replay's host PID and name to the last player to leave the game
	// this will get overwritten as each player leaves the game so it will eventually be set to the last player

	if( m_Replay && ( m_GameLoading || m_GameLoaded ) )
	{
		m_Replay->SetHostPID( player->GetPID( ) );
		m_Replay->SetHostName( player->GetName( ) );

		// add leave message to replay

		if( m_GameLoading && !m_LoadInGame )
			m_Replay->AddLeaveGameDuringLoading( 1, player->GetPID( ), player->GetLeftCode( ) );
		else
			m_Replay->AddLeaveGame( 1, player->GetPID( ), player->GetLeftCode( ) );
	}

	// abort the countdown if there was one in progress

	if (!m_NormalCountdown)
	if( m_CountDownStarted && !m_GameLoading && !m_GameLoaded )
	{
		SendAllChat( m_GHost->m_Language->CountDownAborted( ) );
		m_CountDownStarted = false;
	}

	// abort the votekick

	if( !m_KickVotePlayer.empty( ) )
		SendAllChat( m_GHost->m_Language->VoteKickCancelled( m_KickVotePlayer ) );

	m_KickVotePlayer.clear( );
	m_StartedKickVoteTime = 0;
	
	// abort the votestart
	
	if( m_StartedVoteStartTime != 0 )
	  SendAllChat( "Votestart cancelled!" );

	m_StartedVoteStartTime = 0;

	// abort the rmkvote

	if( !m_RmkVotePlayer.empty( ) )
		SendAllChat( "Rmk vote canceled." );

	m_RmkVotePlayer.clear( );
	m_StartedRmkVoteTime = 0;

}

void CBaseGame :: EventPlayerDisconnectTimedOut( CGamePlayer *player )
{
	if( player->GetGProxy( ) && m_GameLoaded )
	{
		if( !player->GetGProxyDisconnectNoticeSent( ) )
		{
			SendAllChat( player->GetName( ) + " " + m_GHost->m_Language->HasLostConnectionTimedOutGProxy( ) + "." );
			player->SetGProxyDisconnectNoticeSent( true );
		}

		if( GetTime( ) - player->GetLastGProxyWaitNoticeSentTime( ) >= 20 )
		{
			uint32_t TimeRemaining = ( m_GProxyEmptyActions + 1 ) * 60 - ( GetTime( ) - m_StartedLaggingTime );

			if( TimeRemaining > ( (uint32_t)m_GProxyEmptyActions + 1 ) * 60 )
				TimeRemaining = ( m_GProxyEmptyActions + 1 ) * 60;

			SendAllChat( player->GetPID( ), m_GHost->m_Language->WaitForReconnectSecondsRemain( UTIL_ToString( TimeRemaining ) ) );
			player->SetLastGProxyWaitNoticeSentTime( GetTime( ) );
		}

		return;
	}

	// not only do we not do any timeouts if the game is lagging, we allow for an additional grace period of 10 seconds
	// this is because Warcraft 3 stops sending packets during the lag screen
	// so when the lag screen finishes we would immediately disconnect everyone if we didn't give them some extra time

	if( GetTime( ) - m_LastLagScreenTime >= 10 )
	{
		player->SetDeleteMe( true );
		player->SetLeftReason( m_GHost->m_Language->HasLostConnectionTimedOut( ) );
		player->SetLeftCode( PLAYERLEAVE_DISCONNECT );

		if( !m_GameLoading && !m_GameLoaded )
			OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );
	}
}

void CBaseGame :: EventPlayerDisconnectPlayerError( CGamePlayer *player )
{
	// at the time of this comment there's only one player error and that's when we receive a bad packet from the player
	// since TCP has checks and balances for data corruption the chances of this are pretty slim

	player->SetDeleteMe( true );
	player->SetLeftReason( m_GHost->m_Language->HasLostConnectionPlayerError( player->GetErrorString( ) ) );
	player->SetLeftCode( PLAYERLEAVE_DISCONNECT );

	if( !m_GameLoading && !m_GameLoaded )
		OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );
}

void CBaseGame :: EventPlayerDisconnectSocketError( CGamePlayer *player )
{
	if( player->GetGProxy( ) && m_GameLoaded )
	{
		if( !player->GetGProxyDisconnectNoticeSent( ) )
		{
			SendAllChat( player->GetName( ) + " " + m_GHost->m_Language->HasLostConnectionSocketErrorGProxy( player->GetSocket( )->GetErrorString( ) ) + "." );
			player->SetGProxyDisconnectNoticeSent( true );
		}

		if( GetTime( ) - player->GetLastGProxyWaitNoticeSentTime( ) >= 20 )
		{
			uint32_t TimeRemaining = ( m_GProxyEmptyActions + 1 ) * 60 - ( GetTime( ) - m_StartedLaggingTime );

			if( TimeRemaining > ( (uint32_t)m_GProxyEmptyActions + 1 ) * 60 )
				TimeRemaining = ( m_GProxyEmptyActions + 1 ) * 60;

			SendAllChat( player->GetPID( ), m_GHost->m_Language->WaitForReconnectSecondsRemain( UTIL_ToString( TimeRemaining ) ) );
			player->SetLastGProxyWaitNoticeSentTime( GetTime( ) );
		}

		return;


	}

	player->SetDeleteMe( true );
	player->SetLeftReason( m_GHost->m_Language->HasLostConnectionSocketError( player->GetSocket( )->GetErrorString( ) ) );
	player->SetLeftCode( PLAYERLEAVE_DISCONNECT );

	if( !m_GameLoading && !m_GameLoaded )
		OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );
}

void CBaseGame :: EventPlayerDisconnectConnectionClosed( CGamePlayer *player )
{
	if( player->GetGProxy( ) && m_GameLoaded )
	{
		if( !player->GetGProxyDisconnectNoticeSent( ) )
		{
			SendAllChat( player->GetName( ) + " " + m_GHost->m_Language->HasLostConnectionClosedByRemoteHostGProxy( ) + "." );
			player->SetGProxyDisconnectNoticeSent( true );
		}

		if( GetTime( ) - player->GetLastGProxyWaitNoticeSentTime( ) >= 20 )
		{
			uint32_t TimeRemaining = ( m_GProxyEmptyActions + 1 ) * 60 - ( GetTime( ) - m_StartedLaggingTime );

			if( TimeRemaining > ( (uint32_t)m_GProxyEmptyActions + 1 ) * 60 )
				TimeRemaining = ( m_GProxyEmptyActions + 1 ) * 60;

			SendAllChat( player->GetPID( ), m_GHost->m_Language->WaitForReconnectSecondsRemain( UTIL_ToString( TimeRemaining ) ) );
			player->SetLastGProxyWaitNoticeSentTime( GetTime( ) );
		}

		return;
	}

	player->SetDeleteMe( true );
	player->SetLeftReason( m_GHost->m_Language->HasLostConnectionClosedByRemoteHost( ) );
	player->SetLeftCode( PLAYERLEAVE_DISCONNECT );

	if( !m_GameLoading && !m_GameLoaded )
		OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );
}

void CBaseGame :: EventPlayerJoined( CPotentialPlayer *potential, CIncomingJoinPlayer *joinPlayer )
{
	m_LastPlayerJoiningTime = GetTime();

	if( m_CountDownStarted || m_GameLoading || m_GameLoaded ) 
	{
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;		
	}

	// check if the new player's name is empty or too long

	if( joinPlayer->GetName( ).empty( ) || joinPlayer->GetName( ).size( ) > 15 )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game with an invalid name of length " + UTIL_ToString( joinPlayer->GetName( ).size( ) ) );
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	// check if the new player's name is the same as the virtual host name

	if( joinPlayer->GetName( ) == m_VirtualHostName )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game with the virtual host name" );
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	// check if the new player's name is already taken

	if( GetPlayerFromName( joinPlayer->GetName( ), false ) )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but that name is already taken" );
		// SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButTaken( joinPlayer->GetName( ) ) );
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	// identify their joined realm
	// this is only possible because when we send a game refresh via LAN or battle.net we encode an ID value in the 4 most significant bits of the host counter
	// the client sends the host counter when it joins so we can extract the ID value here
	// note: this is not a replacement for spoof checking since it doesn't verify the player's name and it can be spoofed anyway

	uint32_t HostCounterID = joinPlayer->GetHostCounter( ) >> 28;
	string JoinedRealm;

	// we use an ID value of 0 to denote joining via LAN

	if( HostCounterID == 0 )
	{
		// the player is pretending to join via LAN, which they might or might not be (i.e. it could be spoofed)
		// however, we've been broadcasting a random entry key to the LAN
		// if the player is really on the LAN they'll know the entry key, otherwise they won't
		// or they're very lucky since it's a 32 bit number

		if( joinPlayer->GetEntryKey( ) != m_EntryKey )
		{
			// oops!

			CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game over LAN but used an incorrect entry key" );
			potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_WRONGPASSWORD ) );
			potential->SetDeleteMe( true );
			return;
		}
	}
	else
	{
                for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
		{
			if( (*i)->GetHostCounterID( ) == HostCounterID )
				JoinedRealm = (*i)->GetServer( );
		}
	}

	// test if player is reserved/admin/safelisted

	bool RootAdminCheck = false;
	for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
	{
		if( (*j)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			RootAdminCheck = true;
			break;
		}
	}

	bool AdminCheck = false;

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if( (*i)->IsAdmin( joinPlayer->GetName( ) ) || (*i)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			AdminCheck = true;
			break;
		}
	}

	// test if player is safelisted

	bool SafeCheck = false;
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if( (*i)->IsSafe( joinPlayer->GetName( ) ) )
		{
			SafeCheck = true;
			break;
		}
	}

	bool Reserved = IsReserved( joinPlayer->GetName( ) ) || AdminCheck || IsOwner( joinPlayer->GetName( )) || SafeCheck;

	// check if the new player's name is banned
	if (m_GHost->m_Banning != 0)
	if (!Reserved)
	if (!m_ScoreCheckChecked)
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		string BlacklistedName = joinPlayer->GetName( );
		transform( BlacklistedName.begin( ), BlacklistedName.end( ), BlacklistedName.begin( ), (int(*)(int))tolower );		
		CDBBan *Bann = (*i)->IsBannedName( BlacklistedName );
		CDBBan *Ban = (*i)->IsBannedName( joinPlayer->GetName( ) );		
		if( Ban || Bann )
		{
			if( (*i)->GetServer( ) == JoinedRealm ){
				potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
				potential->SetDeleteMe( true );
				CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) +"("+potential->GetExternalIPString()+")] is trying to join the game but is name banned. Nice try!" );
//				SendChat(Player->GetPID(), "Join Request rejected due to a ban for prolly DL & early leaving in lobby (2days ban) or other reason(often >2days of ban)" );
			return;
			}
			string sIP = Ban->GetIP();
			if (false)
			if (sIP==string() && m_GHost->DBType == "mysql")
				m_BanUpdates.push_back( m_GHost->m_DB->ThreadedBanUpdate( joinPlayer->GetName(), sIP ) );

			string sReason = Ban->GetReason();
			string sAdmin = Ban->GetAdmin();
			CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but is banned by name" );
/*			string n=joinPlayer->GetName();
			string s(16-n.length(),' ');
			n=s+n;*/
			if (m_GHost->m_Verbose)
			if ( m_AnnouncedPlayer != joinPlayer->GetName() )
			{
				m_AnnouncedPlayer = joinPlayer->GetName();

//				string sBan = joinPlayer->GetName()+"("+potential->GetExternalIPString()+") is banned by "+sAdmin;
				string sBan = joinPlayer->GetName()+" is banned by "+sAdmin;
				string sBReason = sBan + ", "+sReason;

				if (m_GHost->m_Banning == 2)
				if (sReason=="")
					SendAllChat( sBan );
				else
				{
					if (sBReason.length()<250 && !m_GHost->m_TwoLinesBanAnnouncement)
						SendAllChat( sBReason );
					else
					{
						SendAllChat( sBan );
						SendAllChat( "Ban reason: " + sReason );
					}
				}

//				string s= m_GHost->m_Language->TryingToJoinTheGameButBanned( joinPlayer->GetName() + "|" + potential->GetExternalIPString( ), Ban->GetAdmin() );
 				string s= m_GHost->m_Language->TryingToJoinTheGameButBanned( joinPlayer->GetName(), Ban->GetAdmin() );
				string sr = s+", "+sReason;

				if (m_GHost->m_Banning != 2)
				if (sReason=="")
				{
					SendAllChat(s);
				} else
				{
					if (sr.length()<250)
						SendAllChat(sr);
					else
					{
						SendAllChat(s);
						SendAllChat("Ban reason: "+sReason);
					}
				}
			}
			// let banned players "join" the game with an arbitrary PID then immediately close the connection
			// this causes them to be kicked back to the chat channel on battle.net

			if (m_Bans)
			{
				vector<CGameSlot> Slots = m_Map->GetSlots( );
				potential->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( 1, potential->GetSocket( )->GetPort( ), potential->GetExternalIP( ), Slots, 0, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );
				potential->SetDeleteMe( true );
				return;
			}
	
/*
			potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
			potential->SetDeleteMe( true );
			return;
*/	
		}
	}

	// check if the new player's ip is banned
	if (!Reserved)
	if (!m_ScoreCheckChecked)
	if (!potential->IsLAN())
	if (potential->GetExternalIPString()!="127.0.0.1")
		if (m_GHost->m_IPBanning!=0)
			for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
			{
				CDBBan *IPBan = (*i)->IsBannedIP(potential->GetExternalIPString( ) );

				if( IPBan )
				{
					string sReason = IPBan->GetReason();
					string sName = IPBan->GetName();
					string sAdmin = IPBan->GetAdmin();
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) +"("+potential->GetExternalIPString()+")"+ "] is trying to join the game but is IP banned" );					
					/*
					string n=Player->GetName();
					string s(16-n.length(),' ');
					n=s+n;*/
					if (m_GHost->m_IPBanning==1)
						if (m_GHost->m_Verbose)
						{
							SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButBanned( joinPlayer->GetName()+"(IP of  "+sName+" )", IPBan->GetAdmin()) );
//							SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButBanned( joinPlayer->GetName()+"("+potential->GetExternalIPString()+")", IPBan->GetAdmin()) );
							if (sReason!="")
//								SendAllChat ( sName +"("+potential->GetExternalIPString()+") ban reason: "+sReason);
								SendAllChat ( "Ban reason: "+sReason);
//							else
//								SendAllChat ( sName +"("+potential->GetExternalIPString()+")");
								
						}
					if (m_GHost->m_IPBanning==1 && m_Bans)
					{
						// let banned players "join" the game with an arbitrary PID then immediately close the connection
						// this causes them to be kicked back to the chat channel on battle.net
		
						vector<CGameSlot> Slots = m_Map->GetSlots( );
						potential->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( 1, potential->GetSocket( )->GetPort( ), potential->GetExternalIP( ), Slots, 0, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );
						potential->SetDeleteMe( true );
						return;

/*
						potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
						potential->SetDeleteMe( true );
						return;
*/
					}
					if (m_GHost->m_IPBanning==2) 
					{
						string sBan = joinPlayer->GetName()+"(IP banned as "+sName+") by "+sAdmin;
//						string sBan = joinPlayer->GetName()+"("+potential->GetExternalIPString()+") is IP banned by "+sAdmin;
						string sBReason = sBan + ", "+sReason;

						if (sReason=="")
							SendAllChat( sBan );
						else
						{
							if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
								SendAllChat( sBReason );
							else
							{
								SendAllChat( sBan );
								SendAllChat( "Ban reason: " + sReason );
							}
						}
					}
				}
			}

			// check if we only allow garena

			if (!m_ScoreCheckChecked)
			if (m_GarenaOnly)
			{
				string EIP = potential->GetExternalIPString();
				// kick if not garena, admin, rootadmin, reserver
				if (EIP!="127.0.0.1" && EIP!=m_GHost->m_ExternalIP && !IsOwner( joinPlayer->GetName( ) ) && !AdminCheck && !RootAdminCheck && !IsReserved (joinPlayer->GetName()))
				{
					// let banned players "join" the game with an arbitrary PID then immediately close the connection
					// this causes them to be kicked back to the chat channel on battle.net
	
					vector<CGameSlot> Slots = m_Map->GetSlots( );
					potential->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( 1, potential->GetSocket( )->GetPort( ), potential->GetExternalIP( ), Slots, 0, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );
					potential->SetDeleteMe( true );
					return;
/*
					potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
					potential->SetDeleteMe( true );
					return;
*/
				}
			}
			potential->GetExternalIPString();
			string From;

			// check if the country or provider is not allowed
//			if (potential->GetExternalIPString()!="127.0.0.1")
				if (!Reserved)
				if (!m_ScoreCheckChecked)
					if (m_CountryCheck || m_CountryCheck2 || m_ProviderCheck || m_ProviderCheck2 || ((m_GHost->m_AutoHostCountryCheck2 || m_GHost->m_AutoHostCountryCheck) && m_autohosted))
					{
						string Fromu;
						string P;
						string s;
						bool bad = false;
						bool allowed=false;
						if (m_ProviderCheck2)
							allowed= true;
						if (m_ProviderCheck)
							allowed= false;
						{
							if (potential->GetExternalIPString()=="127.0.0.1")
								From = "Ga";
							else
								From = m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( potential->GetExternalIP( ), true ) );
							Fromu = From;
							transform( Fromu.begin( ), Fromu.end( ), Fromu.begin( ), (int(*)(int))toupper );						

							if (m_CountryCheck)
								if (m_Countries.find(Fromu)==string :: npos)
									bad=true;

							if (!bad)
								if (m_GHost->m_AutoHostCountryCheck2 && m_autohosted)
									if (m_GHost->m_AutoHostCountries2.find(Fromu)!=string :: npos)
										bad=true;

							if (!bad)
							if (m_GHost->m_AutoHostCountryCheck && m_autohosted)
								if (m_GHost->m_AutoHostCountries.find(Fromu)==string :: npos)
									bad=true;

							if (!bad)
								if (m_CountryCheck2)
									if (m_Countries2.find(Fromu)!=string :: npos)
										bad=true;

							if (!bad)
								if (m_ProviderCheck || m_ProviderCheck2)
								{
									if (From!="??" && From!="Ga")
										P=m_GHost->UDPChatWhoIs(From, potential->GetExternalIPString( ));

									transform( P.begin( ), P.end( ), P.begin( ), (int(*)(int))toupper );						
									From = From + " "+ P;
								}
								if (!bad)
									if (m_ProviderCheck)
									{
										stringstream SS;
										SS << m_Providers;

										while( !SS.eof( ) )
										{
											SS >> s;
											if (P.find(s)!=string :: npos)
												allowed=true;
										}
										if (!allowed)
											bad=true;
									}

									if (!bad)
										if (m_ProviderCheck2)
										{
											stringstream SS;
											SS << m_Providers2;

											while( !SS.eof( ) )
											{
												SS >> s;
												if (P.find(s)!=string :: npos)
													allowed=false;
											}
											if (!allowed)
												bad=true;
										}
										if (bad)
										{
											string n=joinPlayer->GetName();
//											string s(16-n.length(),' ');
											//n=s+n;
											if (m_GHost->m_Verbose && m_GHost->m_ShowCountryNotAllowed)
												SendAllChat( m_GHost->m_Language->AutokickingPlayerForDeniedCountry( n, From ) );
											//			potential->SetSocket( NULL );
											//			potential->SetDeleteMe( true );

											vector<CGameSlot> Slots = m_Map->GetSlots( );
											potential->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( 1, potential->GetSocket( )->GetPort( ), potential->GetExternalIP( ), Slots, 0, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );
											potential->SetDeleteMe( true );
											return;

/*											potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
											potential->SetDeleteMe( true );
											return;
*/
										}
						}
					}

// check if we're only allowing certain scores
	if (!m_ScoreCheckChecked)
	if(m_ScoreCheck && !Reserved && !m_Map->GetMapMatchMakingCategory( ).empty( ) && m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
	{
		// scorechecking is enabled
		// start a database query to determine the player's score
		// when the query is complete we will call EventPlayerJoinedWithScore

		m_ScoreChecks.push_back( m_GHost->m_DB->ThreadedScoreCheck( m_Map->GetMapMatchMakingCategory( ), joinPlayer->GetName( ), JoinedRealm ) );
		return;
	}

	string sc = "0";
	string ra = "0";
	string reqScore = UTIL_ToString(m_Scores, 2);
	double Score;
	bool ScoreChecked = false;

	if (m_ScoreCheckChecked)
	{
		uint32_t scorecount = m_GHost->ScoresCount();

		sc = UTIL_ToString(m_ScoreCheckScore, 2);
		ra = UTIL_ToString(m_ScoreCheckRank);

		ScoreChecked = true;
		Score = m_ScoreCheckScore;
		bool allow = false;
		allow = (m_GHost->m_AllowNullScoredPlayers && (Score==0));
		if (!allow)
		if (Score<m_Scores)
		{
			string n=joinPlayer->GetName();
			if (m_GHost->m_Verbose)
				SendAllChat( m_GHost->m_Language->AutokickingPlayerForDeniedScore( n, sc, reqScore ) );

			vector<CGameSlot> Slots = m_Map->GetSlots( );
			potential->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( 1, potential->GetSocket( )->GetPort( ), potential->GetExternalIP( ), Slots, 0, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );
			potential->SetDeleteMe( true );
			return;
		}
	}

	if( m_MatchMaking && m_AutoStartPlayers != 0 && !m_Map->GetMapMatchMakingCategory( ).empty( ) && m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
	{
		// matchmaking is enabled
		// start a database query to determine the player's score
		// when the query is complete we will call EventPlayerJoinedWithScore

		m_ScoreChecks.push_back( m_GHost->m_DB->ThreadedScoreCheck( m_Map->GetMapMatchMakingCategory( ), joinPlayer->GetName( ), JoinedRealm ) );
		return;
	}

	// check if the player is an admin or root admin on any connected realm for determining reserved status
	// we can't just use the spoof checked realm like in EventPlayerBotCommand because the player hasn't spoof checked yet

	bool AnyAdminCheck = false;

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if( (*i)->IsAdmin( joinPlayer->GetName( ) ) || (*i)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			AnyAdminCheck = true;
			break;
		}
	}
	// try to find a slot

	unsigned char SID = 255;
	unsigned char EnforcePID = 255;
	unsigned char EnforceSID = 0;
	CGameSlot EnforceSlot( 255, 0, 0, 0, 0, 0, 0 );

	if( m_SaveGame )
	{
		// in a saved game we enforce the player layout and the slot layout
		// unfortunately we don't know how to extract the player layout from the saved game so we use the data from a replay instead
		// the !enforcesg command defines the player layout by parsing a replay

		for( vector<PIDPlayer> :: iterator i = m_EnforcePlayers.begin( ); i != m_EnforcePlayers.end( ); i++ )
		{
			if( (*i).second == joinPlayer->GetName( ) )
				EnforcePID = (*i).first;
		}

		for( vector<CGameSlot> :: iterator i = m_EnforceSlots.begin( ); i != m_EnforceSlots.end( ); i++ )
		{
			if( (*i).GetPID( ) == EnforcePID )
			{
				EnforceSlot = *i;
				break;
			}

			EnforceSID++;
		}

		if( EnforcePID == 255 || EnforceSlot.GetPID( ) == 255 || EnforceSID >= m_Slots.size( ) )
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but isn't in the enforced list" );
			potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
			potential->SetDeleteMe( true );
			return;
		}

		SID = EnforceSID;
	}
	else
	{
		// try to find an empty slot

		SID = GetEmptySlot( false );
	
		// check if the player is an admin or root admin on any connected realm for determining reserved status
		// we can't just use the spoof checked realm like in EventPlayerBotCommand because the player hasn't spoof checked yet
	
	
		if (AdminCheck)
			m_GHost->UDPChatSend("|vip "+joinPlayer->GetName());
	
		if( SID == 255 && Reserved )
		{
			// a reserved player is trying to join the game but it's full, try to find a reserved slot

			if (AdminCheck)
				SID = GetEmptySlotAdmin( true );
			else
				SID = GetEmptySlot( true );

			if( SID != 255 )
			{
				CGamePlayer *KickedPlayer = GetPlayerFromSID( SID );

				if( KickedPlayer )
				{
					KickedPlayer->SetDeleteMe( true );
					KickedPlayer->SetLeftReason( m_GHost->m_Language->WasKickedForReservedPlayer( joinPlayer->GetName( ) ) );
					KickedPlayer->SetLeftCode( PLAYERLEAVE_LOBBY );

					// send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
					// we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

					SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( KickedPlayer->GetPID( ), KickedPlayer->GetLeftCode( ) ) );
					KickedPlayer->SetLeftMessageSent( true );
				}
			}
		}

		if( SID == 255 && IsOwner( joinPlayer->GetName( ) ) )
		{
			// the owner player is trying to join the game but it's full and we couldn't even find a reserved slot, kick the player in the lowest numbered slot
			// updated this to try to find a player slot so that we don't end up kicking a computer

			SID = 0;

			for( unsigned char i = 0; i < m_Slots.size( ); i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[i].GetComputer( ) == 0 )
				{
					SID = i;
					break;
				}
			}

			CGamePlayer *KickedPlayer = GetPlayerFromSID( SID );

			if( KickedPlayer )
			{
				KickedPlayer->SetDeleteMe( true );
				KickedPlayer->SetLeftReason( m_GHost->m_Language->WasKickedForOwnerPlayer( joinPlayer->GetName( ) ) );
				KickedPlayer->SetLeftCode( PLAYERLEAVE_LOBBY );

				// send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
				// we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

				SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( KickedPlayer->GetPID( ), KickedPlayer->GetLeftCode( ) ) );
				KickedPlayer->SetLeftMessageSent( true );
			}
		}
	}

	if( SID >= m_Slots.size( ) )
	{
		if (AdminCheck && GetTicks() - m_LastAdminJoinAndFullTicks > 2000)
		{
			m_LastAdminJoinAndFullTicks = GetTicks();
			SendAllChat("Admin "+joinPlayer->GetName()+" is trying to join but the lobby is full...");
		}
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	// check if the owner joined and place him on the first slot.
	bool Owner = IsOwner( joinPlayer->GetName( ) );

	if( Owner )
	{
		m_OwnerJoined = true;
		unsigned char SSID = 255;

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetComputer( ) == 0 )
			{
				SSID = i;
				break;
			}
		}
//		m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && 
		if (SSID != SID && SSID !=255)
		{
			unsigned char l;
			unsigned char k;
			l = SSID;
			for (l = SID-1; l>= SSID; l--)
			{
				if (m_Slots[l].GetComputer()==0)
				{
					for (k = l + 1; k<=SID; k++)
					{
						if (m_Slots[k].GetComputer()==0)
						{
							CGameSlot Slot1 = m_Slots[k];
							CGameSlot Slot2 = m_Slots[l];
							if( m_Map->GetMapGameType( ) != GAMETYPE_CUSTOM )
							{
								// regular game - swap everything
								m_Slots[k] = Slot2;
								m_Slots[l] = Slot1;
							}
							else
							{
								// custom game - don't swap the team, colour, or race

								m_Slots[k] = CGameSlot( Slot2.GetPID( ), Slot2.GetDownloadStatus( ), Slot2.GetSlotStatus( ), Slot2.GetComputer( ), Slot1.GetTeam( ), Slot1.GetColour( ), Slot1.GetRace( ) );
								m_Slots[l] = CGameSlot( Slot1.GetPID( ), Slot1.GetDownloadStatus( ), Slot1.GetSlotStatus( ), Slot1.GetComputer( ), Slot2.GetTeam( ), Slot2.GetColour( ), Slot2.GetRace( ) );
							}
							m_GHost->UDPChatSend("|swap "+UTIL_ToString(k)+" "+UTIL_ToString(l));
//							SendAllSlotInfo( );

//							SwapSlots(l, k);
							break;
						}
					}
				}
				if (l==0) 
					break;
			}

//			SwapSlots(SID, SSID);
			SID = SSID;
//			SendAllSlotInfo( );
		}
	}

	// check if an admin joined and place him as high as possible
	bool isAdmin = false;
	bool isAdmin2;
	string pName;


	for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
	{
		if( (*j)->IsAdmin(joinPlayer->GetName( ) ) || (*j)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			isAdmin = true;
			break;
		}
	}
	if (IsReserved (joinPlayer->GetName()))
		isAdmin=true;

	bool dontplacehigher = m_GHost->m_PlaceAdminsHigherOnlyInDota && (m_GetMapType != "dota");

	if ( isAdmin && (!dontplacehigher))
	{
		unsigned char SSID = 255;

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
	// find the first slot occupied by a player who's neither an admin nor reserved
			if( m_Slots[i].GetComputer( ) == 0 && m_Slots[i].GetSlotStatus()==SLOTSTATUS_OCCUPIED)
			{
				isAdmin2=false;
				if (GetPlayerFromSID(i)!=NULL)
				{
					pName = GetPlayerFromSID(i)->GetName();
					
					for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
					{
						if( (*j)->IsAdmin( pName ) || (*j)->IsRootAdmin( pName ) || IsOwner(pName) )
						{
							isAdmin2 = true;
							break;
						}
					}
					if (IsReserved (pName))
						isAdmin2=true;
				} else isAdmin2=true;
				if (!isAdmin2)
				{
					SSID = i;
					break;
				}
			}
		}
		//		m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && 
		if (SSID != SID && SSID !=255 && SSID<SID)
		{
			SwapSlots(SID, SSID);
			SID = SSID;
		}
	}

	// check to see if we have a specific slot reserved for this player
	unsigned char sn = ReservedSlot(joinPlayer->GetName());

	if (sn!=255 && sn!=SID && sn<m_Slots.size( ))
	if (m_Slots[sn].GetComputer( ) == 0)
	{
			SwapSlots(SID, sn);
			SID = sn;
			ResetReservedSlot(joinPlayer->GetName());
	} else
		ResetReservedSlot(joinPlayer->GetName());

	// we have a slot for the new player
	// make room for them by deleting the virtual host player if we have to

	uint32_t SlotReq = 11;
	if (m_ShowRealSlotCount)
		SlotReq = m_Slots.size()-1;
	if( GetSlotsOccupied( ) >= SlotReq || EnforcePID == m_VirtualHostPID )
		DeleteVirtualHost( );

	// turning the CPotentialPlayer into a CGamePlayer is a bit of a pain because we have to be careful not to close the socket
	// this problem is solved by setting the socket to NULL before deletion and handling the NULL case in the destructor
	// we also have to be careful to not modify the m_Potentials vector since we're currently looping through it

//	CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] joined the game" );
	CGamePlayer *Player = new CGamePlayer( potential, m_SaveGame ? EnforcePID : GetNewPID( ), JoinedRealm, joinPlayer->GetName( ), joinPlayer->GetInternalIP( ), Reserved );
	if( potential->GetGarenaUser( ) != NULL ) {
		Player->SetGarenaUser( potential->GetGarenaUser( ) );
		potential->SetGarenaUser( NULL );
	}

	Player->SetSID( SID );
	if (ScoreChecked)
	{
		Player->SetScoreS(sc);
		Player->SetRankS(ra);
	} else
	{
		m_ScoreChecks.push_back( m_GHost->m_DB->ThreadedScoreCheck( m_Map->GetMapMatchMakingCategory( ), joinPlayer->GetName( ), JoinedRealm ) );
	}

	// if bot_rootadminsspoofcheck is 0, consider rootadmins to have already spoof checked
	
	// force admins/rootdamins to be considered spoof checked

	if ( !m_GHost->m_RootAdminsSpoofCheck && RootAdminCheck)
	{
		Player->SetSpoofed( true );
		Player->SetSpoofedRealm(m_CreatorServer);
	}

	if ( !m_GHost->m_AdminsSpoofCheck && AdminCheck)
	{
		Player->SetSpoofed( true );
		Player->SetSpoofedRealm(m_CreatorServer);
	}

	// consider the owner player to have already spoof checked
	// we will still attempt to spoof check them if it's enabled but this allows owners connecting over LAN to access admin commands

	if( IsOwner( joinPlayer->GetName( ) ) )
		Player->SetSpoofed( true );

	// consider LAN players to have already spoof checked since they can't
	// since so many people have trouble with this feature we now use the JoinedRealm to determine LAN status

//	if( UTIL_IsLanIP( Player->GetExternalIP( ) ) || UTIL_IsLocalIP( Player->GetExternalIP( ), m_GHost->m_LocalAddresses ) )
	if( JoinedRealm.empty( ) )
		Player->SetSpoofed( true );

	Player->SetWhoisShouldBeSent( m_GHost->m_SpoofChecks == 1 || ( m_GHost->m_SpoofChecks == 2 && AnyAdminCheck ) );
	m_Players.push_back( Player );
	potential->SetSocket( NULL );
	potential->SetDeleteMe( true );

	if( m_SaveGame )
		m_Slots[SID] = EnforceSlot;
	else
	{
		if( m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
			m_Slots[SID] = CGameSlot( Player->GetPID( ), 255, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam( ), m_Slots[SID].GetColour( ), m_Slots[SID].GetRace( ) );
		else
		{
			m_Slots[SID] = CGameSlot( Player->GetPID( ), 255, SLOTSTATUS_OCCUPIED, 0, 12, 12, SLOTRACE_RANDOM );

			// try to pick a team and colour
			// make sure there aren't too many other players already

			unsigned char NumOtherPlayers = 0;

			for( unsigned char i = 0; i < m_Slots.size( ); i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[i].GetTeam( ) != 12 )
					NumOtherPlayers++;
			}

			if( NumOtherPlayers < m_Map->GetMapNumPlayers( ) )
			{
				if( SID < m_Map->GetMapNumPlayers( ) )
					m_Slots[SID].SetTeam( SID );
				else
					m_Slots[SID].SetTeam( 0 );

				m_Slots[SID].SetColour( GetNewColour( ) );
			}
		}
	}

	// send slot info to the new player
	// the SLOTINFOJOIN packet also tells the client their assigned PID and that the join was successful

	Player->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( Player->GetPID( ), Player->GetSocket( )->GetPort( ), Player->GetExternalIP( ), m_Slots, m_RandomSeed, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );

	// send virtual host info and fake player info (if present) to the new player

	SendVirtualHostPlayerInfo( Player );
	SendFakePlayerInfo( Player );
	SendWTVPlayerInfo( Player );

	BYTEARRAY BlankIP;
	BlankIP.push_back( 0 );
	BlankIP.push_back( 0 );
	BlankIP.push_back( 0 );
	BlankIP.push_back( 0 );

//	CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + Player->GetName( ) + "] joined the game" );
	m_GHost->UDPChatSend("|newplayer "+UTIL_ToString(GetSlotsOpen()) + " " + Player->GetName());

// recalculate nr of players in each team + difference in players nr.
	
	ReCalculateTeams();

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) && *i != Player )
		{
			// send info about the new player to every other player
			string s;
			if( (*i)->GetSocket( ) )
			{								
				s = Player->GetJoinedRealm( );
				if ( JoinedRealm.find("AN") !=string::npos || JoinedRealm.find("an") !=string::npos || s.find("AN") != string::npos || s.find("an") != string::npos || s.find("Garena") != string::npos || s.find("garena") != string::npos || s=="LAN" || s.size( )<3 )
					s = "Ga";
				else if ( s.find("eurobattle") != string::npos )
					s = "EB";
				else s = Player->GetJoinedRealm( ).substr( 0, 2 );
				if( m_GHost->m_HideIPAddresses )
					(*i)->Send( m_Protocol->SEND_W3GS_PLAYERINFO( Player->GetPID( ), ( "[" + s + "]" + Player->GetName( ) ).substr( 0, 14 ), BlankIP, BlankIP ) );
				else
					(*i)->Send( m_Protocol->SEND_W3GS_PLAYERINFO( Player->GetPID( ), ( "[" + s + "]" + Player->GetName( ) ).substr( 0, 14 ), Player->GetExternalIP( ), Player->GetInternalIP( ) ) );
			}

			// send info about every other player to the new player
			s = (*i)->GetJoinedRealm( );
			if ( JoinedRealm.find("AN") !=string::npos || JoinedRealm.find("an") !=string::npos || s.find("AN") != string::npos || s.find("an") != string::npos || s.find("Garena") != string::npos || s.find("garena") != string::npos || s=="LAN" || s.size( )<3 )
					s = "Ga";
			else if ( s.find("eurobattle") != string::npos )
					s = "EB";			
			else s = (*i)->GetJoinedRealm( ).substr( 0, 2 );
			if( m_GHost->m_HideIPAddresses )
				Player->Send( m_Protocol->SEND_W3GS_PLAYERINFO( (*i)->GetPID( ), ( "[" + s + "]" + (*i)->GetName( ) ).substr( 0, 14 ), BlankIP, BlankIP ) );
			else
				Player->Send( m_Protocol->SEND_W3GS_PLAYERINFO( (*i)->GetPID( ), ( "[" + s + "]" + (*i)->GetName( ) ).substr( 0, 14 ), (*i)->GetExternalIP( ), (*i)->GetInternalIP( ) ) );
		}
	}

	// check for multiple ip usage.
	vector<string>IPs;
	bool sayit = false;
	vector<string>Players;
	string IP1, IP2;
	bool pp1, pp2;
	bool IPfound;
	string :: size_type pfound;
	vector<string> :: iterator p;
	if (Player->GetExternalIPString()!="127.0.0.1")
	{
		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			IP1 = (*i)->GetExternalIPString();
		// ignore host' LAN
			if (IP1!=m_GHost->m_ExternalIP)
			for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
			{
				// compare with all the other players except himself.
				if ((*i)!=(*j))
				{
					IP2 = (*j)->GetExternalIPString();
					// matching IP
					if (IP1==IP2)
					{

						if (Player->GetName( )==(*i)->GetName() || Player->GetName( )==(*j)->GetName())
							sayit = true;
						pp1 = false;
						pp2 = false;
						// already in the list of matching IPs?
						IPfound = false;
						p = Players.begin();
						if (IPs.size()>0)
						for (vector<string> :: iterator s = IPs.begin(); s != IPs.end(); s++)
						{
							if ((*s)==IP1)
							{
								IPfound = true;
								// already in the list, check too see if the players are in it
								pfound = (*p).find( (*j)->GetName() );
								if( pfound != string :: npos )
									pp2 = true;
								pfound = (*p).find( (*i)->GetName() );
								if( pfound != string :: npos )
									pp1 = true;

								if (!pp1)
									(*p)+="="+(*i)->GetName();
								if (!pp2)
									(*p)+="="+(*j)->GetName();
							}
							p++;
						}
						// the IP is not yet in the list, add it together with the two player names
						if (!IPfound)
						{
							IPs.push_back(IP1);
							Players.push_back((*i)->GetName()+"="+(*j)->GetName());
						}
					}

				}
			}
		}
	}
	// multiple IP usage was found, list it if last player who joined is part of them
	if (Player->GetExternalIPString()!="127.0.0.1")
	if (IPs.size()>0 && sayit)
	{
		string IPString;
		p = Players.begin();
		IPString = "Same IPs: ";
		for (vector<string> :: iterator s = IPs.begin(); s != IPs.end(); s++)
		{
			IPString += (*p);
			if (s!=IPs.end()-1)
				IPString +=", ";
			p++;
		}
		SendAllChat(IPString);
		m_GHost->UDPChatSend("|sameip");
	}

	// show country and pings when every slot has been occupied.
	if (GetSlotsOpen()==0 && !m_AllSlotsOccupied)
	{
		m_AllSlotsOccupied = true;
		m_AllSlotsAnnounced = false;
		m_SlotsOccupiedTime = GetTime( );
	}

	// update LastPlayerJoined

	m_LastPlayerJoined = Player->GetPID();
	m_LastPlayerJoinedTime = GetTime();

	// pings are no longer updated / will be sent to GUI in 3 seconds
	m_PingsUpdated = false;

	// send a map check packet to the new player

	Player->Send( m_Protocol->SEND_W3GS_MAPCHECK( m_Map->GetMapPath( ), m_Map->GetMapSize( ), m_Map->GetMapInfo( ), m_Map->GetMapCRC( ), m_Map->GetMapSHA1( ) ) );

	// send slot info to everyone, so the new player gets this info twice but everyone else still needs to know the new slot layout

	SendAllSlotInfo( );

	// send a welcome message

	SendWelcomeMessage( Player );

	// if spoof checks are required and we won't automatically spoof check this player then tell them how to spoof check
	// e.g. if automatic spoof checks are disabled, or if automatic spoof checks are done on admins only and this player isn't an admin

	if( m_GHost->m_RequireSpoofChecks && !Player->GetWhoisShouldBeSent( ) )
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			// note: the following (commented out) line of code will crash because calling GetUniqueName( ) twice will result in two different return values
			// and unfortunately iterators are not valid if compared against different containers
			// this comment shall serve as warning to not make this mistake again since it has now been made twice before in GHost++
			// string( (*i)->GetUniqueName( ).begin( ), (*i)->GetUniqueName( ).end( ) )

			BYTEARRAY UniqueName = (*i)->GetUniqueName( );

			if( (*i)->GetServer( ) == JoinedRealm )
				SendChat( Player, m_GHost->m_Language->SpoofCheckByWhispering( string( UniqueName.begin( ), UniqueName.end( ) )  ) );
		}
	}


	// show current player's .sd

	if (m_GHost->m_ShowScoresOnJoin)
		m_ShowScoreOf = Player->GetName();

	// show current player's .note
	if (m_GHost->m_ShowNotesOnJoin)
	{
		bool noted = false;
		string note = Note(Player->GetName());
		noted = IsNoted( Player->GetName());
		if (noted)
			SendAdminChat(Player->GetName()+" - "+note);
	}

	// check for multiple IP usage
	string Room;
	if (JoinedRealm.find("LAN") != string::npos || JoinedRealm == "LAN" || JoinedRealm.size() < 3 )
		Room=potential->GetRoomName( );
	else Room=".";
	SendAllChat( "Player [" + joinPlayer->GetName( ) + "] has joined from [" + ( JoinedRealm == string( ) ? "LAN" : JoinedRealm ) + "] "+ Room );
	CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "(" + Player->GetExternalIPString( ) + "|" + From + ")] has joined from [" + ( JoinedRealm == string( ) ? "LAN" : JoinedRealm ) + "] "+ Room );
	if( m_GHost->m_CheckMultipleIPUsage )
	{
		string Others;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( Player != *i && Player->GetExternalIPString( ) == (*i)->GetExternalIPString( ) )
			{
				if( Others.empty( ) )
					Others = (*i)->GetName( );
				else
					Others += ", " + (*i)->GetName( );
			}
		}

//		if( !Others.empty( ) )
//			SendAllChat( m_GHost->m_Language->MultipleIPAddressUsageDetected( joinPlayer->GetName( ), Others ) );
	}

	// abort the countdown if there was one in progress

	if( m_CountDownStarted && !m_GameLoading && !m_GameLoaded )
	{
		SendAllChat( m_GHost->m_Language->CountDownAborted( ) );
		m_CountDownStarted = false;
	}

	// auto lock the game

	if( m_GHost->m_AutoLock && !m_Locked && IsOwner( joinPlayer->GetName( ) ) )
	{
		SendAllChat( m_GHost->m_Language->GameLocked( ) );
		m_Locked = true;
	}
}

void CBaseGame :: EventPlayerJoinedWithScore( CPotentialPlayer *potential, CIncomingJoinPlayer *joinPlayer, double score )
{
	// this function is only called when matchmaking is enabled
	// EventPlayerJoined will be called first in all cases
	// if matchmaking is enabled EventPlayerJoined will start a database query to retrieve the player's score and keep the connection open while we wait
	// when the database query is complete EventPlayerJoinedWithScore will be called

	// check if the new player's name is the same as the virtual host name

	if( joinPlayer->GetName( ) == m_VirtualHostName )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game with the virtual host name" );
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	// check if the new player's name is already taken

	if( GetPlayerFromName( joinPlayer->GetName( ), false ) )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but that name is already taken" );
		// SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButTaken( joinPlayer->GetName( ) ) );
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	// check if the new player's score is within the limits

	if( score > -99999.0 && ( score < m_MinimumScore || score > m_MaximumScore ) )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but has a rating [" + UTIL_ToString( score, 2 ) + "] outside the limits [" + UTIL_ToString( m_MinimumScore, 2 ) + "] to [" + UTIL_ToString( m_MaximumScore, 2 ) + "]" );
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	bool AdminCheck = false;

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if( (*i)->IsAdmin( joinPlayer->GetName( ) ) || (*i)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			AdminCheck = true;
			break;
		}
	}

	// test if player is safelisted

	bool SafeCheck = false;
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if( (*i)->IsSafe( joinPlayer->GetName( ) ) )
		{
			SafeCheck = true;
			break;
		}
	}

	bool Reserved = IsReserved( joinPlayer->GetName( ) ) || AdminCheck || IsOwner( joinPlayer->GetName( )) || SafeCheck;

	bool RootAdminCheck = false;
	for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
	{
		if( (*j)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			RootAdminCheck = true;
			break;
		}
	}

	// try to find an empty slot

	unsigned char SID = GetEmptySlot( false );

	bool AnyAdminCheck = false;

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if( (*i)->IsAdmin( joinPlayer->GetName( ) ) || (*i)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			AnyAdminCheck = true;
			break;
		}
	}

	if( SID == 255 )
	{
		// no empty slot found, time to do some matchmaking!
		// note: the database code uses a score of -100000 to denote "no score"

		if( m_GHost->m_MatchMakingMethod == 0 )
		{
			// method 0: don't do any matchmaking
			// that was easy!
		}
		else if( m_GHost->m_MatchMakingMethod == 1 )
		{
			// method 1: furthest score method
			// calculate the average score of all players in the game
			// then kick the player with the score furthest from that average (or a player without a score)
			// this ensures that the players' scores will tend to converge as players join the game

			double AverageScore = 0.0;
			uint32_t PlayersScored = 0;

			if( score > -99999.0 )
			{
				AverageScore = score;
				PlayersScored = 1;
			}

			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( (*i)->GetScore( ) > -99999.0 )
				{
					AverageScore += (*i)->GetScore( );
					PlayersScored++;
				}
			}

			if( PlayersScored > 0 )
				AverageScore /= PlayersScored;

			// calculate the furthest player from the average

			CGamePlayer *FurthestPlayer = NULL;

			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( !FurthestPlayer || (*i)->GetScore( ) < -99999.0 || abs( (*i)->GetScore( ) - AverageScore ) > abs( FurthestPlayer->GetScore( ) - AverageScore ) )
					FurthestPlayer = *i;
			}

			if( !FurthestPlayer )
			{
				// this should be impossible

				CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but no furthest player was found (this should be impossible)" );
				potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
				potential->SetDeleteMe( true );
				return;
			}

			// kick the new player if they have the furthest score

			if( score < -99999.0 || abs( score - AverageScore ) > abs( FurthestPlayer->GetScore( ) - AverageScore ) )
			{
				if( score < -99999.0 )
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but has the furthest rating [N/A] from the average [" + UTIL_ToString( AverageScore, 2 ) + "]" );
				else
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but has the furthest rating [" + UTIL_ToString( score, 2 ) + "] from the average [" + UTIL_ToString( AverageScore, 2 ) + "]" );

				potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
				potential->SetDeleteMe( true );
				return;
			}

			// kick the furthest player

			SID = GetSIDFromPID( FurthestPlayer->GetPID( ) );
			FurthestPlayer->SetDeleteMe( true );

			if( FurthestPlayer->GetScore( ) < -99999.0 )
				FurthestPlayer->SetLeftReason( m_GHost->m_Language->WasKickedForHavingFurthestScore( "N/A", UTIL_ToString( AverageScore, 2 ) ) );
			else
				FurthestPlayer->SetLeftReason( m_GHost->m_Language->WasKickedForHavingFurthestScore( UTIL_ToString( FurthestPlayer->GetScore( ), 2 ), UTIL_ToString( AverageScore, 2 ) ) );

			FurthestPlayer->SetLeftCode( PLAYERLEAVE_LOBBY );

			// send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
			// we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

			SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( FurthestPlayer->GetPID( ), FurthestPlayer->GetLeftCode( ) ) );
			FurthestPlayer->SetLeftMessageSent( true );

			if( FurthestPlayer->GetScore( ) < -99999.0 )
				SendAllChat( m_GHost->m_Language->PlayerWasKickedForFurthestScore( FurthestPlayer->GetName( ), "N/A", UTIL_ToString( AverageScore, 2 ) ) );
			else
				SendAllChat( m_GHost->m_Language->PlayerWasKickedForFurthestScore( FurthestPlayer->GetName( ), UTIL_ToString( FurthestPlayer->GetScore( ), 2 ), UTIL_ToString( AverageScore, 2 ) ) );
		}
		else if( m_GHost->m_MatchMakingMethod == 2 )
		{
			// method 2: lowest score method
			// kick the player with the lowest score (or a player without a score)

			CGamePlayer *LowestPlayer = NULL;

			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( !LowestPlayer || (*i)->GetScore( ) < -99999.0 || (*i)->GetScore( ) < LowestPlayer->GetScore( ) )
					LowestPlayer = *i;
			}

			if( !LowestPlayer )
			{
				// this should be impossible

				CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but no lowest player was found (this should be impossible)" );
				potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
				potential->SetDeleteMe( true );
				return;
			}

			// kick the new player if they have the lowest score

			if( score < -99999.0 || score < LowestPlayer->GetScore( ) )
			{
				if( score < -99999.0 )
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but has the lowest rating [N/A]" );
				else
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but has the lowest rating [" + UTIL_ToString( score, 2 ) + "]" );

				potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
				potential->SetDeleteMe( true );
				return;
			}

			// kick the lowest player

			SID = GetSIDFromPID( LowestPlayer->GetPID( ) );
			LowestPlayer->SetDeleteMe( true );

			if( LowestPlayer->GetScore( ) < -99999.0 )
				LowestPlayer->SetLeftReason( m_GHost->m_Language->WasKickedForHavingLowestScore( "N/A" ) );
			else
				LowestPlayer->SetLeftReason( m_GHost->m_Language->WasKickedForHavingLowestScore( UTIL_ToString( LowestPlayer->GetScore( ), 2 ) ) );

			LowestPlayer->SetLeftCode( PLAYERLEAVE_LOBBY );

			// send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
			// we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

			SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( LowestPlayer->GetPID( ), LowestPlayer->GetLeftCode( ) ) );
			LowestPlayer->SetLeftMessageSent( true );

			if( LowestPlayer->GetScore( ) < -99999.0 )
				SendAllChat( m_GHost->m_Language->PlayerWasKickedForLowestScore( LowestPlayer->GetName( ), "N/A" ) );
			else
				SendAllChat( m_GHost->m_Language->PlayerWasKickedForLowestScore( LowestPlayer->GetName( ), UTIL_ToString( LowestPlayer->GetScore( ), 2 ) ) );
		}
	}

	if( SID >= m_Slots.size( ) )
	{
		potential->Send( m_Protocol->SEND_W3GS_REJECTJOIN( REJECTJOIN_FULL ) );
		potential->SetDeleteMe( true );
		return;
	}

	// we have a slot for the new player
	// make room for them by deleting the virtual host player if we have to

	uint32_t SlotReq = 11;
	if (m_ShowRealSlotCount)
		SlotReq = m_Slots.size()-1;

	if( GetSlotsOccupied( ) >= SlotReq )
		DeleteVirtualHost( );

	// identify their joined realm
	// this is only possible because when we send a game refresh via LAN or battle.net we encode an ID value in the 4 most significant bits of the host counter
	// the client sends the host counter when it joins so we can extract the ID value here
	// note: this is not a replacement for spoof checking since it doesn't verify the player's name and it can be spoofed anyway

	uint32_t HostCounterID = joinPlayer->GetHostCounter( ) >> 28;
	string JoinedRealm;

	// we use an ID value of 0 to denote joining via LAN

	if( HostCounterID != 0 )
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			if( (*i)->GetHostCounterID( ) == HostCounterID )
				JoinedRealm = (*i)->GetServer( );
		}
	}

	// turning the CPotentialPlayer into a CGamePlayer is a bit of a pain because we have to be careful not to close the socket
	// this problem is solved by setting the socket to NULL before deletion and handling the NULL case in the destructor
	// we also have to be careful to not modify the m_Potentials vector since we're currently looping through it

	CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] joined the game with score" );
	CGamePlayer *Player = new CGamePlayer( potential, GetNewPID( ), JoinedRealm, joinPlayer->GetName( ), joinPlayer->GetInternalIP( ), false );

	// consider LAN players to have already spoof checked since they can't
	// since so many people have trouble with this feature we now use the JoinedRealm to determine LAN status

	if( JoinedRealm.empty( ) )
		Player->SetSpoofed( true );

	Player->SetWhoisShouldBeSent( m_GHost->m_SpoofChecks == 1 || ( m_GHost->m_SpoofChecks == 2 && AnyAdminCheck ) );
	Player->SetSID( SID );
	Player->SetScore( score );
	m_Players.push_back( Player );
	potential->SetSocket( NULL );
	potential->SetDeleteMe( true );
	m_Slots[SID] = CGameSlot( Player->GetPID( ), 255, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam( ), m_Slots[SID].GetColour( ), m_Slots[SID].GetRace( ) );

	// send slot info to the new player
	// the SLOTINFOJOIN packet also tells the client their assigned PID and that the join was successful

	Player->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( Player->GetPID( ), Player->GetSocket( )->GetPort( ), Player->GetExternalIP( ), m_Slots, m_RandomSeed, m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0, m_Map->GetMapNumPlayers( ) ) );

	// send virtual host info and fake player info (if present) to the new player

	SendVirtualHostPlayerInfo( Player );
	SendFakePlayerInfo( Player );
	SendWTVPlayerInfo( Player );

	BYTEARRAY BlankIP;
	BlankIP.push_back( 0 );
	BlankIP.push_back( 0 );
	BlankIP.push_back( 0 );
	BlankIP.push_back( 0 );


	// check if the new player's ip is banned
	if (Player->GetExternalIPString()!="127.0.0.1")
	if (m_GHost->m_IPBanning!=0)
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		CDBBan *IPBan = (*i)->IsBannedIP(Player->GetExternalIPString( ));

		if( IPBan )
		{
			string sReason = IPBan->GetReason();
			string sName = IPBan->GetName();
			delete IPBan;
			CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + Player->GetName( ) +"("+Player->GetExternalIPString()+")"+ "] is trying to join the game but is IP banned" );
/*
			string n=Player->GetName();
			string s(16-n.length(),' ');
			n=s+n;*/
			if (m_GHost->m_IPBanning==1)
			if (m_GHost->m_Verbose)
			{
				SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButBanned( Player->GetName()+"("+Player->GetExternalIPString()+")", IPBan->GetAdmin()) );
				if (sReason!="")
					SendAllChat ( sName +"("+Player->GetExternalIPString()+") ban reason: "+sReason);
			}
			if (m_GHost->m_IPBanning==1)
			{
				Player->SetDeleteMe( true );
				Player->SetLeftReason( "was autokicked, " + Player->GetName( ) +Player->GetExternalIPString() + " IP not allowed");
				OpenSlot( GetSIDFromPID( Player->GetPID( ) ), false );
				m_Players.pop_back();
				return;
			}
			string sBan = Player->GetName()+"("+Player->GetExternalIPString()+") is IP banned";
			string sBReason = sBan + ", "+sReason;

			if (sReason=="")
				SendAllChat( sBan );
			else
			{
				if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
					SendAllChat( sBReason );
				else
				{
					SendAllChat( sBan );
					SendAllChat( "Ban reason: " + sReason );
				}
			}
		}
	}

	// check if we only allow garena

	if (m_GarenaOnly)
	{
		string EIP = Player->GetExternalIPString();
// kick if not garena, admin, rootadmin, reserver
		if (EIP!="127.0.0.1" && EIP!=m_GHost->m_ExternalIP && !IsOwner( Player->GetName( ) ) && !AdminCheck && !RootAdminCheck && !IsReserved (Player->GetName()))
		{
			Player->SetDeleteMe( true );
			Player->SetLeftReason( "was autokicked, GArena only");
			Player->SetLeftCode( PLAYERLEAVE_LOBBY );
			OpenSlot( GetSIDFromPID( Player->GetPID( ) ), false );
			m_Players.pop_back();
			return;
		}
	}

	// check if the country or provider is not allowed
	string From;
	if (Player->GetExternalIPString()!="127.0.0.1")
	if (!Reserved)
	if (m_CountryCheck || m_CountryCheck2 || m_ProviderCheck || m_ProviderCheck2)
	{
		string P;
		string s;
		bool bad = false;
		bool allowed=false;
		if (m_ProviderCheck2)
			allowed= true;
		if (m_ProviderCheck)
			allowed= false;
		{
			From = Player->GetCountry();
			if (m_CountryCheck)
				if (m_Countries.find(From)==string :: npos)
					bad=true;

			if (!bad)
			if (m_CountryCheck2)
				if (m_Countries2.find(From)!=string :: npos)
					bad=true;
			
			if (!bad)
			if (m_ProviderCheck || m_ProviderCheck2)
			{
				P = Player->GetProvider();
				transform( P.begin( ), P.end( ), P.begin( ), (int(*)(int))toupper );						
				From = From + " "+ P;
			}
			if (!bad)
			if (m_ProviderCheck)
			{
				stringstream SS;
				SS << m_Providers;

				while( !SS.eof( ) )
				{
					SS >> s;
					if (P.find(s)!=string :: npos)
						allowed=true;
				}
				if (!allowed)
					bad=true;
			}
			
			if (!bad)
			if (m_ProviderCheck2)
			{
				stringstream SS;
				SS << m_Providers2;

				while( !SS.eof( ) )
				{
					SS >> s;
					if (P.find(s)!=string :: npos)
						allowed=false;
				}
				if (!allowed)
					bad=true;
			}
			if (bad)
			{
				string n=Player->GetName();
				string s(16-n.length(),' ');
				n=s+n;
				if (m_GHost->m_Verbose)
				SendAllChat( m_GHost->m_Language->AutokickingPlayerForDeniedCountry( n, From ) );
		//			potential->SetSocket( NULL );
		//			potential->SetDeleteMe( true );
				Player->SetDeleteMe( true );
				Player->SetLeftReason( "was autokicked, " + From + " not allowed");
				Player->SetLeftCode( PLAYERLEAVE_LOBBY );

				OpenSlot( GetSIDFromPID( Player->GetPID( ) ), false );
				m_Players.pop_back();
				return;
			}
		}
	}

	CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + Player->GetName( ) + "] from [" + From + "] joined the game with score" );
	m_GHost->UDPChatSend("|newplayer "+Player->GetName());

// recalculate nr of players in each team + difference in players nr.
	
	ReCalculateTeams();

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) && *i != Player )
		{
			// send info about the new player to every other player

			if( (*i)->GetSocket( ) )
			{
				if( m_GHost->m_HideIPAddresses )
					(*i)->Send( m_Protocol->SEND_W3GS_PLAYERINFO( Player->GetPID( ), Player->GetName( ), BlankIP, BlankIP ) );
				else
					(*i)->Send( m_Protocol->SEND_W3GS_PLAYERINFO( Player->GetPID( ), Player->GetName( ), Player->GetExternalIP( ), Player->GetInternalIP( ) ) );
			}

			// send info about every other player to the new player

			if( m_GHost->m_HideIPAddresses )
				Player->Send( m_Protocol->SEND_W3GS_PLAYERINFO( (*i)->GetPID( ), (*i)->GetName( ), BlankIP, BlankIP ) );
			else
				Player->Send( m_Protocol->SEND_W3GS_PLAYERINFO( (*i)->GetPID( ), (*i)->GetName( ), (*i)->GetExternalIP( ), (*i)->GetInternalIP( ) ) );
		}
	}

	// check for multiple ip usage.
	vector<string>IPs;
	bool sayit = false;
	vector<string>Players;
	string IP1, IP2;
	bool pp1, pp2;
	bool IPfound;
	string :: size_type pfound;
	vector<string> :: iterator p;
	if (Player->GetExternalIPString()!="127.0.0.1")
	{
		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			IP1 = (*i)->GetExternalIPString();
		// ignore host' LAN
			if (IP1!=m_GHost->m_ExternalIP)
			for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
			{
				// compare with all the other players except himself.
				if ((*i)!=(*j))
				{
					IP2 = (*j)->GetExternalIPString();
					// matching IP
					if (IP1==IP2)
					{

						if (Player->GetName( )==(*i)->GetName() || Player->GetName( )==(*j)->GetName())
							sayit = true;
						pp1 = false;
						pp2 = false;
						// already in the list of matching IPs?
						IPfound = false;
						p = Players.begin();
						if (IPs.size()>0)
						for (vector<string> :: iterator s = IPs.begin(); s != IPs.end(); s++)
						{
							if ((*s)==IP1)
							{
								IPfound = true;
								// already in the list, check too see if the players are in it
								pfound = (*p).find( (*j)->GetName() );
								if( pfound != string :: npos )
									pp2 = true;
								pfound = (*p).find( (*i)->GetName() );
								if( pfound != string :: npos )
									pp1 = true;

								if (!pp1)
									(*p)+="="+(*i)->GetName();
								if (!pp2)
									(*p)+="="+(*j)->GetName();
							}
							p++;
						}
						// the IP is not yet in the list, add it together with the two player names
						if (!IPfound)
						{
							IPs.push_back(IP1);
							Players.push_back((*i)->GetName()+"="+(*j)->GetName());
						}
					}

				}
			}
		}
	}
	// multiple IP usage was found, list it if last player who joined is part of them
	if (Player->GetExternalIPString()!="127.0.0.1")
	if (IPs.size()>0 && sayit)
	{
		string IPString;
		p = Players.begin();
		IPString = "Same IPs: ";
		for (vector<string> :: iterator s = IPs.begin(); s != IPs.end(); s++)
		{
			IPString += (*p);
			if (s!=IPs.end()-1)
				IPString +=", ";
			p++;
		}
		SendAllChat(IPString);
		m_GHost->UDPChatSend("|sameip");
	}

	// show country and pings when every slot has been occupied.
	if (GetSlotsOpen()==0 && !m_AllSlotsOccupied)
	{
		m_AllSlotsOccupied = true;
		m_AllSlotsAnnounced = false;
		m_SlotsOccupiedTime = GetTime( );
	}

	// update LastPlayerJoined

	m_LastPlayerJoined = Player->GetPID();
	m_LastPlayerJoinedTime = GetTime();

	// send a map check packet to the new player

	Player->Send( m_Protocol->SEND_W3GS_MAPCHECK( m_Map->GetMapPath( ), m_Map->GetMapSize( ), m_Map->GetMapInfo( ), m_Map->GetMapCRC( ), m_Map->GetMapSHA1( ) ) );

	// send slot info to everyone, so the new player gets this info twice but everyone else still needs to know the new slot layout

	SendAllSlotInfo( );

	// send a welcome message

	SendWelcomeMessage( Player );

	// if spoof checks are required and we won't automatically spoof check this player then tell them how to spoof check
	// e.g. if automatic spoof checks are disabled, or if automatic spoof checks are done on admins only and this player isn't an admin

	if( m_GHost->m_RequireSpoofChecks && !Player->GetWhoisShouldBeSent( ) )
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			// note: the following (commented out) line of code will crash because calling GetUniqueName( ) twice will result in two different return values
			// and unfortunately iterators are not valid if compared against different containers
			// this comment shall serve as warning to not make this mistake again since it has now been made twice before in GHost++
			// string( (*i)->GetUniqueName( ).begin( ), (*i)->GetUniqueName( ).end( ) )

			BYTEARRAY UniqueName = (*i)->GetUniqueName( );

			if( (*i)->GetServer( ) == JoinedRealm )
				SendChat( Player, m_GHost->m_Language->SpoofCheckByWhispering( string( UniqueName.begin( ), UniqueName.end( ) )  ) );
		}
	}

	if( score < -99999.0 )
		SendAllChat( m_GHost->m_Language->PlayerHasScore( joinPlayer->GetName( ), "N/A" ) );
	else
		SendAllChat( m_GHost->m_Language->PlayerHasScore( joinPlayer->GetName( ), UTIL_ToString( score, 2 ) ) );

	uint32_t PlayersScored = 0;
	uint32_t PlayersNotScored = 0;
	double AverageScore = 0.0;
	double MinScore = 0.0;
	double MaxScore = 0.0;
	bool Found = false;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
		{
			if( (*i)->GetScore( ) < -99999.0 )
				PlayersNotScored++;
			else
			{
				PlayersScored++;
				AverageScore += (*i)->GetScore( );

				if( !Found || (*i)->GetScore( ) < MinScore )
					MinScore = (*i)->GetScore( );

				if( !Found || (*i)->GetScore( ) > MaxScore )
					MaxScore = (*i)->GetScore( );

				Found = true;
			}
		}
	}

	double Spread = MaxScore - MinScore;
	SendAllChat( m_GHost->m_Language->RatedPlayersSpread( UTIL_ToString( PlayersScored ), UTIL_ToString( PlayersScored + PlayersNotScored ), UTIL_ToString( (uint32_t)Spread ) ) );

	// check for multiple IP usage

	if( m_GHost->m_CheckMultipleIPUsage )
	{
		string Others;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( Player != *i && Player->GetExternalIPString( ) == (*i)->GetExternalIPString( ) )
			{
				if( Others.empty( ) )
					Others = (*i)->GetName( );
				else
					Others += ", " + (*i)->GetName( );
			}
		}

//		if( !Others.empty( ) )
//			SendAllChat( m_GHost->m_Language->MultipleIPAddressUsageDetected( joinPlayer->GetName( ), Others ) );
	}

	// abort the countdown if there was one in progress

	if( m_CountDownStarted && !m_GameLoading && !m_GameLoaded )
	{
		SendAllChat( m_GHost->m_Language->CountDownAborted( ) );
		m_CountDownStarted = false;
	}

	// auto lock the game

	if( m_GHost->m_AutoLock && !m_Locked && IsOwner( joinPlayer->GetName( ) ) )
	{
		SendAllChat( m_GHost->m_Language->GameLocked( ) );
		m_Locked = true;
	}

	// balance the slots

	if( m_AutoStartPlayers != 0 && GetNumHumanPlayers( ) == m_AutoStartPlayers )
		BalanceSlots( );
}

void CBaseGame :: EventPlayerLeft( CGamePlayer *player, uint32_t reason )
{
	// this function is only called when a player leave packet is received, not when there's a socket error, kick, etc...

	uint32_t GameNr = GetGameNr();

	bool show = true;
	if (m_GHost->m_CurrentGame)
		if	(m_GHost->m_CurrentGame->GetCreationTime()==GetCreationTime())
			show = false;
	if (show && !m_GameEnded)
		m_GHost->UDPChatSend("|leaver "+UTIL_ToString(GameNr)+" "+player->GetName());


	m_LastLeaverTicks = GetTicks();
	player->SetDeleteMe( true );
	if( reason == PLAYERLEAVE_GPROXY )
		player->SetLeftReason( m_GHost->m_Language->WasUnrecoverablyDroppedFromGProxy( ) );
	else
		player->SetLeftReason( m_GHost->m_Language->HasLeftVoluntarily( ) );
	player->SetLeftCode( PLAYERLEAVE_LOST );

	ReCalculateTeams();

	if( !m_GameLoading && !m_GameLoaded )
		OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );

	//ban leaver who left the game in 30 secs after finishing map downloading.
	if( !m_DownloadOnlyMode && player->GetDownloadFinished( ) && GetTime( ) - player->GetFinishedDownloadingTime( ) < 30 )
	{
		SendAllChat(player->GetName() + " BANNED for dl & early leaving in lobby" );
		SendChat(player->GetPID(), ", left the lobby in 30 secs after downloaded gets banned for 2 days. DL xong, trong 30s ma out la bi ban nick 2 ngay" );
		string Reason = "Leaver in 30 secs after downloaded gets banned for 2 days";
		Reason = "Autobanned "+Reason;
		CONSOLE_Print( "[AUTOBAN2days: " + m_GameName + "] Autobanning " + player->GetName( ) + " (" + Reason +")" );
		for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
            { 
				(*j)-> AddBan(player->GetName( ),player->GetExternalIPString( ), "12", "Autobot", " reason: DL and leave", "0");
            }
 
        m_GHost->m_DB->ThreadedBanAdd(" ",player->GetName( ), player->GetExternalIPString( ), "in lobby", "Autobot","DL and leave",2,0);
	}
	if (IsOwner(player->GetName())){
		if ( m_GHost->m_NewOwner < 2 )
			m_GHost->m_NewOwner = 0;
		m_OwnerName = m_DefaultOwner;
		CONSOLE_Print( "[GAME: " + m_GameName + "] Return OwnerName [" + m_OwnerName +"] = DefaultOwner [" + m_DefaultOwner + "]" );
	}
}

void CBaseGame :: EventPlayerLoaded( CGamePlayer *player )
{
	CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + player->GetName( ) + "] finished loading in " + UTIL_ToString( (float)( player->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks ) / 1000, 2 ) + " seconds" );

	if( m_LoadInGame )
	{
		// send any buffered data to the player now
		// see the Update function for more information about why we do this
		// this includes player loaded messages, game updates, and player leave messages

		queue<BYTEARRAY> *LoadInGameData = player->GetLoadInGameData( );

		while( !LoadInGameData->empty( ) )
		{
			Send( player, LoadInGameData->front( ) );
			LoadInGameData->pop( );
		}

		// start the lag screen for the new player

		bool FinishedLoading = true;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			FinishedLoading = (*i)->GetFinishedLoading( );

			if( !FinishedLoading )
				break;
		}

		if( !FinishedLoading )
			Send( player, m_Protocol->SEND_W3GS_START_LAG( m_Players, true ) );

		// remove the new player from previously loaded players' lag screens

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( *i != player && (*i)->GetFinishedLoading( ) )
				Send( *i, m_Protocol->SEND_W3GS_STOP_LAG( player ) );
		}

		// send a chat message to previously loaded players

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( *i != player && (*i)->GetFinishedLoading( ) )
				SendChat( *i, m_GHost->m_Language->PlayerFinishedLoading( player->GetName( ) ) );
		}

		if( !FinishedLoading )
			SendChat( player, m_GHost->m_Language->PleaseWaitPlayersStillLoading( ) );

		// if HCL was sent message first player not to set the map mode
		unsigned char Nr=255;
		unsigned char Nrt;
		string sSlotNr;
		string sGameNr;
		CGamePlayer *p = NULL;
		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			Nrt = GetSIDFromPID((*i)->GetPID());
			if (Nrt<Nr)
			{
				Nr = Nrt;
				p = (*i);
			}
		}
		if (m_HCL && p!=NULL && p==player && !m_HCLCommandString.empty())
		{
			SendChat(p->GetPID(), "-" + m_HCLCommandString + " auto set, no need to type it in " + p->GetName() );
		}
	}
	else
		SendAll( m_Protocol->SEND_W3GS_GAMELOADED_OTHERS( player->GetPID( ) ) );
}

void CBaseGame :: EventPlayerAction( CGamePlayer *player, CIncomingAction *action )
{
	m_Actions.push( action );

	// check for players saving the game and notify everyone

	if( !action->GetAction( )->empty( ) && (*action->GetAction( ))[0] == 6 )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + player->GetName( ) + "] is saving the game" );
		SendAllChat( m_GHost->m_Language->PlayerIsSavingTheGame( player->GetName( ) ) );
	}
}

void CBaseGame :: EventPlayerKeepAlive( CGamePlayer *player, uint32_t checkSum )
{
	if( m_GHost->m_dropifdesync)
	{
		// check for desyncs
		// however, it's possible that not every player has sent a checksum for this frame yet
		// first we verify that we have enough checksums to work with otherwise we won't know exactly who desynced

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetDeleteMe( ) && (*i)->GetCheckSums( )->empty( ) )
				return;
		}

		// now we check for desyncs since we know that every player has at least one checksum waiting

		bool FoundPlayer = false;
		uint32_t FirstCheckSum = 0;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetDeleteMe( ) )
			{
				FoundPlayer = true;
				FirstCheckSum = (*i)->GetCheckSums( )->front( );
				break;
			}
		}

		if( !FoundPlayer )
			return;

		bool AddToReplay = true;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetDeleteMe( ) && (*i)->GetCheckSums( )->front( ) != FirstCheckSum )
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] desync detected" );
				SendAllChat( m_GHost->m_Language->DesyncDetected( ) );
	
				// try to figure out who desynced
				// this is complicated by the fact that we don't know what the correct game state is so we let the players vote
				// put the players into bins based on their game state

				map<uint32_t, vector<unsigned char> > Bins;

				for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
				{
					if( !(*j)->GetDeleteMe( ) )
						Bins[(*j)->GetCheckSums( )->front( )].push_back( (*j)->GetPID( ) );
				}

				uint32_t StateNumber = 1;
				map<uint32_t, vector<unsigned char> > :: iterator LargestBin = Bins.begin( );
				bool Tied = false;

				for( map<uint32_t, vector<unsigned char> > :: iterator j = Bins.begin( ); j != Bins.end( ); j++ )
				{
					if( (*j).second.size( ) > (*LargestBin).second.size( ) )
					{
						LargestBin = j;
						Tied = false;
					}
					else if( j != LargestBin && (*j).second.size( ) == (*LargestBin).second.size( ) )
						Tied = true;

					string Players;
	
					for( vector<unsigned char> :: iterator k = (*j).second.begin( ); k != (*j).second.end( ); k++ )
					{
						CGamePlayer *Player = GetPlayerFromPID( *k );
	
						if( Player )
						{
							if( Players.empty( ) )
								Players = Player->GetName( );
							else
							Players += ", " + Player->GetName( );
						}
					}
	
					SendAllChat( m_GHost->m_Language->PlayersInGameState( UTIL_ToString( StateNumber ), Players ) );
					StateNumber++;
				}

				FirstCheckSum = (*LargestBin).first;

				if( Tied )
				{
					// there is a tie, which is unfortunate
					// the most common way for this to happen is with a desync in a 1v1 situation
					// this is not really unsolvable since the game shouldn't continue anyway so we just kick both players
					// in a 2v2 or higher the chance of this happening is very slim
					// however, we still kick every player because it's not fair to pick one or another group
					// todotodo: it would be possible to split the game at this point and create a "new" game for each game state

					CONSOLE_Print( "[GAME: " + m_GameName + "] can't kick desynced players because there is a tie, kicking all players instead" );
					StopPlayers( m_GHost->m_Language->WasDroppedDesync( ) );
					AddToReplay = false;
				}
				else
				{
					CONSOLE_Print( "[GAME: " + m_GameName + "] kicking desynced players" );
	
					for( map<uint32_t, vector<unsigned char> > :: iterator j = Bins.begin( ); j != Bins.end( ); j++ )
					{
						// kick players who are NOT in the largest bin
						// examples: suppose there are 10 players
						// the most common case will be 9v1 (e.g. one player desynced and the others were unaffected) and this will kick the single outlier
						// another (very unlikely) possibility is 8v1v1 or 8v2 and this will kick both of the outliers, regardless of whether their game states match

						if( (*j).first != (*LargestBin).first )
						{
							for( vector<unsigned char> :: iterator k = (*j).second.begin( ); k != (*j).second.end( ); k++ )
							{
								CGamePlayer *Player = GetPlayerFromPID( *k );

								if( Player )
								{
									Player->SetDeleteMe( true );
									Player->SetLeftReason( m_GHost->m_Language->WasDroppedDesync( ) );
									Player->SetLeftCode( PLAYERLEAVE_LOST );
								}
							}
						}
					}
				}
	
				// don't continue looking for desyncs, we already found one!

				break;
			}
		}

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetDeleteMe( ) )
				(*i)->GetCheckSums( )->pop( );
		}

		// add checksum to replay

		if( m_Replay && AddToReplay )
			m_Replay->AddCheckSum( FirstCheckSum );
	}
	else
	{
		// check for desyncs
		// however, it's possible that not every player has sent a checksum for this frame yet
		// first we verify that we have enough checksums to work with otherwise we won't know exactly who desynced

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( (*i)->GetCheckSums( )->empty( ) )
				return;
		}

		// now we check for desyncs since we know that every player has at least one checksum waiting

		uint32_t FirstCheckSum = player->GetCheckSums( )->front( );

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !m_Desynced && (*i)->GetCheckSums( )->front( ) != FirstCheckSum )
			{
				m_Desynced = true;
				CONSOLE_Print( "[GAME: " + m_GameName + "] desync detected" );
				SendAllChat( m_GHost->m_Language->DesyncDetected( ) );

				// try to figure out who desynced
				// this is complicated by the fact that we don't know what the correct game state is so we let the players vote
				// put the players into bins based on their game state

				map<uint32_t, vector<unsigned char> > Bins;

				for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
					Bins[(*j)->GetCheckSums( )->front( )].push_back( (*j)->GetPID( ) );

				uint32_t StateNumber = 1;
				map<uint32_t, vector<unsigned char> > :: iterator LargestBin = Bins.begin( );
				bool Tied = false;

				for( map<uint32_t, vector<unsigned char> > :: iterator j = Bins.begin( ); j != Bins.end( ); j++ )
				{
					if( (*j).second.size( ) > (*LargestBin).second.size( ) )
					{
						LargestBin = j;
						Tied = false;
					}
					else if( j != LargestBin && (*j).second.size( ) == (*LargestBin).second.size( ) )
						Tied = true;

					string Players;

					for( vector<unsigned char> :: iterator k = (*j).second.begin( ); k != (*j).second.end( ); k++ )
					{
						CGamePlayer *Player = GetPlayerFromPID( *k );
	
						if( Player )
						{
							if( Players.empty( ) )
								Players = Player->GetName( );
							else
								Players += ", " + Player->GetName( );
						}
					}

					SendAllChat( m_GHost->m_Language->PlayersInGameState( UTIL_ToString( StateNumber ), Players ) );
					StateNumber++;
				}

				// todotodo: kick the desynced player(s) and don't stop recording the replay

				/*

				if( Tied )
				{
					// can't kick
				}
				else
				{
					for( map<uint32_t, vector<unsigned char> > :: iterator j = Bins.begin( ); j != Bins.end( ); j++ )
					{
						if( (*j).first != (*LargestBin).first )
						{
							for( vector<unsigned char> :: iterator k = (*j).second.begin( ); k != (*j).second.end( ); k++ )
							{
								CGamePlayer *Player = GetPlayerFromPID( *k );
	
								if( Player )
								{
									Player->SetDeleteMe( true );
									Player->SetLeftReason( "was dropped due to desync" );
									Player->SetLeftCode( PLAYERLEAVE_LOBBY );
								}
							}
						}
					}
				}

				*/
			}
		}

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			(*i)->GetCheckSums( )->pop( );

		// add checksum to replay but only if we're not desynced

		if( m_Replay && !m_Desynced )
			m_Replay->AddCheckSum( FirstCheckSum );
	}
}

void CBaseGame :: EventPlayerChatToHost( CGamePlayer *player, CIncomingChatPlayer *chatPlayer )
{
	if( chatPlayer->GetFromPID( ) == player->GetPID( ) )
	{
		if( chatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_MESSAGE || chatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_MESSAGEEXTRA )
		{
			// relay the chat message to other players

			bool isAdmin = IsOwner(player->GetName());
			bool isRootAdmin = isAdmin;
			for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
			{
				if( (*j)->IsAdmin(player->GetName()))
				{
					isAdmin = true;
				}
				if( (*j)->IsRootAdmin( player->GetName() ) )
				{
					isRootAdmin = true;
					isAdmin = true;
				}
			}

			string msg = chatPlayer->GetMessage( ); 
			bool iscmd = false;
			if( !msg.empty( ) && msg[0] == m_GHost->m_CommandTrigger )
				iscmd = true;
			bool Relay = true;
			bool cRelay = !(iscmd && !m_GHost->m_RelayChatCommands);

			if( !m_GameLoading )
			{
				BYTEARRAY ExtraFlags = chatPlayer->GetExtraFlags( );
				
				// calculate timestamp

				string MinString = UTIL_ToString( ( m_GameTicks / 1000 ) / 60 );
				string SecString = UTIL_ToString( ( m_GameTicks / 1000 ) % 60 );

				if( MinString.size( ) == 1 )
					MinString.insert( 0, "0" );

				if( SecString.size( ) == 1 )
					SecString.insert( 0, "0" );

				if( !ExtraFlags.empty( ) )
				{
					if (IsMuted(player->GetName()))
						Relay = false;
					
					uint32_t GameNr = GetGameNr();

					unsigned char SID = GetSIDFromPID( chatPlayer->GetFromPID() );
					unsigned char fteam;
					string All = "0";
					fteam = m_Slots[SID].GetTeam();
					if (ExtraFlags[0]==0)
						All="1";
						
					m_GHost->UDPChatSend("|gamechat "+UTIL_ToString(GameNr) + " "+UTIL_ToString(fteam)+" "+All+" "+player->GetName()+" "+chatPlayer->GetMessage());

					if( ExtraFlags[0] != 0 )
					CONSOLE_Print( "[GAME: " + m_GameName + "] (" + MinString + ":" + SecString + ") "+"[Team: "+UTIL_ToString(fteam+1)+"] [Allies] [" + player->GetName( ) + "]: " + chatPlayer->GetMessage( ) );

					string m = chatPlayer->GetMessage();
					transform( m.begin( ), m.end( ), m.begin( ), (int(*)(int))tolower );
					if (m_GetMapType == "dota")
					{
						bool blueplayer = false;
						if (m.substr(0,1) == "-" && m.length()>1)
						{
							unsigned char Nr=255;
							unsigned char Nrt;
							string sSlotNr;
							string sName=" ";
							CGamePlayer *p = NULL;
							for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
							{
								Nrt = GetSIDFromPID((*i)->GetPID());
								if (Nrt<Nr)
								{
									Nr = Nrt;
									sName = (*i)->GetName();
									p = (*i);
								}
							}
							sSlotNr = UTIL_ToString(Nr);
							if (p)
								if (sName == player->GetName())
								{
									blueplayer = true;
								}
						}
						if (m.substr(0,4) == "-wtf" && GetTime()<m_GameLoadedTime+15 && m_GHost->m_detectwtf)
						{
							if (blueplayer)
							{
								CONSOLE_Print( "[GAME: " + m_GameName + "] -wtf detected, disabling stats");
								m_DisableStats = true;
							}
						}
						if (m.substr(0,7) == "-switch" && m.size()==9)
							InitSwitching(chatPlayer->GetFromPID(), m.substr(8,1) );
//						if (m.substr(0,3) == "-no" || m.substr(0,14) == "-switch cancel")
						if (m=="-no" || m=="-switch cancel")
						if (m_SwitchTime!=0)
							SwitchDeny (chatPlayer->GetFromPID());								

//						if (m.substr(0,2)== "ok" || m.substr(0,3) == "-ok" || m.substr(0,14) == "-switch accept")
						if (m=="-ok" || m=="-switch accept")
						if (m_SwitchTime!=0)
							SwitchAccept(chatPlayer->GetFromPID());
						// disable gameoverteamdiff if switch is detected (until we handle switches)						
//						m_GameOverDiffCanceled = true;
					}

					if( ExtraFlags[0] == 0 )
					{
						// this is an ingame [All] message, print it to the console


						CONSOLE_Print( "[GAME: " + m_GameName + "] (" + MinString + ":" + SecString + ") "+"[Team: "+UTIL_ToString(fteam+1)+"] [All] [" + player->GetName( ) + "]: " + chatPlayer->GetMessage( ) );

						// don't relay ingame messages targeted for all players if we're currently muting all
						// note that commands will still be processed even when muting all because we only stop relaying the messages, the rest of the function is unaffected

						if( m_MuteAll )
						if(!isRootAdmin)
							Relay = false;
					}	

					if (m_Listen)
					{
//						unsigned char SID = GetSIDFromPID( chatPlayer->GetFromPID() );
						if( SID < m_Slots.size( ) )
						{
							unsigned char ListenUser;
							stringstream SS;
							unsigned char lteam;
							unsigned char siduser;
							SS<<m_ListenUser;

							while( !SS.eof( ) )							
							{
								SS >> ListenUser;
								siduser = GetSIDFromPID(ListenUser);
								if (siduser!=255)
								{
									lteam = m_Slots[GetSIDFromPID(ListenUser)].GetTeam();
									BYTEARRAY lu;
									lu.push_back(ListenUser);
									if (GetPlayerFromPID(ListenUser)!=NULL)
										if (ExtraFlags[0]!=0 && fteam != lteam)
											Send( lu, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( chatPlayer->GetFromPID( ), lu, chatPlayer->GetFlag( ), chatPlayer->GetExtraFlags( ), chatPlayer->GetMessage( ) ) );
//											SendChat(ListenUser, player->GetName( )+": "+chatPlayer->GetMessage( ));
								}
								if (m_ListenUser.length()==1)
									break;
							}

						}
					}

					if( Relay )
					{
						// add chat message to replay
						// this includes allied chat and private chat from both teams as long as it was relayed

						if( m_Replay )
							m_Replay->AddChatMessage( chatPlayer->GetFromPID( ), chatPlayer->GetFlag( ), UTIL_ByteArrayToUInt32( chatPlayer->GetExtraFlags( ), false ), chatPlayer->GetMessage( ) );
					}
				}
				else
				{
					// this is a lobby message, print it to the console
					player->PushMessage(chatPlayer->GetMessage( ) );
					
					if( player->IsSpammer() )
					{
						//count << "SPAMMER!!!!! " << player->GetName( ) << endl;
						OpenSlot( GetSIDFromPID( player->GetPID( ) ), true );
					}
					if (IsMuted(player->GetName()))
						Relay = false;


					if (m_GHost->m_AdminGame && !iscmd)
					{
						if (m_VirtualHostName=="|cFFC04040Admin")
							for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
							{
								(*i)->QueueChatCommand( player->GetName()+": " + chatPlayer->GetMessage( ) );
							}
					}

					CONSOLE_Print( "[GAME: "+ m_GameName + "] [Lobby] [" + player->GetName( ) + "]: "+ chatPlayer->GetMessage( ) );
					m_GHost->UDPChatSend("|lobby "+player->GetName()+" "+chatPlayer->GetMessage());
					if( m_MuteLobby )
						Relay = false;
				}
			}

			// handle bot commands

			string Message = chatPlayer->GetMessage( );

			if( Message == "?trigger" )
				SendChat( player, m_GHost->m_Language->CommandTrigger( string( 1, m_GHost->m_CommandTrigger ) ) );
			else if( !Message.empty( ) && Message[0] == m_GHost->m_CommandTrigger )
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

				// don't allow EventPlayerBotCommand to veto a previous instruction to set Relay to false
				// so if Relay is already false (e.g. because the player is muted) then it cannot be forced back to true here

				if( EventPlayerBotCommand( player, Command, Payload ) )
					Relay = false;
			}
				if( Relay)
				{
//					msg = m_GHost->CensorRemoveDots(msg);
					string msg2 = m_GHost->CensorMessage(msg);
					if (cRelay)
					{
						if (m_RootListen)
							Send( Silence(AddRootAdminPIDs(chatPlayer->GetToPIDs( ), player->GetPID())), m_Protocol->SEND_W3GS_CHAT_FROM_HOST( chatPlayer->GetFromPID( ), Silence(AddRootAdminPIDs(chatPlayer->GetToPIDs( ), player->GetPID())), chatPlayer->GetFlag( ), chatPlayer->GetExtraFlags( ), msg2 ) );
						else
							Send( Silence(chatPlayer->GetToPIDs( )), m_Protocol->SEND_W3GS_CHAT_FROM_HOST( chatPlayer->GetFromPID( ), Silence(chatPlayer->GetToPIDs( )), chatPlayer->GetFlag( ), chatPlayer->GetExtraFlags( ), msg2 ) );
					}
					else
					{
						BYTEARRAY b = Silence(GetRootAdminPIDs( player->GetPID() ));
						if (!b.empty())
							Send( b, m_Protocol->SEND_W3GS_CHAT_FROM_HOST( chatPlayer->GetFromPID( ), Silence(GetRootAdminPIDs(player->GetPID())), chatPlayer->GetFlag( ), chatPlayer->GetExtraFlags( ), msg2 ) );
					}
					if (m_GHost->m_autoinsultlobby)
					if (msg2!=msg && m_GHost->m_CensorMute && !m_GameLoaded && !m_GameLoading)
					{
						bool ok = true;
						if (isAdmin && !m_GHost->m_CensorMuteAdmins)
							ok = false;
						if (isRootAdmin)
							ok = false;
						if (ok && GetTime()-m_LastMars>=10)
						{
							string srv = m_Server;
							string nam1 = m_VirtualHostName;
							string nam2 = player->GetName();
							vector<string> randoms;
							bool safe2 = false;
							safe2 = IsSafe(nam2) || IsAdmin(nam2) || IsRootAdmin(nam2);
							if (safe2)
							{
								m_LastMars = GetTime();
								string msg = m_GHost->GetMars();

								randoms.push_back(m_GHost->m_RootAdmin);
								randoms.push_back(m_GHost->m_VirtualHostName);
								for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
								{
									if ((*i)->GetName()!=nam2 && (*i)->GetName()!=nam1)
										randoms.push_back((*i)->GetName());
								}
								random_shuffle(randoms.begin(), randoms.end());
								Replace( msg, "$VICTIM$", nam2 );
								Replace( msg, "$USER$", nam1 );
								Replace( msg, "$RANDOM$", randoms[0] );
								SendAllChat( msg );
							}
						}
					}
					if  (msg2!=msg && m_GHost->m_CensorMute && m_GameLoaded)
					{
						bool ok = true;
						if (isAdmin && !m_GHost->m_CensorMuteAdmins)
							ok = false;
						if (isRootAdmin)
							ok = false;
						if (ok)
							AddToCensorMuted(player->GetName());
					}
				}
		}
		else if( chatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_TEAMCHANGE && !m_CountDownStarted )
			EventPlayerChangeTeam( player, chatPlayer->GetByte( ) );
		else if( chatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_COLOURCHANGE && !m_CountDownStarted )
			EventPlayerChangeColour( player, chatPlayer->GetByte( ) );
		else if( chatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_RACECHANGE && !m_CountDownStarted )
			EventPlayerChangeRace( player, chatPlayer->GetByte( ) );
		else if( chatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_HANDICAPCHANGE && !m_CountDownStarted )
			EventPlayerChangeHandicap( player, chatPlayer->GetByte( ) );
	}
}

bool CBaseGame :: EventPlayerBotCommand( CGamePlayer *player, string command, string payload )
{
	// return true if the command itself should be hidden from other players

	return false;
}

void CBaseGame :: EventPlayerChangeTeam( CGamePlayer *player, unsigned char team )
{
	// player is requesting a team change

	if( m_SaveGame )
		return;

	if( m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
	{
		unsigned char oldSID = GetSIDFromPID( player->GetPID( ) );
		unsigned char newSID = GetEmptySlot( team, player->GetPID( ) );
		SwapSlots( oldSID, newSID );
	}
	else
	{
		if( team > 12 )
			return;

		if( team == 12 )
		{
			if( m_Map->GetMapObservers( ) != MAPOBS_ALLOWED && m_Map->GetMapObservers( ) != MAPOBS_REFEREES )
				return;
		}
		else
		{
			if( team >= m_Map->GetMapNumPlayers( ) )
				return;

			// make sure there aren't too many other players already

			unsigned char NumOtherPlayers = 0;

			for( unsigned char i = 0; i < m_Slots.size( ); i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[i].GetTeam( ) != 12 && m_Slots[i].GetPID( ) != player->GetPID( ) )
					NumOtherPlayers++;
			}

			if( NumOtherPlayers >= m_Map->GetMapNumPlayers( ) )
				return;
		}

		unsigned char SID = GetSIDFromPID( player->GetPID( ) );

		if( SID < m_Slots.size( ) )
		{
			m_Slots[SID].SetTeam( team );

			if( team == 12 )
			{
				// if they're joining the observer team give them the observer colour

				m_Slots[SID].SetColour( 12 );
			}
			else if( m_Slots[SID].GetColour( ) == 12 )
			{
				// if they're joining a regular team give them an unused colour

				m_Slots[SID].SetColour( GetNewColour( ) );
			}

			SendAllSlotInfo( );
		}
	}
}

void CBaseGame :: EventPlayerChangeColour( CGamePlayer *player, unsigned char colour )
{
	// player is requesting a colour change

	if( m_SaveGame )
		return;

	if( m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
		return;

	if( colour > 11 )
		return;

	unsigned char SID = GetSIDFromPID( player->GetPID( ) );

	if( SID < m_Slots.size( ) )
	{
		// make sure the player isn't an observer

		if( m_Slots[SID].GetTeam( ) == 12 )
			return;

		ColourSlot( SID, colour );
	}
}

void CBaseGame :: EventPlayerChangeRace( CGamePlayer *player, unsigned char race )
{
	// player is requesting a race change

	if( m_SaveGame )
		return;

	if( m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
		return;

	if( m_Map->GetMapFlags( ) & MAPFLAG_RANDOMRACES )
		return;

	if( race != SLOTRACE_HUMAN && race != SLOTRACE_ORC && race != SLOTRACE_NIGHTELF && race != SLOTRACE_UNDEAD && race != SLOTRACE_RANDOM )
		return;

	unsigned char SID = GetSIDFromPID( player->GetPID( ) );

	if( SID < m_Slots.size( ) )
	{
		m_Slots[SID].SetRace( race );
		SendAllSlotInfo( );
	}
}

void CBaseGame :: EventPlayerChangeHandicap( CGamePlayer *player, unsigned char handicap )
{
	// player is requesting a handicap change

	if( m_SaveGame )
		return;

	if( m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
		return;

	if( handicap != 50 && handicap != 60 && handicap != 70 && handicap != 80 && handicap != 90 && handicap != 100 )
		return;

	unsigned char SID = GetSIDFromPID( player->GetPID( ) );

	if( SID < m_Slots.size( ) )
	{
		m_Slots[SID].SetHandicap( handicap );
		SendAllSlotInfo( );
	}
}

void CBaseGame :: EventPlayerDropRequest( CGamePlayer *player )
{
	uint32_t TimePassed = GetTime() - m_LagScreenTime;
	uint32_t TimeRemaining = m_GHost->m_DropVoteTime - TimePassed;
	if (m_Lagging)
	if (TimePassed < m_GHost->m_DropVoteTime)
	{
		SendAllChat(UTIL_ToString(TimeRemaining)+" seconds grace period remaining");
		player->SetDropVote(false);
	}
	else
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + player->GetName( ) + "] voted to drop laggers" );
		SendAllChat( m_GHost->m_Language->PlayerVotedToDropLaggers( player->GetName( ) ) );

		// check if at least half the players voted to drop

		uint32_t Votes = 0;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( (*i)->GetDropVote( ) )
				Votes++;
		}

		if( (float)Votes / m_Players.size( ) > 0.49 )
			StopLaggers( m_GHost->m_Language->LaggedOutDroppedByVote( ) );
	}
}

void CBaseGame :: EventPlayerMapSize( CGamePlayer *player, CIncomingMapSize *mapSize )
{
	if( m_GameLoading || m_GameLoaded )
		return;

	// todotodo: the variable names here are confusing due to extremely poor design on my part

	uint32_t MapSize = UTIL_ByteArrayToUInt32( m_Map->GetMapSize( ), false );

	if( mapSize->GetSizeFlag( ) != 1 || mapSize->GetMapSize( ) != MapSize )
	{
		// the player doesn't have the map

		bool candownload = m_GHost->m_AdminsAndSafeCanDownload && (IsRootAdmin(player->GetName()) || IsAdmin(player->GetName()) || IsSafe(player->GetName()));

		if( m_GHost->m_AllowDownloads != 0 || candownload )
		{
			string *MapData = m_Map->GetMapData( );

			if( !MapData->empty( ) )
			{
				if( candownload || m_GHost->m_AllowDownloads == 1 || ( m_GHost->m_AllowDownloads == 2 && player->GetDownloadAllowed( ) ) )
				{
					if( !player->GetDownloadStarted( ) && mapSize->GetSizeFlag( ) == 1 )
					{
						// inform the client that we are willing to send the map

						CONSOLE_Print( "[GAME: " + m_GameName + "] map download started for player [" + player->GetName( ) + "]" );
						Send( player, m_Protocol->SEND_W3GS_STARTDOWNLOAD( GetHostPID( ) ) );
						player->SetDownloadStarted( true );
						player->SetStartedDownloadingTicks( GetTicks( ) );
					}
					else
					{
						// send up to 50 pieces of the map at once so that the download goes faster
						// if we wait for each MAPPART packet to be acknowledged by the client it'll take a long time to download
						// this is because we would have to wait the round trip time (the ping time) between sending every 1442 bytes of map data
						// doing it this way allows us to send at least 70 KB in each round trip interval which is much more reasonable
						// the theoretical throughput is [70 KB * 1000 / ping] in KB/sec so someone with 100 ping (round trip ping, not LC ping) could download at 700 KB/sec

						// how many players are currently downloading?

						uint32_t downloaders = CountDownloading();

						// share between max allowed downloaders
						if (m_GHost->m_maxdownloaders!=0)
							if (downloaders>m_GHost->m_maxdownloaders)
								downloaders = m_GHost->m_maxdownloaders;
						
						// calculate shared rate in KB/s
						float sharedRate = 1024*10;
						if (m_GHost->m_totaldownloadspeed>0)
							sharedRate = (float) m_GHost->m_totaldownloadspeed / (float) downloaders;
						
						float Seconds = (float)( GetTicks( ) - player->GetStartedDownloadingTicks( ) ) / 1000;
						double Rate = (float)player->GetLastMapPartSent( ) / 1024 / Seconds;
						uint32_t Percent = (uint32_t)((float)player->GetLastMapPartSent( )*100/(float)MapSize);
						if (Percent<0)
							Percent = 0;

						uint32_t ETA = 0;
						uint32_t TotalTime = 100;

						if (Percent!=0)
						{
							TotalTime = (uint32_t)(float)((100*Seconds/(float)Percent));
							ETA = TotalTime - (uint32_t)Seconds;
						}

						uint32_t toSend = 1;
						float setRate = (float)m_GHost->m_clientdownloadspeed;
						bool queued = false;
						uint32_t DownloadNr = DownloaderNr(player->GetName());
						if (setRate == 0)
							setRate = 2048;

						// if shared rate is lower than client's allowed rate, use shared rate
						if (setRate>sharedRate)
							setRate = sharedRate;

						// check if there are too many downloading, and set rate to 1 if true
						if (m_GHost->m_maxdownloaders!=0)
							if (DownloadNr>m_GHost->m_maxdownloaders)
								queued = true;
						if (queued)
								setRate = 1;

						// if the setrate (KB/s) is higher...send 50 pieces otherwise 1
						if (player->GetLastMapPartSent( )==0 || Rate <= setRate)
							toSend = 50;

						string sDownloadInfo= string();
						uint32_t iRate = (uint32_t)Rate;
						sDownloadInfo = UTIL_ToString(iRate)+" KB/s ("+UTIL_ToString(Percent)+"% ";
						if (!queued && ETA!=0)
							sDownloadInfo += "ETA: "+UTIL_ToString(ETA)+" s ";
						if (queued)
							sDownloadInfo += "queued";
						sDownloadInfo += ")";
						
						player->SetDownloadInfo(sDownloadInfo);

						if (DownloadNr>=downloaders)
							ShowDownloading();

						while( player->GetLastMapPartSent( ) < mapSize->GetMapSize( ) + 1442 * toSend && player->GetLastMapPartSent( ) < MapSize )
						{
							Send( player, m_Protocol->SEND_W3GS_MAPPART( GetHostPID( ), player->GetPID( ), player->GetLastMapPartSent( ), MapData ) );
							player->SetLastMapPartSent( player->GetLastMapPartSent( ) + 1442 );
						}

					}
				}
			}
			else
			{
				player->SetDeleteMe( true );
				player->SetLeftReason( "doesn't have the map and there is no local copy of the map to send" );
				player->SetLeftCode( PLAYERLEAVE_LOBBY );
				OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );
			}
		}
		else
		{
			player->SetDeleteMe( true );
			player->SetLeftReason( "doesn't have the map and map downloads are disabled" );
			player->SetLeftCode( PLAYERLEAVE_LOBBY );
			OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );
		}
	}
	else
	{
		if( player->GetDownloadStarted( ) )
		{
			// calculate download rate

			float Seconds = (float)( GetTicks( ) - player->GetStartedDownloadingTicks( ) ) / 1000;
			float Rate = (float)MapSize / 1024 / Seconds;
			CONSOLE_Print( "[GAME: " + m_GameName + "] map download finished for player [" + player->GetName( ) + "] in " + UTIL_ToString( Seconds, 1 ) + " seconds" );
			SendAllChat( m_GHost->m_Language->PlayerDownloadedTheMap( player->GetName( ), UTIL_ToString( Seconds, 1 ), UTIL_ToString( Rate, 1 ) ) );
			player->SetDownloadFinished( true );
			player->SetFinishedDownloadingTime( GetTime( ) );

			if (m_DownloadOnlyMode){
			SendChat (player->GetPID(),"This is a download only game, you should leave now");
			} else {
			SendChat (player->GetPID()," will get banned for 2 days IF you leave in 30s. dl xong, trong 30s ma out la bi Ban nick 2 ngay" );
			SendAllChat ("@" + player->GetName() + ": WARNING, leaver in 30s after downloaded gets banned for 2 days.  DL xong, trong 30s ma out la bi Ban nick 2 ngay" );
			}
			// add to database

			m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedDownloadAdd( m_Map->GetMapPath( ), MapSize, player->GetName( ), player->GetExternalIPString( ), player->GetSpoofed( ) ? 1 : 0, player->GetSpoofedRealm( ), GetTicks( ) - player->GetStartedDownloadingTicks( ) ) );
		} else
		if (m_DownloadOnlyMode)
		{
			player->SetDeleteMe( true );
			player->SetLeftReason( "does have the map and this is a download only game" );
			player->SetLeftCode( PLAYERLEAVE_LOBBY );
			OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );			
		}
	}

	unsigned char NewDownloadStatus = (unsigned char)( (float)mapSize->GetMapSize( ) / MapSize * 100 );
	unsigned char SID = GetSIDFromPID( player->GetPID( ) );

	if( NewDownloadStatus > 100 )
		NewDownloadStatus = 100;

	if( SID < m_Slots.size( ) )
	{
		// only send the slot info if the download status changed

		if( m_Slots[SID].GetDownloadStatus( ) != NewDownloadStatus )
		{
			m_Slots[SID].SetDownloadStatus( NewDownloadStatus );

			// we don't actually send the new slot info here
			// this is an optimization because it's possible for a player to download a map very quickly
			// if we send a new slot update for every percentage change in their download status it adds up to a lot of data
			// instead, we mark the slot info as "out of date" and update it only once in awhile (once per second when this comment was made)

			m_SlotInfoChanged = true;
		}
	}
}

void CBaseGame :: EventPlayerPongToHost( CGamePlayer *player, uint32_t pong )
{
	// autokick players with excessive pings but only if they're not reserved and we've received at least 3 pings from them
	// also don't kick anyone if the game is loading or loaded - this could happen because we send pings during loading but we stop sending them after the game is loaded
	// see the Update function for where we send pings

	if( !m_GameLoading && !m_GameLoaded && !player->GetDeleteMe( ) && !player->GetReserved( ) && player->GetNumPings( ) >= 3 && player->GetPing( m_GHost->m_LCPings ) > m_GHost->m_AutoKickPing )
	// ignore big pings for 6 seconds the last player who joined if all slots are occupied
	if(!(m_AllSlotsOccupied && player->GetPID()==m_LastPlayerJoined && GetTime()-m_LastPlayerJoinedTime>6))	
	{
		// send a chat message because we don't normally do so when a player leaves the lobby

		SendAllChat( m_GHost->m_Language->AutokickingPlayerForExcessivePing( player->GetName( ), UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) ) );
		player->SetDeleteMe( true );
		player->SetLeftReason( "was autokicked for excessive ping of " + UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) );
		player->SetLeftCode( PLAYERLEAVE_LOBBY );
		OpenSlot( GetSIDFromPID( player->GetPID( ) ), false );
	}
}

void CBaseGame :: EventGameStarted( )
{
	if (m_GHost->m_ShuffleSlotsOnStart)
		ShuffleSlots();
	CONSOLE_Print( "[GAME: " + m_GameName + "] started loading with " + UTIL_ToString( GetNumHumanPlayers( ) ) + " players" );

	// encode the HCL command string in the slot handicaps
	// here's how it works:
	//  the user inputs a command string to be sent to the map
	//  it is almost impossible to send a message from the bot to the map so we encode the command string in the slot handicaps
	//  this works because there are only 6 valid handicaps but Warcraft III allows the bot to set up to 256 handicaps
	//  we encode the original (unmodified) handicaps in the new handicaps and use the remaining space to store a short message
	//  only occupied slots deliver their handicaps to the map and we can send one character (from a list) per handicap
	//  when the map finishes loading, assuming it's designed to use the HCL system, it checks if anyone has an invalid handicap
	//  if so, it decodes the message from the handicaps and restores the original handicaps using the encoded values
	//  the meaning of the message is specific to each map and the bot doesn't need to understand it
	//  e.g. you could send game modes, # of rounds, level to start on, anything you want as long as it fits in the limited space available
	//  note: if you attempt to use the HCL system on a map that does not support HCL the bot will drastically modify the handicaps
	//  since the map won't automatically restore the original handicaps in this case your game will be ruined

	uint32_t GameNr = GetGameNr();
	bool HCL = false;

	if( !m_HCLCommandString.empty( ) )
	{
		if( m_HCLCommandString.size( ) <= GetSlotsOccupied( ) )
		{
			string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

			if( m_HCLCommandString.find_first_not_of( HCLChars ) == string :: npos )
			{
				unsigned char EncodingMap[256];
				unsigned char j = 0;

				for( uint32_t i = 0; i < 256; i++ )
				{
					// the following 7 handicap values are forbidden

					if( j == 0 || j == 50 || j == 60 || j == 70 || j == 80 || j == 90 || j == 100 )
						j++;

					EncodingMap[i] = j++;
				}

				unsigned char CurrentSlot = 0;

				for( string :: iterator si = m_HCLCommandString.begin( ); si != m_HCLCommandString.end( ); si++ )
				{
					while( m_Slots[CurrentSlot].GetSlotStatus( ) != SLOTSTATUS_OCCUPIED )
						CurrentSlot++;

					unsigned char HandicapIndex = ( m_Slots[CurrentSlot].GetHandicap( ) - 50 ) / 10;
					unsigned char CharIndex = HCLChars.find( *si );
					m_Slots[CurrentSlot++].SetHandicap( EncodingMap[HandicapIndex + CharIndex * 6] );
				}

				SendAllSlotInfo( );

				m_GHost->UDPChatSend("|hclsent "+UTIL_ToString(GameNr)+" " + m_GameName);
				HCL = true;
				m_HCL = true;

				CONSOLE_Print( "[GAME: " + m_GameName + "] successfully encoded HCL command string [" + m_HCLCommandString + "]" );
			}
			else
				CONSOLE_Print( "[GAME: " + m_GameName + "] encoding HCL command string [" + m_HCLCommandString + "] failed because it contains invalid characters" );
		}
		else
			CONSOLE_Print( "[GAME: " + m_GameName + "] encoding HCL command string [" + m_HCLCommandString + "] failed because there aren't enough occupied slots" );
	}
	if (!HCL)
		m_GHost->UDPChatSend("|hclnotsent "+UTIL_ToString(GameNr)+" " + m_GameName);

	m_StartedLoadingTime = GetTime( );

	// send a final slot info update if necessary
	// this typically won't happen because we prevent the !start command from completing while someone is downloading the map
	// however, if someone uses !start force while a player is downloading the map this could trigger
	// this is because we only permit slot info updates to be flagged when it's just a change in download status, all others are sent immediately
	// it might not be necessary but let's clean up the mess anyway

	if( m_SlotInfoChanged )
		SendAllSlotInfo( );

	m_StartedLoadingTicks = GetTicks( );
	m_LastLagScreenResetTime = GetTime( );
	m_GameLoading = true;

	// disable load in game feature if hcl has been sent and dota version is 6.63b
	if (m_Map->GetMapLocalPath().find("6.63b") != string::npos)
	if (HCL)
		m_LoadInGame = false;

	// disable load in game feature if on garena
	if (m_GarenaOnly)
		m_LoadInGame = false;

	m_GHost->UDPChatSend("|startgame "+UTIL_ToString(GameNr)+" " + m_GameName);

	// since we use a fake countdown to deal with leavers during countdown the COUNTDOWN_START and COUNTDOWN_END packets are sent in quick succession
	// send a start countdown packet

	if (!m_NormalCountdown)
	SendAll( m_Protocol->SEND_W3GS_COUNTDOWN_START( ) );

	// remove the virtual host player

	DeleteVirtualHost( );

	// send an end countdown packet

	SendAll( m_Protocol->SEND_W3GS_COUNTDOWN_END( ) );

	// send a game loaded packet for the fake player (if present)

	for( vector<unsigned char> :: iterator i = m_FakePlayers.begin( ); i != m_FakePlayers.end( ); ++i )
		SendAll( m_Protocol->SEND_W3GS_GAMELOADED_OTHERS( *i ) );

	// send a game loaded packet for the Waaagh!TV player (if present)

	if( m_WTVPlayerPID != 255 )
		SendAll( m_Protocol->SEND_W3GS_GAMELOADED_OTHERS( m_WTVPlayerPID ) );

	// record the starting number of players

	m_StartPlayers = GetNumHumanPlayers( );

	// close the listening socket

	delete m_Socket;
	m_Socket = NULL;

	// delete any potential players that are still hanging around

	for( vector<CPotentialPlayer *> :: iterator i = m_Potentials.begin( ); i != m_Potentials.end( ); i++ )
		delete *i;

	m_Potentials.clear( );

	// set initial values for replay

	if( m_Replay )
	{
		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			m_Replay->AddPlayer( (*i)->GetPID( ), (*i)->GetName( ) );

		m_Replay->SetSlots( m_Slots );
		m_Replay->SetRandomSeed( m_RandomSeed );
		m_Replay->SetSelectMode( m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM ? 3 : 0 );
		m_Replay->SetStartSpotCount( m_Map->GetMapNumPlayers( ) );

		/*
		
		BYTEARRAY MapGameType;

		if( m_SaveGame )
		{
			MapGameType.push_back( 0 );
			MapGameType.push_back( 2 );
			MapGameType.push_back( 0 );
			MapGameType.push_back( 0 );
		}
		else
		{
			MapGameType.push_back( m_Map->GetMapGameType( ) );
			MapGameType.push_back( 0 );
			MapGameType.push_back( 0 );
			MapGameType.push_back( 0 );
		}

		*/

		m_Replay->SetMapGameType( m_Map->GetMapGameType( ) );

		if( !m_Players.empty( ) )
		{
			// this might not be necessary since we're going to overwrite the replay's host PID and name everytime a player leaves

			m_Replay->SetHostPID( m_Players[0]->GetPID( ) );
			m_Replay->SetHostName( m_Players[0]->GetName( ) );
		}
	}

	// build a stat string for use when saving the replay
	// we have to build this now because the map data is going to be deleted

	BYTEARRAY StatString;
	UTIL_AppendByteArray( StatString, m_Map->GetMapGameFlags( ) );
	StatString.push_back( 0 );
	UTIL_AppendByteArray( StatString, m_Map->GetMapWidth( ) );
	UTIL_AppendByteArray( StatString, m_Map->GetMapHeight( ) );
	UTIL_AppendByteArray( StatString, m_Map->GetMapCRC( ) );
	UTIL_AppendByteArray( StatString, m_Map->GetMapPath( ) );
	UTIL_AppendByteArray( StatString, "GHost++" );
	StatString.push_back( 0 );
	UTIL_AppendByteArray( StatString, m_Map->GetMapSHA1( ) );		// note: in replays generated by Warcraft III it stores 20 zeros for the SHA1 instead of the real thing
	StatString = UTIL_EncodeStatString( StatString );
	m_StatString = string( StatString.begin( ), StatString.end( ) );

	// delete the map data

	delete m_Map;
	m_Map = NULL;

	if( m_LoadInGame )
	{
		// buffer all the player loaded messages
		// this ensures that every player receives the same set of player loaded messages in the same order, even if someone leaves during loading
		// if someone leaves during loading we buffer the leave message to ensure it gets sent in the correct position but the player loaded message wouldn't get sent if we didn't buffer it now

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
				(*j)->AddLoadInGameData( m_Protocol->SEND_W3GS_GAMELOADED_OTHERS( (*i)->GetPID( ) ) );
		}
	}

	// move the game to the games in progress vector

	m_GHost->m_CurrentGame = NULL;
	m_GHost->m_Games.push_back( this );

	// and finally reenter battle.net chat

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		(*i)->QueueGameUncreate( );
		(*i)->QueueEnterChat( );
	}
}

void CBaseGame :: EventGameLoaded( )
{
	CONSOLE_Print( "[GAME: " + m_GameName + "] finished loading with " + UTIL_ToString( GetNumHumanPlayers( ) ) + " players" );

	unsigned char Nr=255;
	unsigned char Nrt;
	string sSlotNr;
	string sGameNr;
	string sName=" ";
	CGamePlayer *p = NULL;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
			Nrt = GetSIDFromPID((*i)->GetPID());
			if (Nrt<Nr)
			{
				Nr = Nrt;
				sName = (*i)->GetName();
				p = (*i);
			}
	}
	sSlotNr = UTIL_ToString(Nr);
	uint32_t GameNr = GetGameNr();

	sGameNr = UTIL_ToString(GameNr);
	m_GHost->UDPChatSend("|loadedgame "+sGameNr+"|"+sSlotNr+"|"+sName+"|"+ m_GetMapType+"|"+m_GetMapPath+"|"+m_GameName);

	// remember how many players were at game start
	m_PlayersatStart = m_Players.size();

	// check if the teams have even number of players
	if (m_GetMapNumTeams > 1)
	{
		uint32_t sTeam1 = 0;
		uint32_t sTeam2 = 0;

		for( unsigned char s = 0; s < m_Slots.size( ); s++ )
		{
			if (m_Slots[s].GetTeam()==0)
				sTeam1++;
			if (m_Slots[s].GetTeam()==1)
				sTeam2++;
		}
		if (sTeam1==sTeam2)	
			m_EvenPlayeredTeams = true;
	}

	// update slots - needed for switching detection
	for( unsigned char s = 0; s < m_Slots.size( ); s++ )
	{
		CGamePlayer *pp = GetPlayerFromSID(s);
		if (pp)
			pp->SetSID(s);
	}
	// reserve players from last game

	if (m_GHost->m_HoldPlayersForRMK)
	{
		bool isadmin;
		for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
		{
			isadmin = false;
			for( vector<CBNET *> :: iterator k = m_GHost->m_BNETs.begin( ); k != m_GHost->m_BNETs.end( ); k++ )
			{
				if( (*k)->IsAdmin( (*j)->GetName( ) ) || (*k)->IsRootAdmin( (*j)->GetName( ) ) )
				{
					isadmin = true;
					break;
				}
			}			
			if (!isadmin)
			{
				if (m_GHost->m_PlayersfromRMK.length()==0)
					m_GHost->m_PlayersfromRMK +=" ";
				m_GHost->m_PlayersfromRMK +=(*j)->GetName();
			}
		}
		CONSOLE_Print( "[GHOST] reserving players from [" + GetGameName( ) + "] for next game" );
	}

	m_GameLoadedTime = GetTime();

	// if HCL was sent message first player not to set the map mode
	if (!m_LoadInGame && p!=NULL && !m_HCLCommandString.empty())
	{
		SendChat(p->GetPID(), "-" + m_HCLCommandString + " auto set, no need to type it in " + p->GetName() );
	}

	// if autohosted and no HCL, message first player to set the map mode
	if (m_autohosted && p!=NULL && m_HCLCommandString.empty())
	{
		string Modes = p->GetName()+ "!! You're on slot 1. Enter any map modes now!";
		string Modes2 = string();

		if (m_GameName.find("-")!= string::npos)
		{
			uint32_t j = 0;
			uint32_t k = 0;
			uint32_t i = 0;
			string mode = string();
			Modes = p->GetName()+"!!! Type";
			while (j<m_GameName.length()-1 && (m_GameName.find("-",j)!= string::npos))
			{
				k = m_GameName.find("-",j);
				i = m_GameName.find(" ",k);
				if (i==0)
					i = m_GameName.length() - k + 1;
				else
					i = i - k;
				mode = m_GameName.substr(k, i);
				Modes += " "+mode;
				if (Modes2.length()>0)
					Modes2+=" ";
				Modes2+= mode;
				j = k + i;
			}
			Modes += " now!";
		}

		SendChat(p, Modes);
		SendChat(p, Modes);
		SendChat(p, Modes);
		
		if (Modes2.length()>0)
			CONSOLE_Print( "[GAME: " + m_GameName + "] " + p->GetName() + " should type " + Modes2 );
	}

	// send shortest, longest, and personal load times to each player

	CGamePlayer *Shortest = NULL;
	CGamePlayer *Longest = NULL;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !Shortest || (*i)->GetFinishedLoadingTicks( ) < Shortest->GetFinishedLoadingTicks( ) )
			Shortest = *i;

		if( !Longest || (*i)->GetFinishedLoadingTicks( ) > Longest->GetFinishedLoadingTicks( ) )
			Longest = *i;
	}

	if( Shortest && Longest )
	{
		SendAllChat( m_GHost->m_Language->ShortestLoadByPlayer( Shortest->GetName( ), UTIL_ToString( (float)( Shortest->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks ) / 1000, 2 ) ) );
		SendAllChat( m_GHost->m_Language->LongestLoadByPlayer( Longest->GetName( ), UTIL_ToString( (float)( Longest->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks ) / 1000, 2 ) ) );
	}

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		SendChat( *i, m_GHost->m_Language->YourLoadingTimeWas( UTIL_ToString( (float)( (*i)->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks ) / 1000, 2 ) ) );
}

unsigned char CBaseGame :: GetSIDFromPID( unsigned char PID )
{
	if( m_Slots.size( ) > 255 )
		return 255;

	for( unsigned char i = 0; i < m_Slots.size( ); i++ )
	{
		if( m_Slots[i].GetPID( ) == PID )
			return i;
	}

	return 255;
}

CGamePlayer *CBaseGame :: GetPlayerFromPID( unsigned char PID )
{
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
/*
		if (!m_GameLoaded)
		if ((*i)->GetLeftReason( )!="")
			break;
*/
		if( !(*i)->GetLeftMessageSent( ) && (*i)->GetPID( ) == PID )
			return *i;
	}

	return NULL;
}


CGamePlayer *CBaseGame :: GetPlayerFromSID( unsigned char SID )
{
	if( SID < m_Slots.size( ) )
		return GetPlayerFromPID( m_Slots[SID].GetPID( ) );

	return NULL;
}

CGamePlayer *CBaseGame :: GetPlayerFromName( string name, bool sensitive )
{
	if( !sensitive )
		transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
		{
			string TestName = (*i)->GetName( );

			if( !sensitive )
				transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

			if( TestName == name )
				return *i;
		}
	}

	return NULL;
}

uint32_t CBaseGame :: GetPlayerFromNamePartial( string name, CGamePlayer **player )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t Matches = 0;
	*player = NULL;

	// try to match each player with the passed string (e.g. "Gen" would be matched with "lock")

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
		{
			string TestName = (*i)->GetName( );
			transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

			if( TestName.find( name ) != string :: npos )
			{
				Matches++;
				*player = *i;

				// if the name matches exactly stop any further matching

				if( TestName == name )
				{
					Matches = 1;
					break;
				}
			}
		}
	}

	return Matches;
}

CGamePlayer *CBaseGame :: GetPlayerFromColour( unsigned char colour )
{
	for( unsigned char i = 0; i < m_Slots.size( ); i++ )
	{
		if( m_Slots[i].GetColour( ) == colour )
			return GetPlayerFromSID( i );
	}

	return NULL;
}

unsigned char CBaseGame :: GetNewPID( )
{
	// find an unused PID for a new player to use

	for( unsigned char TestPID = 1; TestPID < 255; TestPID++ )
	{
		if( TestPID == m_VirtualHostPID || TestPID == m_WTVPlayerPID )
			continue;

		bool InUse = false;
		
		for( vector<unsigned char> :: iterator i = m_FakePlayers.begin( ); i != m_FakePlayers.end( ); ++i )
		{
			if( *i == TestPID )
			{
				InUse = true;
				break;
			}
		}

		if( InUse )
			continue;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetLeftMessageSent( ) && (*i)->GetPID( ) == TestPID )
			{
				InUse = true;
				break;
			}
		}

		if( !InUse )
			return TestPID;
	}

	// this should never happen

	return 255;
}

unsigned char CBaseGame :: GetNewColour( )
{
	// find an unused colour for a player to use

	for( unsigned char TestColour = 0; TestColour < 12; TestColour++ )
	{
		bool InUse = false;

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetColour( ) == TestColour )
			{
				InUse = true;
				break;
			}
		}

		if( !InUse )
			return TestColour;
	}

	// this should never happen

	return 12;
}

BYTEARRAY CBaseGame :: GetAllyPIDs( unsigned char PID)
{
	BYTEARRAY result;

	unsigned char team = m_Slots[GetSIDFromPID(PID)].GetTeam();

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
		if (GetTeam(GetSIDFromPID((*i)->GetPID()))==team)
			result.push_back( (*i)->GetPID( ) );
	}

	return result;
}

BYTEARRAY CBaseGame :: GetAdminPIDs()
{
	BYTEARRAY result;
	
	bool isadmin = false;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		isadmin = IsOwner((*i)->GetName( )) || IsAdmin((*i)->GetName( )) || IsRootAdmin((*i)->GetName());
		if( !(*i)->GetLeftMessageSent( ) && isadmin )
			result.push_back( (*i)->GetPID( ) );
	}

	return result;
}

BYTEARRAY CBaseGame :: Silence( BYTEARRAY PIDs)
{
	BYTEARRAY result;

	for(uint32_t j = 0; j<PIDs.size(); j++)
	{
		CGamePlayer *p = GetPlayerFromPID(PIDs[j]);
		if (p)
		if (!p->GetSilence())
			result.push_back(PIDs[j]);
		if (!p)
			result.push_back(PIDs[j]);
	}
	return result;
}

BYTEARRAY CBaseGame :: AddRootAdminPIDs( BYTEARRAY PIDs, unsigned char omitpid)
{
	BYTEARRAY result;
	unsigned char root;
	bool isadmin = false;
	for(uint32_t j = 0; j<PIDs.size(); j++)
	{
		result.push_back(PIDs[j]);
	}
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		//		isadmin = IsOwner((*i)->GetName( )) || IsRootAdmin((*i)->GetName());
		isadmin = IsRootAdmin((*i)->GetName());
		root = (*i)->GetPID( );
		if( !(*i)->GetLeftMessageSent( ) && isadmin &&  (*i)->GetPID( )!= omitpid)
		{
			bool isAlready = false;

			for(uint32_t j = 0; j<PIDs.size(); j++)
			{
				if (root == PIDs[j])
					isAlready = true;
			}
			if (!isAlready)
				result.push_back(root);
		}
	}

	return result;
}

BYTEARRAY CBaseGame :: GetRootAdminPIDs( unsigned char omitpid)
{
	BYTEARRAY result;

	bool isadmin = false;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		//		isadmin = IsOwner((*i)->GetName( )) || IsRootAdmin((*i)->GetName());
		isadmin = IsRootAdmin((*i)->GetName());
		if( !(*i)->GetLeftMessageSent( ) && isadmin &&  (*i)->GetPID( )!= omitpid)
			result.push_back( (*i)->GetPID( ) );
	}
	return result;
}

BYTEARRAY CBaseGame :: GetRootAdminPIDs()
{
	BYTEARRAY result;

	bool isadmin = false;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
//		isadmin = IsOwner((*i)->GetName( )) || IsRootAdmin((*i)->GetName());
		isadmin = IsRootAdmin((*i)->GetName());
		if( !(*i)->GetLeftMessageSent( ) && isadmin )
			result.push_back( (*i)->GetPID( ) );
	}

	return result;
}

BYTEARRAY CBaseGame :: GetPIDs( )
{
	BYTEARRAY result;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
			result.push_back( (*i)->GetPID( ) );
	}

	return result;
}

BYTEARRAY CBaseGame :: GetPIDs( unsigned char excludePID )
{
	BYTEARRAY result;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) && (*i)->GetPID( ) != excludePID )
			result.push_back( (*i)->GetPID( ) );
	}

	return result;
}

unsigned char CBaseGame :: GetHostPID( )
{
	// return the player to be considered the host (it can be any player) - mainly used for sending text messages from the bot
	// try to find the virtual host player first

	if( m_VirtualHostPID != 255 )
		return m_VirtualHostPID;

	// try to find the fakeplayer next

	if( m_FakePlayers.size( ) )
		return m_FakePlayers[0];

	string OwnerLower = m_OwnerName;
	transform( OwnerLower.begin( ), OwnerLower.end( ), OwnerLower.begin( ), (int(*)(int))tolower );

	// try to find the owner player next

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		string name = (*i)->GetName();
		transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
		if( !(*i)->GetLeftMessageSent( ) && name == OwnerLower )
			return (*i)->GetPID( );
	}

	// okay then, just use the first available player

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
			return (*i)->GetPID( );
	}

	return 255;
}

unsigned char CBaseGame :: GetEmptySlot( bool reserved )
{
	if( m_Slots.size( ) > 255 )
		return 255;

	if( m_SaveGame )
	{
		// unfortunately we don't know which slot each player was assigned in the savegame
		// but we do know which slots were occupied and which weren't so let's at least force players to use previously occupied slots

		vector<CGameSlot> SaveGameSlots = m_SaveGame->GetSlots( );

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN && SaveGameSlots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && SaveGameSlots[i].GetComputer( ) == 0 )
				return i;
		}

		// don't bother with reserved slots in savegames
	}
	else
	{
		// look for an empty slot for a new player to occupy
		// if reserved is true then we're willing to use closed or occupied slots as long as it wouldn't displace a player with a reserved slot

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN )
				return i;
		}

		if( reserved )
		{
			// no empty slots, but since player is reserved give them a closed slot

			for( unsigned char i = 0; i < m_Slots.size( ); i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_CLOSED )
					return i;
			}

			// no closed slots either, give them an occupied slot but not one occupied by another reserved player
			// first look for a player who is downloading the map and has the least amount downloaded so far

			unsigned char LeastDownloaded = 100;
			unsigned char LeastSID = 255;

			for( unsigned char i = 0; i < m_Slots.size( ); i++ )
			{
				CGamePlayer *Player = GetPlayerFromSID( i );

				if( Player && !Player->GetReserved( ) && m_Slots[i].GetDownloadStatus( ) < LeastDownloaded )
				{
					LeastDownloaded = m_Slots[i].GetDownloadStatus( );
					LeastSID = i;
				}
			}

			if( LeastSID != 255 )
				return LeastSID;

			// nobody who isn't reserved is downloading the map, just choose the first player who isn't reserved

			for( unsigned char i = m_Slots.size( )-1; i > 0; i-- )
			{
				CGamePlayer *Player = GetPlayerFromSID( i );

				if( Player && !Player->GetReserved( ) )
					return i;
			}
		}
	}

	return 255;
}

unsigned char CBaseGame :: GetEmptySlotAdmin( bool reserved )
{
	if( m_Slots.size( ) > 255 )
		return 255;

	if( m_SaveGame )
	{
		// unfortunately we don't know which slot each player was assigned in the savegame
		// but we do know which slots were occupied and which weren't so let's at least force players to use previously occupied slots

		vector<CGameSlot> SaveGameSlots = m_SaveGame->GetSlots( );

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN && SaveGameSlots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && SaveGameSlots[i].GetComputer( ) == 0 )
				return i;
		}

		// don't bother with reserved slots in savegames
	}
	else
	{
		// look for an empty slot for a new player to occupy
		// if reserved is true then we're willing to use closed or occupied slots as long as it wouldn't displace a player with a reserved slot

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN )
				return i;
		}

		if( reserved )
		{
			// no empty slots, but since player is reserved give them a closed slot

			for( unsigned char i = 0; i < m_Slots.size( ); i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_CLOSED )
					return i;
			}

			// no closed slots either, give them an occupied slot but not one occupied by another reserved player
			// first look for a player who is downloading the map and has the least amount downloaded so far

			unsigned char LeastDownloaded = 100;
			unsigned char LeastSID = 255;

			for( unsigned char i = 0; i < m_Slots.size( ); i++ )
			{
				CGamePlayer *Player = GetPlayerFromSID( i );

				if( Player && !Player->GetReserved( ) && m_Slots[i].GetDownloadStatus( ) < LeastDownloaded )
				{
					LeastDownloaded = m_Slots[i].GetDownloadStatus( );
					LeastSID = i;
				}
			}

			if( LeastSID != 255 )
				return LeastSID;

			// nobody who isn't reserved is downloading the map, just choose the first player who isn't an admin

			for( unsigned char i = m_Slots.size( )-1; i > 0; i-- )
			{
				CGamePlayer *Player = GetPlayerFromSID( i );
				bool isadmin = false;
				if (Player)
				{
					isadmin = IsOwner(Player->GetName( )) || IsAdmin(Player->GetName( )) || IsRootAdmin(Player->GetName());
					if (m_GHost->m_SafeLobbyImmunity && IsSafe(Player->GetName()))
						isadmin = true;						
				}

				if( Player && !isadmin )
					return i;
			}
		}
	}

	return 255;
}

unsigned char CBaseGame :: GetEmptySlot( unsigned char team, unsigned char PID )
{
	if( m_Slots.size( ) > 255 )
		return 255;

	// find an empty slot based on player's current slot

	unsigned char StartSlot = GetSIDFromPID( PID );

	if( StartSlot < m_Slots.size( ) )
	{
		if( m_Slots[StartSlot].GetTeam( ) != team )
		{
			// player is trying to move to another team so start looking from the first slot on that team
			// we actually just start looking from the very first slot since the next few loops will check the team for us

			StartSlot = 0;
		}

		if( m_SaveGame )
		{
			vector<CGameSlot> SaveGameSlots = m_SaveGame->GetSlots( );

			for( unsigned char i = StartSlot; i < m_Slots.size( ); i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN && m_Slots[i].GetTeam( ) == team && SaveGameSlots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && SaveGameSlots[i].GetComputer( ) == 0 )
					return i;
			}

			for( unsigned char i = 0; i < StartSlot; i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN && m_Slots[i].GetTeam( ) == team && SaveGameSlots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && SaveGameSlots[i].GetComputer( ) == 0 )
					return i;
			}
		}
		else
		{
			// find an empty slot on the correct team starting from StartSlot

			for( unsigned char i = StartSlot; i < m_Slots.size( ); i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN && m_Slots[i].GetTeam( ) == team )
					return i;
			}

			// didn't find an empty slot, but we could have missed one with SID < StartSlot
			// e.g. in the DotA case where I am in slot 4 (yellow), slot 5 (orange) is occupied, and slot 1 (blue) is open and I am trying to move to another slot

			for( unsigned char i = 0; i < StartSlot; i++ )
			{
				if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN && m_Slots[i].GetTeam( ) == team )
					return i;
			}
		}
	}

	return 255;
}

void CBaseGame :: SwapSlots( unsigned char SID1, unsigned char SID2 )
{
	if( SID1 < m_Slots.size( ) && SID2 < m_Slots.size( ) && SID1 != SID2 )
	{
		CGameSlot Slot1 = m_Slots[SID1];
		CGameSlot Slot2 = m_Slots[SID2];

// update slots - needed for switching detection
		CGamePlayer *p1 = GetPlayerFromSID(SID1);
		CGamePlayer *p2 = GetPlayerFromSID(SID2);

		if (p1)
			p1->SetSID(SID2);

		if (p2)
			p2->SetSID(SID1);

		if (!m_GameLoaded && !m_GameLoading)
			m_GHost->UDPChatSend("|swap "+UTIL_ToString(SID1)+" "+UTIL_ToString(SID2));
		else
		{
			uint32_t GameNr = GetGameNr();

			m_GHost->UDPChatSend("|swapg "+UTIL_ToString(GameNr)+" "+UTIL_ToString(SID1)+" "+UTIL_ToString(SID2));
		}

		if( m_Map->GetMapGameType( ) != GAMETYPE_CUSTOM )
		{
			// regular game - swap everything

			m_Slots[SID1] = Slot2;
			m_Slots[SID2] = Slot1;
		}
		else
		{
			// custom game - don't swap the team, colour, or race

			m_Slots[SID1] = CGameSlot( Slot2.GetPID( ), Slot2.GetDownloadStatus( ), Slot2.GetSlotStatus( ), Slot2.GetComputer( ), Slot1.GetTeam( ), Slot1.GetColour( ), Slot1.GetRace( ) );
			m_Slots[SID2] = CGameSlot( Slot1.GetPID( ), Slot1.GetDownloadStatus( ), Slot1.GetSlotStatus( ), Slot1.GetComputer( ), Slot2.GetTeam( ), Slot2.GetColour( ), Slot2.GetRace( ) );
		}
		SendAllSlotInfo( );
		m_GHost->UDPChatSend("|lobbyupdate");		
	}
}

void CBaseGame :: SwapSlotsS( unsigned char SID1, unsigned char SID2 )
{
	if( SID1 < m_Slots.size( ) && SID2 < m_Slots.size( ) && SID1 != SID2 )
	{
		CGameSlot Slot1 = m_Slots[SID1];
		CGameSlot Slot2 = m_Slots[SID2];

		m_GHost->UDPChatSend("|swap "+UTIL_ToString(SID1)+" "+UTIL_ToString(SID2));

		if( m_GetMapGameType != GAMETYPE_CUSTOM )
		{
			// regular game - swap everything

			m_Slots[SID1] = Slot2;
			m_Slots[SID2] = Slot1;
		}
		else
		{
			// custom game - don't swap the team, colour, or race

			m_Slots[SID1] = CGameSlot( Slot2.GetPID( ), Slot2.GetDownloadStatus( ), Slot2.GetSlotStatus( ), Slot2.GetComputer( ), Slot1.GetTeam( ), Slot1.GetColour( ), Slot1.GetRace( ), Slot2.GetComputerType( ), Slot2.GetHandicap( ) );
			m_Slots[SID2] = CGameSlot( Slot1.GetPID( ), Slot1.GetDownloadStatus( ), Slot1.GetSlotStatus( ), Slot1.GetComputer( ), Slot2.GetTeam( ), Slot2.GetColour( ), Slot2.GetRace( ), Slot1.GetComputerType( ), Slot1.GetHandicap( ) );
		}

//		SendAllSlotInfo( );
	}
}

void CBaseGame :: OpenSlot( unsigned char SID, bool kick )
{
	if( SID < m_Slots.size( ) )
	{
		if( kick )
		{
			CGamePlayer *Player = GetPlayerFromSID( SID );

			if( Player )
			{
				Player->SetDeleteMe( true );
				Player->SetLeftReason( "was kicked when opening a slot" );
				Player->SetLeftCode( PLAYERLEAVE_LOBBY );
			}
		}

		CGameSlot Slot = m_Slots[SID];
		m_Slots[SID] = CGameSlot( 0, 255, SLOTSTATUS_OPEN, 0, Slot.GetTeam( ), Slot.GetColour( ), Slot.GetRace( ) ); 
		SendAllSlotInfo( );

		m_GHost->UDPChatSend("|lobbyupdate");		
	}
}

void CBaseGame :: CloseSlot( unsigned char SID, bool kick )
{
	if( SID < m_Slots.size( ) )
	{
		if( kick )
		{
			CGamePlayer *Player = GetPlayerFromSID( SID );

			if( Player )
			{
				Player->SetDeleteMe( true );
				Player->SetLeftReason( "was kicked when closing a slot" );
				Player->SetLeftCode( PLAYERLEAVE_LOBBY );
			}
		}

		CGameSlot Slot = m_Slots[SID];
		m_Slots[SID] = CGameSlot( 0, 255, SLOTSTATUS_CLOSED, 0, Slot.GetTeam( ), Slot.GetColour( ), Slot.GetRace( ) ); 
		SendAllSlotInfo( );

		m_GHost->UDPChatSend("|lobbyupdate");		
	}
}

void CBaseGame :: ComputerSlot( unsigned char SID, unsigned char skill, bool kick )
{
	if( SID < m_Slots.size( ) && skill < 3 )
	{
		if( kick )
		{
			CGamePlayer *Player = GetPlayerFromSID( SID );

			if( Player )
			{
				Player->SetDeleteMe( true );
				Player->SetLeftReason( "was kicked when creating a computer in a slot" );
				Player->SetLeftCode( PLAYERLEAVE_LOBBY );
			}
		}

		CGameSlot Slot = m_Slots[SID];
		m_Slots[SID] = CGameSlot( 0, 100, SLOTSTATUS_OCCUPIED, 1, Slot.GetTeam( ), Slot.GetColour( ), Slot.GetRace( ), skill );
		SendAllSlotInfo( );

		m_GHost->UDPChatSend("|lobbyupdate");		
	}
}

void CBaseGame :: ColourSlot( unsigned char SID, unsigned char colour )
{
	if( SID < m_Slots.size( ) && colour < 12 )
	{
		// make sure the requested colour isn't already taken

		bool Taken = false;
		unsigned char TakenSID = 0;

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetColour( ) == colour )
			{
				TakenSID = i;
				Taken = true;
			}
		}

		if( Taken && m_Slots[TakenSID].GetSlotStatus( ) != SLOTSTATUS_OCCUPIED )
		{
			// the requested colour is currently "taken" by an unused (open or closed) slot
			// but we allow the colour to persist within a slot so if we only update the existing player's colour the unused slot will have the same colour
			// this isn't really a problem except that if someone then joins the game they'll receive the unused slot's colour resulting in a duplicate
			// one way to solve this (which we do here) is to swap the player's current colour into the unused slot

			m_Slots[TakenSID].SetColour( m_Slots[SID].GetColour( ) );
			m_Slots[SID].SetColour( colour );
			SendAllSlotInfo( );
		}
		else if( !Taken )
		{
			// the requested colour isn't used by ANY slot

			m_Slots[SID].SetColour( colour );
			SendAllSlotInfo( );
		}
	}
}

void CBaseGame :: OpenAllSlots( )
{
	bool Changed = false;

	for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
	{
		if( (*i).GetSlotStatus( ) == SLOTSTATUS_CLOSED )
		{
			(*i).SetSlotStatus( SLOTSTATUS_OPEN );
			Changed = true;
		}
	}

	if( Changed )
		SendAllSlotInfo( );
	m_GHost->UDPChatSend("|lobbyupdate");		
}

void CBaseGame :: CloseAllSlots( )
{
	bool Changed = false;

	for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
	{
		if( (*i).GetSlotStatus( ) == SLOTSTATUS_OPEN )
		{
			(*i).SetSlotStatus( SLOTSTATUS_CLOSED );
			Changed = true;
		}
	}

	if( Changed )
		SendAllSlotInfo( );
	m_GHost->UDPChatSend("|lobbyupdate");		
}

void CBaseGame :: ShuffleSlots( )
{
	// we only want to shuffle the player slots
	// that means we need to prevent this function from shuffling the open/closed/computer slots too
	// so we start by copying the player slots to a temporary vector

	vector<CGameSlot> PlayerSlots;

	for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
	{
		if( (*i).GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && (*i).GetComputer( ) == 0 && (*i).GetColour()!=12 )
			PlayerSlots.push_back( *i );
	}

	// now we shuffle PlayerSlots

	if( m_Map->GetMapGameType( ) == GAMETYPE_CUSTOM )
	{
		// custom game
		// rather than rolling our own probably broken shuffle algorithm we use random_shuffle because it's guaranteed to do it properly
		// so in order to let random_shuffle do all the work we need a vector to operate on
		// unfortunately we can't just use PlayerSlots because the team/colour/race shouldn't be modified
		// so make a vector we can use

		vector<unsigned char> SIDs;

		for( unsigned char i = 0; i < PlayerSlots.size( ); i++ )
			SIDs.push_back( i );

		random_shuffle( SIDs.begin( ), SIDs.end( ) );

		// now put the PlayerSlots vector in the same order as the SIDs vector

		vector<CGameSlot> Slots;

		// as usual don't modify the team/colour/race

		for( unsigned char i = 0; i < SIDs.size( ); i++ )
			Slots.push_back( CGameSlot( PlayerSlots[SIDs[i]].GetPID( ), PlayerSlots[SIDs[i]].GetDownloadStatus( ), PlayerSlots[SIDs[i]].GetSlotStatus( ), PlayerSlots[SIDs[i]].GetComputer( ), PlayerSlots[i].GetTeam( ), PlayerSlots[i].GetColour( ), PlayerSlots[i].GetRace( ) ) );

		PlayerSlots = Slots;
	}
	else
	{
		// regular game
		// it's easy when we're allowed to swap the team/colour/race!

		random_shuffle( PlayerSlots.begin( ), PlayerSlots.end( ) );
	}

	// now we put m_Slots back together again

	vector<CGameSlot> :: iterator CurrentPlayer = PlayerSlots.begin( );
	vector<CGameSlot> Slots;

	for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
	{
		if( (*i).GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && (*i).GetComputer( ) == 0 && (*i).GetColour() != 12 )
		{
			Slots.push_back( *CurrentPlayer );
			CurrentPlayer++;
		}
		else
			Slots.push_back( *i );
	}

	m_Slots = Slots;

	// and finally tell everyone about the new slot configuration

	SendAllSlotInfo( );

	// update slots - needed for switching detection
	for( unsigned char s = 0; s < m_Slots.size( ); s++ )
	{
		CGamePlayer *p = GetPlayerFromSID(s);
		if (p)
			p->SetSID(s);
	}

	m_GHost->UDPChatSend("|lobbyupdate");		
}

vector<unsigned char> CBaseGame :: BalanceSlotsRecursive( vector<unsigned char> PlayerIDs, unsigned char *TeamSizes, double *PlayerScores, unsigned char StartTeam )
{
	// take a brute force approach to finding the best balance by iterating through every possible combination of players
	// 1.) since the number of teams is arbitrary this algorithm must be recursive
	// 2.) on the first recursion step every possible combination of players into two "teams" is checked, where the first team is the correct size and the second team contains everyone else
	// 3.) on the next recursion step every possible combination of the remaining players into two more "teams" is checked, continuing until all the actual teams are accounted for
	// 4.) for every possible combination, check the largest difference in total scores between any two actual teams
	// 5.) minimize this value by choosing the combination of players with the smallest difference

	vector<unsigned char> BestOrdering = PlayerIDs;
	double BestDifference = -1.0;

	for( unsigned char i = StartTeam; i < 12; i++ )
	{
		if( TeamSizes[i] > 0 )
		{
			unsigned char Mid = TeamSizes[i];

			// the base case where only one actual team worth of players was passed to this function is handled by the behaviour of next_combination
			// in this case PlayerIDs.begin( ) + Mid will actually be equal to PlayerIDs.end( ) and next_combination will return false

			while( next_combination( PlayerIDs.begin( ), PlayerIDs.begin( ) + Mid, PlayerIDs.end( ) ) )
			{
				// we're splitting the players into every possible combination of two "teams" based on the midpoint Mid
				// the first (left) team contains the correct number of players but the second (right) "team" might or might not
				// for example, it could contain one, two, or more actual teams worth of players
				// so recurse using the second "team" as the full set of players to perform the balancing on

				vector<unsigned char> BestSubOrdering = BalanceSlotsRecursive( vector<unsigned char>( PlayerIDs.begin( ) + Mid, PlayerIDs.end( ) ), TeamSizes, PlayerScores, i + 1 );

				// BestSubOrdering now contains the best ordering of all the remaining players (the "right team") given this particular combination of players into two "teams"
				// in order to calculate the largest difference in total scores we need to recombine the subordering with the first team

				vector<unsigned char> TestOrdering = vector<unsigned char>( PlayerIDs.begin( ), PlayerIDs.begin( ) + Mid );
				TestOrdering.insert( TestOrdering.end( ), BestSubOrdering.begin( ), BestSubOrdering.end( ) );

				// now calculate the team scores for all the teams that we know about (e.g. on subsequent recursion steps this will NOT be every possible team)

				vector<unsigned char> :: iterator CurrentPID = TestOrdering.begin( );
				double TeamScores[12];

				for( unsigned char j = StartTeam; j < 12; j++ )
				{
					TeamScores[j] = 0.0;

					for( unsigned char k = 0; k < TeamSizes[j]; k++ )
					{
						TeamScores[j] += PlayerScores[*CurrentPID];
						CurrentPID++;
					}
				}

				// find the largest difference in total scores between any two teams

				double LargestDifference = 0.0;

				for( unsigned char j = StartTeam; j < 12; j++ )
				{
					if( TeamSizes[j] > 0 )
					{
						for( unsigned char k = j + 1; k < 12; k++ )
						{
							if( TeamSizes[k] > 0 )
							{
								double Difference = abs( TeamScores[j] - TeamScores[k] );

								if( Difference > LargestDifference )
									LargestDifference = Difference;

							}
						}
					}
				}

				// and minimize it

				if( BestDifference < 0.0 || LargestDifference < BestDifference )
				{
					BestOrdering = TestOrdering;
					BestDifference = LargestDifference;
				}
			}
		}
	}

	return BestOrdering;
}

void CBaseGame :: BalanceSlots( )
{
	if( m_Map->GetMapGameType( ) != GAMETYPE_CUSTOM )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] error balancing slots - can't balance slots in non custom games" );
		return;
	}

	// setup the necessary variables for the balancing algorithm
	// use an array of 13 elements for 12 players because GHost++ allocates PID's from 1-12 (i.e. excluding 0) and we use the PID to index the array

	vector<unsigned char> PlayerIDs;
	unsigned char TeamSizes[12];
	double PlayerScores[13];
	memset( TeamSizes, 0, sizeof( unsigned char ) * 12 );

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		unsigned char PID = (*i)->GetPID( );

		if( PID < 13 )
		{
			unsigned char SID = GetSIDFromPID( PID );

			if( SID < m_Slots.size( ) )
			{
				unsigned char Team = m_Slots[SID].GetTeam( );

				if( Team < 12 )
				{
					// we are forced to use a default score because there's no way to balance the teams otherwise

					double Score = (*i)->GetScore( );

					if( Score < -99999.0 )
						Score = m_Map->GetMapDefaultPlayerScore( );

					PlayerIDs.push_back( PID );
					TeamSizes[Team]++;
					PlayerScores[PID] = Score;
				}
			}
		}
	}

	sort( PlayerIDs.begin( ), PlayerIDs.end( ) );

	// balancing the teams is a variation of the bin packing problem which is NP
	// we can have up to 12 players and/or teams so the scope of the problem is sometimes small enough to process quickly
	// let's try to figure out roughly how much work this is going to take
	// examples:
	//  2 teams of 4 =     70 ~    5ms *** ok
	//  2 teams of 5 =    252 ~    5ms *** ok
	//  2 teams of 6 =    924 ~   20ms *** ok
	//  3 teams of 2 =     90 ~    5ms *** ok
	//  3 teams of 3 =   1680 ~   25ms *** ok
	//  3 teams of 4 =  34650 ~  250ms *** will cause a lag spike
	//  4 teams of 2 =   2520 ~   30ms *** ok
	//  4 teams of 3 = 369600 ~ 3500ms *** unacceptable

	uint32_t AlgorithmCost = 0;
	uint32_t PlayersLeft = PlayerIDs.size( );

	for( unsigned char i = 0; i < 12; i++ )
	{
		if( TeamSizes[i] > 0 )
		{
			if( AlgorithmCost == 0 )
				AlgorithmCost = nCr( PlayersLeft, TeamSizes[i] );
			else
				AlgorithmCost *= nCr( PlayersLeft, TeamSizes[i] );

			PlayersLeft -= TeamSizes[i];
		}
	}

	if( AlgorithmCost > 40000 )
	{
		// the cost is too high, don't run the algorithm
		// a possible alternative: stop after enough iterations and/or time has passed

		CONSOLE_Print( "[GAME: " + m_GameName + "] shuffling slots instead of balancing - the algorithm is too slow (with a cost of " + UTIL_ToString( AlgorithmCost ) + ") for this team configuration" );
		SendAllChat( m_GHost->m_Language->ShufflingPlayers( ) );
		ShuffleSlots( );
		return;
	}

	uint32_t StartTicks = GetTicks( );
	vector<unsigned char> BestOrdering = BalanceSlotsRecursive( PlayerIDs, TeamSizes, PlayerScores, 0 );
	uint32_t EndTicks = GetTicks( );

	// the BestOrdering assumes the teams are in slot order although this may not be the case
	// so put the players on the correct teams regardless of slot order

	vector<unsigned char> :: iterator CurrentPID = BestOrdering.begin( );

	for( unsigned char i = 0; i < 12; i++ )
	{
		unsigned char CurrentSlot = 0;

		for( unsigned char j = 0; j < TeamSizes[i]; j++ )
		{
			while( CurrentSlot < m_Slots.size( ) && m_Slots[CurrentSlot].GetTeam( ) != i )
				CurrentSlot++;

			// put the CurrentPID player on team i by swapping them into CurrentSlot

			unsigned char SID1 = CurrentSlot;
			unsigned char SID2 = GetSIDFromPID( *CurrentPID );

			if( SID1 < m_Slots.size( ) && SID2 < m_Slots.size( ) )
			{
				CGameSlot Slot1 = m_Slots[SID1];
				CGameSlot Slot2 = m_Slots[SID2];
				m_Slots[SID1] = CGameSlot( Slot2.GetPID( ), Slot2.GetDownloadStatus( ), Slot2.GetSlotStatus( ), Slot2.GetComputer( ), Slot1.GetTeam( ), Slot1.GetColour( ), Slot1.GetRace( ) );
				m_Slots[SID2] = CGameSlot( Slot1.GetPID( ), Slot1.GetDownloadStatus( ), Slot1.GetSlotStatus( ), Slot1.GetComputer( ), Slot2.GetTeam( ), Slot2.GetColour( ), Slot2.GetRace( ) );
			}
			else
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] shuffling slots instead of balancing - the balancing algorithm tried to do an invalid swap (this shouldn't happen)" );
				SendAllChat( m_GHost->m_Language->ShufflingPlayers( ) );
				ShuffleSlots( );
				return;
			}

			CurrentPID++;
			CurrentSlot++;
		}
	}

	CONSOLE_Print( "[GAME: " + m_GameName + "] balancing slots completed in " + UTIL_ToString( EndTicks - StartTicks ) + "ms (with a cost of " + UTIL_ToString( AlgorithmCost ) + ")" );
	SendAllChat( m_GHost->m_Language->BalancingSlotsCompleted( ) );
	SendAllSlotInfo( );

	for( unsigned char i = 0; i < 12; i++ )
	{
		bool TeamHasPlayers = false;
		double TeamScore = 0.0;

		for( vector<CGamePlayer *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); j++ )
		{
			unsigned char SID = GetSIDFromPID( (*j)->GetPID( ) );

			if( SID < m_Slots.size( ) && m_Slots[SID].GetTeam( ) == i )
			{
				TeamHasPlayers = true;
				double Score = (*j)->GetScore( );

				if( Score < -99999.0 )
					Score = m_Map->GetMapDefaultPlayerScore( );

				TeamScore += Score;
			}
		}

		if( TeamHasPlayers )
			SendAllChat( m_GHost->m_Language->TeamCombinedScore( UTIL_ToString( i + 1 ), UTIL_ToString( TeamScore, 2 ) ) );
	}
	m_GHost->UDPChatSend("|lobbyupdate");		
}

void CBaseGame :: AddToSpoofed( string server, string name, bool sendMessage )
{
	CGamePlayer *Player = GetPlayerFromName( name, true );

	if( Player )
	{
		Player->SetSpoofedRealm( server );
		Player->SetSpoofed( true );

		if (!m_GHost->IsSpoofedIP(name, Player->GetExternalIPString()))
			m_GHost->AddSpoofedIP(name, Player->GetExternalIPString());

		if( sendMessage )
			SendAllChat( m_GHost->m_Language->SpoofCheckAcceptedFor( server, name ) );
	}
}

bool CBaseGame :: IsMuted ( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Muted.begin( ); i != m_Muted.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

uint32_t CBaseGame :: IsCensorMuted ( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i != m_CensorMuted.size(); i++ )
	{
		if( m_CensorMuted[i] == name )
			return m_CensorMutedSeconds[i]-(GetTime()-m_CensorMutedTime[i]);
	}

	return 0;
}

void CBaseGame :: DelFromMuted( string name)
{
	string nam = name;
	transform( nam.begin( ), nam.end( ), nam.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Muted.begin( ); i != m_Muted.end( ); i++ )
	{
		if( *i == nam )
		{
			m_Muted.erase(i);
			return;
		}
	}
}

unsigned char CBaseGame :: GetTeam ( unsigned char SID)
{
	return m_Slots[SID].GetTeam();
}

bool CBaseGame :: IsListening ( unsigned char user)
{
	stringstream SS;
	unsigned char luser;
	string u;
	u = user;
	if (m_ListenUser==u)
		return true;
	SS << m_ListenUser;
	while( !SS.eof( ) )
	{
		SS >> luser;
		if (luser == user) 
			return true;
	}
	return false;
}

void CBaseGame :: SwitchDeny ( unsigned char PID)
{
	m_SwitchTime = 0;
	CGamePlayer *p = GetPlayerFromPID(PID);
	CONSOLE_Print("[GAME: " + m_GameName + "] Switch canceled being denied by "+p->GetName());
}

void CBaseGame :: SwitchAccept ( unsigned char PID)
{
	//reset timers and players ff
        	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		        {
				(*i)->SetHadFFed(false);
				(*i)->SetTimeFFed(0);
				(*i)->SetCanFF(true);            
		         }

//reset timers and players RMK
        	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		        {
				(*i)->SetHadRMKed(false);
				(*i)->SetTimeRMKed(0);
				(*i)->SetCanRMK(true);
		         }
	if (m_SwitchTime == 0 || (GetTime() - m_SwitchTime) > 60)
		return;
	CGamePlayer *p = GetPlayerFromPID(PID);
	if (!p->m_Switchok)
	{
		p->m_Switchok = true;
		m_SwitchNr ++;
		CONSOLE_Print("[GAME: " + m_GameName + "] Switch accepted by "+p->GetName()+" "+UTIL_ToString(m_SwitchNr)+"/"+UTIL_ToString(m_Players.size()-1));
		if (m_SwitchNr == m_Players.size()-2)
		{
			// have to fix this somehow - fixed! hopefully :)
			SwapSlotsS(m_SwitchS, m_SwitchT);
			m_Switched = true;
			ReCalculateTeams();
			CONSOLE_Print("[GAME: " + m_GameName + "] Switch completed");			
			m_SwitchTime = 0;
			// disable gameoverteamdiff if switch is detected (until we handle switches)						
			m_GameOverDiffCanceled = true;


			uint32_t GameNr = GetGameNr();

			string message = "Switch completed: "+UTIL_ToString(m_SwitchS+1)+" <-> "+UTIL_ToString(m_SwitchT+1);
			string hname = m_VirtualHostName;
			if (hname.substr(0,2)=="|c")
				hname = hname.substr(10,hname.length()-10);

			m_GHost->UDPChatSend("|gamechat "+UTIL_ToString(GameNr) + " 0 1 "+hname+" "+message);

		}
	}
}
void CBaseGame :: InitSwitching ( unsigned char PID, string nr)
{
	uint32_t CNr;

	CNr = UTIL_ToUInt32(nr);
	if (CNr<1 || CNr>5)
		return;
	
	if (m_SwitchTime!=0 && (GetTime() - m_SwitchTime)<60)
		return;

	unsigned char SID = GetSIDFromPID(PID);
	unsigned char TSID;
	if (m_Slots[SID].GetTeam()==0)
		TSID = CNr + 4;
	else
		TSID = CNr - 1;

	m_SwitchTime = GetTime();
	m_SwitchNr = 0;
	CGamePlayer *p = GetPlayerFromPID(PID);
	m_SwitchS = SID;
	m_SwitchT = TSID;
	p->m_Switching = true;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++)
	{
		(*i)->m_Switchok = false;
	}
	CONSOLE_Print("[GAME: " + m_GameName + "] "+p->GetName()+" wants to switch");
}


void CBaseGame :: ReCalculateTeams( )
{
	unsigned char sid;
	unsigned char team;

	m_Team1 = 0;
	m_Team2 = 0;
	m_Team3 = 0;
	m_Team4 = 0;
	m_TeamDiff = 0;

	if (m_Players.empty())
		return;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++)
	{
		// ignore players who left and didn't get deleted yet.
		if (*i)
		if (!(*i)->GetLeftMessageSent( ))
		{
			sid = GetSIDFromPID((*i)->GetPID());
			if (sid!=255)
			{
				team = m_Slots[sid].GetTeam();
				if (team == 0)
					m_Team1++;
				if (team == 1)
					m_Team2++;
				if (team == 2)
					m_Team3++;
				if (team == 3)
					m_Team4++;
			}			
		}
	}

	if (m_GetMapNumTeams == 2)
	{
		if (m_Team1>m_Team2)
			m_TeamDiff = m_Team1-m_Team2;
		if (m_Team1<m_Team2)
			m_TeamDiff = m_Team2-m_Team1;
	}

}

void CBaseGame :: AddToListen( unsigned char user)
{
	stringstream SS;
	unsigned char luser;
	if (m_ListenUser=="")
	{
		m_ListenUser= user;
		m_Listen = true;
		return;
	}
	SS << m_ListenUser;
	while( !SS.eof( ) )
	{
		SS >> luser;
		if (luser == user) return;
	}
	m_ListenUser +=" "+user;
}

void CBaseGame :: DelFromListen( unsigned char user)
{
	stringstream SS;
	string tusers="";
	unsigned char luser;
	SS << m_ListenUser;
	while( !SS.eof( ) )
	{
		SS >> luser;
		if (luser != user)
		if (tusers.length()==0)
			tusers = luser;
		else
			tusers +=" "+luser;
	}
	m_ListenUser = tusers;
	if (m_ListenUser=="")
		m_Listen = false;
}

void CBaseGame :: AddToMuted( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	// check that the user is not already muted

	for( vector<string> :: iterator i = m_Muted.begin( ); i != m_Muted.end( ); i++ )
	{
		if( *i == name )
		{
			return;
		}
	}

	m_Muted.push_back( name );
}

void CBaseGame :: AddToCensorMuted( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	// check that the user is not already muted

	for( vector<string> :: iterator i = m_CensorMuted.begin( ); i != m_CensorMuted.end( ); i++ )
	{
		if( *i == name )
		{
			return;
		}
	}

	m_CensorMuted.push_back( name );

	unsigned long secs = 0;
	CGamePlayer *Player = GetPlayerFromName(name, false);
	Player->SetCensors(Player->GetCensors()+1);
	if (Player->GetCensors()==1)
		secs = m_GHost->m_CensorMuteFirstSeconds;
	if (Player->GetCensors()==2)
		secs = m_GHost->m_CensorMuteSecondSeconds;
	if (Player->GetCensors()>2)
		secs = m_GHost->m_CensorMuteExcessiveSeconds;

	m_CensorMutedSeconds.push_back( secs);
	m_CensorMutedTime.push_back( GetTime());
	AddToMuted(name);
//	CONSOLE_Print( "[GAME: " + m_GameName + "] "+name+" censor muted for "+UTIL_ToString(secs)+" seconds" );
	SendAllChat(m_GHost->m_Language->PlayerMutedForBeingFoulMouthed(name, UTIL_ToString(secs)));
}

void CBaseGame :: ProcessCensorMuted()
{
	if (GetTime()-m_CensorMutedLastTime>5)
	{
		m_CensorMutedLastTime = GetTime();
		for (uint32_t i = 0; i<m_CensorMuted.size(); i++)
		{
			// mute time has passed?
			if (m_CensorMutedLastTime>m_CensorMutedTime[i]+m_CensorMutedSeconds[i])
			{
				SendAllChat(m_GHost->m_Language->RemovedPlayerFromTheMuteList(m_CensorMuted[i]));
				DelFromMuted(m_CensorMuted[i]);
				m_CensorMuted.erase(m_CensorMuted.begin()+i);
				m_CensorMutedTime.erase(m_CensorMutedTime.begin()+i);
				m_CensorMutedSeconds.erase(m_CensorMutedSeconds.begin()+i);
				if (i==m_CensorMuted.size()) 
					return;
			}
		}
	}
}

bool CBaseGame :: IsGameName( string name )
{
	for( vector<string> :: iterator i = m_GameNames.begin( ); i != m_GameNames.end( ); i++ )
	{
		if( name.find( *i ) != string :: npos )
			return true;
	}

	return false;
}

void CBaseGame :: AddGameName( string name )
{
	// add game name to names list

	for( vector<string> :: iterator i = m_GameNames.begin( ); i != m_GameNames.end( ); i++ )
	{
		if( *i == name )
		{
			return;
		}
	}
	m_GameNames.push_back( name );
}

void CBaseGame :: AutoSetHCL ( )
{
	// auto set HCL if map_defaulthcl is not empty
	string gameName = m_GameName;
	transform( gameName.begin( ), gameName.end( ), gameName.begin( ), (int(*)(int))tolower );
	string m_Mode = string();
	string m_Modes = string();
	string m_Modes2 = string();
	bool forceindota = (m_GHost->m_forceautohclindota && m_Map->GetMapType().find("dota")!= string ::npos);
	if (m_GHost->m_autohclfromgamename || forceindota )
		if (!m_Map->GetMapDefaultHCL().empty() || forceindota)
			//	if (m_AutoHosted)
			if (gameName.find("-")!= string::npos)
			{
				uint32_t j = 0;
				uint32_t k = 0;
				uint32_t i = 0;
				string mode = string();
				while (j<gameName.length()-1 && (gameName.find("-",j)!= string::npos))
				{
					k = gameName.find("-",j);
					i = gameName.find(" ",k);
					if (i==0)
						i = gameName.length() - k + 1;
					else
						i = i - k;
					mode = gameName.substr(k, i);
					// keep the first mode separately to be used in HCL
					if (m_Mode.empty())
						m_Mode = gameName.substr(k+1, i-1);
					m_Modes += " "+mode;
					if (m_Modes2.length()>0)
						m_Modes2+=" ";
					m_Modes2+= mode;
					j = k + i;
				}
				if (!m_Mode.empty())
				{
					CONSOLE_Print( "[GHOST] autosetting HCL to [" + m_Mode + "]" );
					if( m_Mode.size( ) <= m_Slots.size( ) )
					{
						string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

						if( m_Mode.find_first_not_of( HCLChars ) == string :: npos )
						{
							SetHCL(m_Mode);
							if (m_Mode != m_HCLCommandString)
								SendAllChat( m_GHost->m_Language->SettingHCL( GetHCL() ) );
						}
						else
							SendAllChat( m_GHost->m_Language->UnableToSetHCLInvalid( ) );
					}
					else
						SendAllChat( m_GHost->m_Language->UnableToSetHCLTooLong( ) );			
				}

			} else
				// no gamemode detected from gamename, disable map_defaulthcl
			{
				SetHCL(string());
			}
}

void CBaseGame :: AddToReserved( string name, unsigned char nr )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	// check that the user is not already reserved

	vector<unsigned char> :: iterator j=m_ReservedS.begin();
	for( vector<string> :: iterator i = m_Reserved.begin( ); i != m_Reserved.end( ); i++ )
	{
		if( *i == name )
		{
			*j=nr;
			return;
		}
		j++;
	}

	m_Reserved.push_back( name );
	m_ReservedS.push_back( nr );

	// upgrade the user if they're already in the game

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		string NameLower = (*i)->GetName( );
		transform( NameLower.begin( ), NameLower.end( ), NameLower.begin( ), (int(*)(int))tolower );

		if( NameLower == name )
			(*i)->SetReserved( true );
	}
}

void CBaseGame :: DelFromReserved( string name)
{
	string nam = name;
	transform( nam.begin( ), nam.end( ), nam.begin( ), (int(*)(int))tolower );

	uint32_t j = 0;
	for( vector<string> :: iterator i = m_Reserved.begin( ); i != m_Reserved.end( ); i++ )
	{
		if( *i == nam )
		{
			m_Reserved.erase(i);
			m_ReservedS.erase(m_ReservedS.begin()+j);
			return;
		}
		j++;
	}

	// downgrade the user if they're already in the game

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		string NameLower = (*i)->GetName( );
		transform( NameLower.begin( ), NameLower.end( ), NameLower.begin( ), (int(*)(int))tolower );

		if( NameLower == nam )
			(*i)->SetReserved( false );
	}
}

bool CBaseGame :: IsOwner( string name )
{
	string OwnerLower = m_OwnerName;
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	transform( OwnerLower.begin( ), OwnerLower.end( ), OwnerLower.begin( ), (int(*)(int))tolower );
	return name == OwnerLower;
}

bool CBaseGame :: IsReserved( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Reserved.begin( ); i != m_Reserved.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

unsigned char CBaseGame :: ReservedSlot(string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	
	vector<unsigned char>::iterator j=m_ReservedS.begin();
	for( vector<string> :: iterator i = m_Reserved.begin( ); i != m_Reserved.end( ); i++ )
	{
		if( *i == name )
			return *j;
		j++;
	}

	return 255;
}

void CBaseGame :: ResetReservedSlot (string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	vector<unsigned char> :: iterator j=m_ReservedS.begin();
	for( vector<string> :: iterator i = m_Reserved.begin( ); i != m_Reserved.end( ); i++ )
	{
		if( *i == name )
		{
			*j=255;
			return;
		}
		j++;
	}

}

bool CBaseGame :: IsDownloading( )
{
	// returns true if at least one player is downloading the map

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetDownloadStarted( ) && !(*i)->GetDownloadFinished( ) )
			return true;
	}

	return false;
}

bool CBaseGame :: IsGameDataSaved( )
{
	return true;

}

void CBaseGame :: SaveGameData( )
{

}

uint32_t CBaseGame :: DownloaderNr( string name)
{
	uint32_t result = 1;	

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetDownloadStarted( ) && !(*i)->GetDownloadFinished( ) )
		{
			if ((*i)->GetName()!=name)
				result++;
			else return result;
		}
	}

	return result;

}


uint32_t CBaseGame :: CountDownloading( )
{
	uint32_t result = 0;	

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetDownloadStarted( ) && !(*i)->GetDownloadFinished( ) )
			result++;
	}

	return result;
}

uint32_t CBaseGame :: ShowDownloading( )
{	
	if (GetTime()-m_DownloadInfoTime<m_GHost->m_ShowDownloadsInfoTime || !m_GHost->m_ShowDownloadsInfo)
		return 0;

	m_DownloadInfoTime = GetTime();

	uint32_t result = 0;	
	string Down;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetDownloadStarted( ) && !(*i)->GetDownloadFinished( ) )
		{
			if (Down.length()>0)
				Down += ", ";
			Down += (*i)->GetName()+": "+(*i)->GetDownloadInfo();
				result++;
		}
	}

	CONSOLE_Print( "[GAME: " + m_GameName + "] "+Down );
	SendAllChat(Down);
	return result;
}

void CBaseGame :: StartCountDown( bool force )
{
	if( !m_CountDownStarted )
	{
		if( force )
		{
			m_CountDownStarted = true;
			m_CountDownCounter = 10;
			if (m_NormalCountdown)
			SendAll( m_Protocol->SEND_W3GS_COUNTDOWN_START( ) );
		}
		else
		{
			// check if the HCL command string is short enough

			if( m_HCLCommandString.size( ) > GetSlotsOccupied( ) )
			{
				SendAllChat( m_GHost->m_Language->TheHCLIsTooLongUseForceToStart( ) );
				return;
			}

			// check if everyone has the map

			string StillDownloading;

			for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
			{
				if( (*i).GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && (*i).GetComputer( ) == 0 && (*i).GetDownloadStatus( ) != 100 )
				{
					CGamePlayer *Player = GetPlayerFromPID( (*i).GetPID( ) );

					if( Player )
					{
						if( StillDownloading.empty( ) )
							StillDownloading = Player->GetName( );
						else
							StillDownloading += ", " + Player->GetName( );
					}
				}
			}

			if( !StillDownloading.empty( ) )
				SendAllChat( m_GHost->m_Language->PlayersStillDownloading( StillDownloading ) );

			// check if everyone is spoof checked

			string NotSpoofChecked;

			if( m_GHost->m_RequireSpoofChecks )
			{
				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					if( !(*i)->GetSpoofed( ) )
					{
						if( NotSpoofChecked.empty( ) )
							NotSpoofChecked = (*i)->GetName( );
						else
							NotSpoofChecked += ", " + (*i)->GetName( );
					}
				}

				if( !NotSpoofChecked.empty( ) )
					SendAllChat( m_GHost->m_Language->PlayersNotYetSpoofChecked( NotSpoofChecked ) );
			}

			// check if everyone has been pinged enough (3 times) that the autokicker would have kicked them by now
			// see function EventPlayerPongToHost for the autokicker code

			string NotPinged;
			
			bool AutoHostLocal = (m_autohosted && m_GHost->m_AutoHostLocal);
			if (!AutoHostLocal)
			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( !(*i)->GetReserved( ) && (*i)->GetNumPings( ) < 3 )
				{
					if( NotPinged.empty( ) )
						NotPinged = (*i)->GetName( );
					else
						NotPinged += ", " + (*i)->GetName( );
				}
			}

			if( !NotPinged.empty( ) )
				SendAllChat( m_GHost->m_Language->PlayersNotYetPinged( NotPinged ) );

			// if no problems found start the game

			if( StillDownloading.empty( ) && NotSpoofChecked.empty( ) && NotPinged.empty( ) )
			{
				m_CountDownStarted = true;
				m_CountDownCounter = 10;
				if (m_NormalCountdown)
				SendAll( m_Protocol->SEND_W3GS_COUNTDOWN_START( ));
			}
		}
	}
}

void CBaseGame :: StartCountDownAuto( bool requireSpoofChecks )
{
	if( !m_CountDownStarted )
	{
		// check if enough players are present
		ReCalculateTeams();
		uint32_t PNr;
		if (m_GetMapNumTeams<=4)
			PNr = GetNumHumanPlayers();
		else
			PNr = m_Team1+m_Team2+m_Team3+m_Team4;
		if( PNr < m_AutoStartPlayers )
		{
			if(m_GHost->m_PlayerBeforeStartPrintDelay > m_ActualyPrintPlayerWaitingStartDelay)
			{
				m_ActualyPrintPlayerWaitingStartDelay++;
			}
			else
			{
				string s = string();
				s = m_GHost->m_Language->WaitingForPlayersBeforeAutoStart( UTIL_ToString( m_AutoStartPlayers ), UTIL_ToString( m_AutoStartPlayers - PNr ));
				m_ActualyPrintPlayerWaitingStartDelay = 0;
				bool EnoughPlayers = false;
				if (m_GetMapNumTeams!=2)
				if (GetNumPlayers()>=2)
					EnoughPlayers = true;
				if (GetNumPlayers()<2)
					EnoughPlayers = true;
				if (m_GetMapNumTeams==2)
				if (m_Team1>=1 && m_Team2>=1)
					EnoughPlayers = true;
				if (m_GHost->m_AutoHostAllowStart && EnoughPlayers)
				s = s+" "+string(1, m_GHost->m_CommandTrigger)+"start to start now!";
				if (!s.empty())
					SendAllChat(s);
			}
			return;
		}
		// check if everyone has the map

		string StillDownloading;

		for( vector<CGameSlot> :: iterator i = m_Slots.begin( ); i != m_Slots.end( ); i++ )
		{
			if( (*i).GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && (*i).GetComputer( ) == 0 && (*i).GetDownloadStatus( ) != 100 )
			{
				CGamePlayer *Player = GetPlayerFromPID( (*i).GetPID( ) );

				if( Player )
				{
					if( StillDownloading.empty( ) )
						StillDownloading = Player->GetName( );
					else
						StillDownloading += ", " + Player->GetName( );
				}
			}
		}

		if( !StillDownloading.empty( ) )
		{
			SendAllChat( m_GHost->m_Language->PlayersStillDownloading( StillDownloading ) );
			return;
		}

		// check if everyone is spoof checked

		string NotSpoofChecked;

		if( requireSpoofChecks )
		{
			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
			{
				if( !(*i)->GetSpoofed( ) )
				{
					if( NotSpoofChecked.empty( ) )
						NotSpoofChecked = (*i)->GetName( );
					else
						NotSpoofChecked += ", " + (*i)->GetName( );
				}
			}

			if( !NotSpoofChecked.empty( ) )
				SendAllChat( m_GHost->m_Language->PlayersNotYetSpoofChecked( NotSpoofChecked ) );
		}

		// check if everyone has been pinged enough (3 times) that the autokicker would have kicked them by now
		// see function EventPlayerPongToHost for the autokicker code

		string NotPinged;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( !(*i)->GetReserved( ) && (*i)->GetNumPings( ) < 3 )
			{
				if( NotPinged.empty( ) )
					NotPinged = (*i)->GetName( );
				else
					NotPinged += ", " + (*i)->GetName( );
			}
		}

		if( !NotPinged.empty( ) )
		{
			SendAllChat( m_GHost->m_Language->PlayersNotYetPingedAutoStart( NotPinged ) );
			return;
		}

		// if no problems found start the game

		if( StillDownloading.empty( ) && NotSpoofChecked.empty( ) && NotPinged.empty( ) )
		{
			m_CountDownStarted = true;
			m_CountDownCounter = 10;
		}
	}
}

void CBaseGame :: StopPlayers( string reason )
{
	// disconnect every player and set their left reason to the passed string
	// we use this function when we want the code in the Update function to run before the destructor (e.g. saving players to the database)
	// therefore calling this function when m_GameLoading || m_GameLoaded is roughly equivalent to setting m_Exiting = true
	// the only difference is whether the code in the Update function is executed or not

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		(*i)->SetDeleteMe( true );
		(*i)->SetLeftReason( reason );
		(*i)->SetLeftCode( PLAYERLEAVE_LOST );
	}
}

void CBaseGame :: StopLaggers( string reason )
{
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( (*i)->GetLagging( ) )
		{
			(*i)->SetDeleteMe( true );
			(*i)->SetLeftReason( reason );
			(*i)->SetLeftCode( PLAYERLEAVE_DISCONNECT );
		}
	}
}

void CBaseGame :: CreateVirtualHost( )
{
	if( m_VirtualHostPID != 255 )
		return;

	m_VirtualHostPID = GetNewPID( );
	BYTEARRAY IP;
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );
	IP.push_back( 0 );
	SendAll( m_Protocol->SEND_W3GS_PLAYERINFO( m_VirtualHostPID, m_VirtualHostName, IP, IP ) );
}

void CBaseGame :: DeleteVirtualHost( )
{
	if( m_VirtualHostPID == 255 )
		return;

	SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( m_VirtualHostPID, PLAYERLEAVE_LOBBY ) );
	m_VirtualHostPID = 255;
}

void CBaseGame :: CreateFakePlayer( )
{
	if( m_FakePlayers.size( ) > 10 )
			return;

	unsigned char SID = GetEmptySlot( false );

	if( SID < m_Slots.size( ) )
	{
		if( GetSlotsOccupied( ) >= m_Slots.size() - 1 )
			DeleteVirtualHost( );

		unsigned char FakePlayerPID = GetNewPID( );

		BYTEARRAY IP;
		IP.push_back( 0 );
		IP.push_back( 0 );
		IP.push_back( 0 );
		IP.push_back( 0 );
		
		SendAll( m_Protocol->SEND_W3GS_PLAYERINFO( FakePlayerPID, "Troll[" + UTIL_ToString( FakePlayerPID ) + "]", IP, IP ) );
		m_Slots[SID] = CGameSlot( FakePlayerPID, 100, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam( ), m_Slots[SID].GetColour( ), m_Slots[SID].GetRace( ) );
		m_FakePlayers.push_back( FakePlayerPID );
		SendAllSlotInfo( );
	}
}

void CBaseGame :: DeleteFakePlayer( )
{
	if( m_FakePlayers.empty( ) )
		return;

	for( unsigned char i = 0; i < m_Slots.size( ); ++i )
	{
		for( vector<unsigned char> :: iterator j = m_FakePlayers.begin( ); j != m_FakePlayers.end( ); ++j )
		{
			if( m_Slots[i].GetPID( ) == *j )
			{
				m_Slots[i] = CGameSlot( 0, 255, SLOTSTATUS_OPEN, 0, m_Slots[i].GetTeam( ), m_Slots[i].GetColour( ), m_Slots[i].GetRace( ) );
				SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( *j, PLAYERLEAVE_LOBBY ) );
			}
		}
	}

	m_FakePlayers.clear( );
	SendAllSlotInfo( );
}


void CBaseGame :: CreateWTVPlayer( string name, bool lobbyhost )
{
#ifdef WIN32
	if( m_GHost->m_CurrentGame->wtvprocessid == 0 && !lobbyhost )
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);
		ZeroMemory( &pi, sizeof(pi) );

		string wtvRecorderEXE = m_GHost->m_wtvPath + "\\wtvRecorder.exe";

		int hProcess = CreateProcessA( wtvRecorderEXE.c_str( ), NULL, NULL, NULL, TRUE, HIGH_PRIORITY_CLASS, NULL, m_GHost->m_wtvPath.c_str( ), LPSTARTUPINFOA(&si), &pi );

		if( !hProcess )
			CONSOLE_Print( "[WaaaghTV] : Failed to start wtvRecorder.exe" );
		else
		{
			m_GHost->m_CurrentGame->wtvprocessid = int( pi.dwProcessId );
			CONSOLE_Print( "[WaaaghTV] : wtvRecorder.exe started!" );
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}

	if( m_WTVPlayerPID != 255 )
		return;

	unsigned char SID = GetEmptySlot( false );

	if( SID < m_Slots.size( ) )
	{
		m_WTVPlayerPID = GetNewPID( );
		BYTEARRAY IP;
		IP.push_back( 0 );
		IP.push_back( 0 );
		IP.push_back( 0 );
		IP.push_back( 0 );
		m_GHost->m_wtvPlayerName = name;
		SendAll( m_Protocol->SEND_W3GS_PLAYERINFO( m_WTVPlayerPID, name, IP, IP ) );
		m_Slots[SID] = CGameSlot( m_WTVPlayerPID, 100, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam( ), m_Slots[SID].GetColour( ), m_Slots[SID].GetRace( ) );
		SendAllSlotInfo( );

		for( unsigned char i = 0; i < m_Slots.size( ); i++ )
		{
			if( m_Slots[i].GetPID( ) == m_WTVPlayerPID )
			{
				SwapSlots( i, m_Slots.size()-2 );
				OpenSlot(i, false);
				break;
			}
		}

		CloseSlot(  m_Slots.size()-1, true );
	}
#endif
}

void CBaseGame :: DeleteWTVPlayer( )
{
#ifdef WIN32
	if( m_GHost->m_CurrentGame->wtvprocessid != 0 )
	{
		HANDLE hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, m_GHost->m_CurrentGame->wtvprocessid );

		if( m_GHost->m_CurrentGame->wtvprocessid != GetCurrentProcessId( ) && m_GHost->m_CurrentGame->wtvprocessid != 0 )
		{
			CONSOLE_Print( "[WaaaghTV] shutting down wtvrecorder.exe" );

			if( !TerminateProcess(hProcess, 0) )
				CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (1-1-1)" );
			else
				CloseHandle( hProcess );
		}
		else
			CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (1-2-1)" );

		m_GHost->m_CurrentGame->wtvprocessid = 0;
	}

	if( m_WTVPlayerPID == 255 )
		return;

	for( unsigned char i = 0; i < m_Slots.size( ); i++ )
	{
		if( m_Slots[i].GetPID( ) == m_WTVPlayerPID )
			m_Slots[i] = CGameSlot( 0, 255, SLOTSTATUS_OPEN, 0, m_Slots[i].GetTeam( ), m_Slots[i].GetColour( ), m_Slots[i].GetRace( ) ); 
	}

	SendAll( m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS( m_WTVPlayerPID, PLAYERLEAVE_LOBBY ) );
	SendAllSlotInfo( );
	m_WTVPlayerPID = 255;

	OpenSlot(  m_Slots.size()-2, true );
	OpenSlot(  m_Slots.size()-1, true );
#endif
}

uint32_t CBaseGame :: GetGameNr( )
{
	uint32_t GameNr = 0;
	for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); i++ )
	{
		if ((*i)->GetCreationTime()==GetCreationTime() && (*i)->GetGameName()==GetGameName())
		{
			break;
		}
		GameNr++;
	}
	return GameNr;
}

bool CBaseGame :: IsAutoBanned( string name )
{
	for( vector<string> :: iterator i = m_AutoBanTemp.begin( ); i != m_AutoBanTemp.end( ); i++ )
	{
		if( *i == name )
			return true;
	}

	return false;
}

bool CBaseGame :: IsSafe( string name )
{
	bool safe = false;
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		safe = (*i)->IsSafe(name);
		if (safe)
			break;
	}
	return safe;	
}

string CBaseGame :: Voucher( string name )
{
	string voucher = string();
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		voucher = (*i)->Voucher(name);
		if (!voucher.empty())
			break;
	}
	return voucher;	
}

void CBaseGame :: AddNote( string name, string note )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if ((*i)->GetServer()==m_Server)
			(*i)->AddNote(name, note);
	}
	return;
}

bool CBaseGame :: IsNoted( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	bool noted = false;
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		noted = (*i)->IsNoted(name);
		if (noted)
			break;
	}
	return noted;	
}

string CBaseGame :: Note( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string note = string();
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		note = (*i)->Note(name);
		if (!note.empty())
			break;
	}
	return note;	
}

bool CBaseGame :: IsAdmin( string name )
{
	bool admin = false;
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		admin = (*i)->IsAdmin(name);
		if (admin)
			break;
	}
	return admin;	
}

bool CBaseGame :: IsRootAdmin( string name )
{
	bool admin = false;
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		admin = (*i)->IsRootAdmin(name);
		if (admin)
			break;
	}
	return admin;	
}

string CBaseGame :: CustomReason ( uint32_t ctime, string reason, string name)
{
	struct tm * timeinfo;
	string Reason = string();
	char buffer [80];
	string sDate;
	time_t Now = time( NULL );
	timeinfo = localtime( &Now );
	strftime (buffer,80,"%B %Y",timeinfo);  
	sDate = buffer;

	// time in minutes since game loaded
	float iTime = (float)(ctime - m_GameLoadedTime)/60;
	if (m_GetMapType == "dota")
		if (iTime>2)
			iTime -=2;
		else
			iTime = 1;
	string sTime = UTIL_ToString( iTime,0 ) + "m";
	string sTeam = string();
	string st1 = string();
	string st2 = string();
	unsigned char t;
	unsigned char c;
	uint32_t t1;
	uint32_t t2;
	bool found = false;

	CGamePlayer *Player = GetPlayerFromName( name, true );


	if (m_GetMapNumTeams==2)
	{
		for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); i++ )
		{
			if ((*i)->GetName()==name)
			{
				t = (*i)->GetTeam();
				c = (*i)->GetColour();
				t1 = (*i)->GetTeam1();
				t2 = (*i)->GetTeam2();
				found = true;
			}
		}

		if (!found && Player)
		{
			t = m_Slots[Player->GetSID()].GetTeam();
			c = m_Slots[Player->GetSID()].GetColour();
			t1 = m_Team1;
			t2 = m_Team2;
			if (t==0)
				t1--;
			else
				t2--;
		}

		if (t==0)
			st1 = UTIL_ToString(t1+1)+"*";
		else
			st1 = UTIL_ToString(t1);						
		if (t==1)
			st2 = UTIL_ToString(t2+1)+"*";
		else
			st2 = UTIL_ToString(t2);						

		sTeam = " "+st1+"v"+st2;
	}

	Reason = reason;

	if (Reason =="" || Reason == "l" || Reason == "noob" || Reason == "leaver" || Reason == "i" || Reason =="r"
		|| Reason == "lag" || Reason == "feeder" || Reason == "lagger" || Reason == "idiot" || Reason == "f" || Reason == "n" || Reason == "mh" || Reason == "m")
	{

		if (Reason=="l" || Reason=="leaver")
			Reason = "Leaver";

		if (Reason=="i" || Reason =="idiot")
			Reason = "Idiot";

		if (Reason=="r")
			Reason = "Rager";

		if (Reason=="lag" || Reason == "lagger")
			Reason = "Lagger";

		if (Reason=="f" || Reason =="feeder")
			Reason = "Feeder";

		if (Reason=="n" || Reason =="noob")
			Reason = "Noob";

		if (Reason=="mh" || Reason =="m")
			Reason = "Map Hacker";

		Reason = m_GHost->m_Language->BanReason(Reason, sTime, sDate, sTeam, GetGameName());
	}
	return Reason;
}

string CBaseGame :: CustomReason ( string reason, string name)
{
	return CustomReason(GetTime(), reason, name);
}

string CBaseGame :: GetGameInfo()
{
	string sMsg = string();
	uint32_t GameNr = GetGameNr()+1;
	if (!m_GameLoading && !m_GameLoaded)
	{
		string slots = UTIL_ToString(m_GHost->m_CurrentGame->GetSlotsOccupied())+"/"+UTIL_ToString(m_GHost->m_CurrentGame->m_Slots.size());
		sMsg = "Lobby: "+ m_GHost->m_CurrentGame->GetGameName()+" ("+slots+") - "+m_GHost->m_CurrentGame->GetOwnerName();
	} else
	{
		struct tm * timeinfo;
		char buffer [80];
		string sDate;
		time_t Now = time( NULL );
		timeinfo = localtime( &Now );
		strftime (buffer,80,"%B %Y",timeinfo);  
		sDate = buffer;
		// time in minutes since game loaded
		float iTime = (float)(GetTime() - m_GameLoadedTime)/60;
		if (!m_GameLoaded)
			iTime = 0;
		if (m_GetMapType == "dota")
			if (iTime>2)
				iTime -=2;
			else
				iTime = 1;
		string sScore = string();
		string sTime = UTIL_ToString( iTime,0 );
		if (sTime.length()<2)
			sTime = "0"+sTime;
		sTime = sTime + "m : ";
		string sTeam = string();
		string st1 = string();
		string st2 = string();
		bool found = false;

		//		CGamePlayer *Player = GetPlayerFromName( name, true );
		if (m_GetMapNumTeams==2)
		{
			st1 = UTIL_ToString(m_Team1);
			st2 = UTIL_ToString(m_Team2);						

			sTeam = st1+"v"+st2+" : ";
		}

		if (m_GetMapType == "dota")
		{
			uint32_t ssteam1 = 0;
			uint32_t ssteam2 = 0;
			uint32_t t;
			uint32_t s;
			for (uint32_t i=0; i<m_Players.size(); i++)
			{
				t = m_Slots[m_Players[i]->GetSID()].GetTeam();
				s = m_Players[i]->GetDOTAKills();
				if (t==0)
					ssteam1 +=s;
				else
					ssteam2 +=s;
			}
			sScore = UTIL_ToString(ssteam1)+"-"+UTIL_ToString(ssteam2)+" : ";
		}
		string sAdmin = m_OwnerName+" : ";
		string sGameNr = "#" + UTIL_ToString(GameNr)+": [ ";
		string sGameN = GetGameName();
		sMsg = sGameNr+sTime+sTeam+sScore+sAdmin+sGameN+" ]";
	}
	return sMsg;
}

void CBaseGame :: SetDynamicLatency( )
{
	// set dynamic latency every 5 seconds
	if (GetTicks() - m_LastDynamicLatencyTicks < 5000)
		return;

	if (m_GameLoading)
		return;

	uint32_t maxadd = m_GHost->m_DynamicLatencyMaxToAdd;

	m_DynamicLatency = m_Latency;
	m_LastDynamicLatencyTicks = GetTicks();

	string Console = string();
	double percent = 0;
	string percents = string();

	// get the highest ping in the game
	
	uint32_t highestping = 0;
	uint32_t ping = 0;

	for (uint32_t i=0; i<m_Players.size(); i++)
	{
		if (m_Players[i]->GetNumPings( ) > 0)
		{
			ping = m_Players[i]->GetPing(m_GHost->m_LCPings);

			if (ping>highestping)
				highestping = ping;
		}
	}

	if (highestping + m_GHost->m_DynamicLatencyAddedToPing < m_Latency)
		m_DynamicLatency = highestping + m_GHost->m_DynamicLatencyAddedToPing;
	else
		m_DynamicLatency = m_Latency + maxadd/2;

// if there is a lobby, increase latency to max
	if (m_GHost->m_DynamicLatencyIncreasewhenLobby && m_GHost->m_CurrentGame)
	{
		m_DynamicLatency = m_Latency + maxadd/3;

// reduce it to 2.2x highest ping if it is more than that.
		if (m_GHost->m_DynamicLatency2xPingMax)
		if (m_DynamicLatency> (uint32_t) (highestping * 2.2))
			m_DynamicLatency = (uint32_t) (highestping * 2.2);
	}

	// if the actions are sent too slow, set dynamic latency to max + 30

	bool LateActions = (GetTicks() - m_LastActionLateTicks < 2000) && m_GHost->m_newLatency;

	if (LateActions || m_Lagging)
	{
		m_DynamicLatency = m_Latency + maxadd;

		// reduce it to 2.2x highest ping if it is more than that.
		if (m_GHost->m_DynamicLatency2xPingMax)
		if (m_DynamicLatency> (uint32_t) (highestping * 2.2))
			m_DynamicLatency = (uint32_t) (highestping * 2.2);
/*
		double percent = m_DynamicLatency*100/m_Latency;
		string percents = UTIL_ToString(percent, 0);

		Console = "[GAME: " + m_GameName + "] Dynamic latency at "+percents+"% = "+UTIL_ToString(m_DynamicLatency)+" ms="+UTIL_ToString(m_Latency)+"+"+UTIL_ToString(maxadd)+" to compensate late actions";
		if (GetTicks() - m_GHost->m_LastDynamicLatencyConsole > 30000)
		{
			CONSOLE_Print(Console);
		}
		return;
*/
	}

// further tweak the dynamic latency based on the players max sync and how far it is from the synclimit
// modified to consider synclimit = 12

	uint32_t synclimit = 12;

	double syncpercent = m_MaxSync * 100 / synclimit;
	double latencydiff = m_Latency - m_DynamicLatency;

	if (syncpercent>100)
		syncpercent = 100;

	// if someone is behind on the sync, increase latency a bit
	if (syncpercent>=90)
		latencydiff += maxadd;

	double tweaklatencyby = (syncpercent * latencydiff / 100);

	if (tweaklatencyby>latencydiff+maxadd)
		tweaklatencyby = latencydiff+maxadd;

	if (m_DynamicLatency + tweaklatencyby> m_Latency + maxadd)
		tweaklatencyby = m_Latency + maxadd - m_DynamicLatency;

	m_DynamicLatency += (uint32_t)tweaklatencyby;

/*
	// reduce it to 2x highest ping if it is more than that.
	if (m_GHost->m_DynamicLatency2xPingMax)
		if (m_DynamicLatency> highestping * 2)
			m_DynamicLatency = highestping * 2;
*/

// don't go lower than 20/50 ms
	uint32_t lowestlatencyallowed = 20;
	if (!m_GHost->m_newLatency)
		lowestlatencyallowed = 50;

	if (m_DynamicLatency<lowestlatencyallowed)
		m_DynamicLatency = lowestlatencyallowed;

	percent = m_DynamicLatency*100/m_Latency;
	percents = UTIL_ToString(percent, 0);

	int diff = m_Latency - m_DynamicLatency;
	uint32_t absdiff = abs(diff);
	string op = string();
	if (m_DynamicLatency < m_Latency)
		op = "-";
	else if (m_DynamicLatency > m_Latency)
		op = "+";
	if (op.empty())
		Console = "[GAME: " + m_GameName + "] Dynamic latency at "+percents+"% = "+UTIL_ToString(m_DynamicLatency)+" ms"+" sync="+UTIL_ToString(syncpercent)+"% - "+m_MaxSyncUser+"";
	else
		Console = "[GAME: " + m_GameName + "] Dynamic latency at "+percents+"% = "+UTIL_ToString(m_DynamicLatency)+" ms="+UTIL_ToString(m_Latency)+op+UTIL_ToString(absdiff)+""+" sync="+UTIL_ToString(syncpercent)+"% - "+m_MaxSyncUser+"";

	if (GetTicks() - m_GHost->m_LastDynamicLatencyConsole > 30000)
	{
		m_GHost->m_Log = false;
		CONSOLE_Print(Console);
		m_GHost->m_Log = true;
	}
}
