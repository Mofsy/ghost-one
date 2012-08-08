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

#ifndef GAME_BASE_H
#define GAME_BASE_H

#include "gameslot.h"

//
// CBaseGame
//

class CTCPServer;
class CGameProtocol;
class CPotentialPlayer;
class CGamePlayer;
class CDBGamePlayer;
class CDBBan;
class CMap;
class CSaveGame;
class CReplay;
class CIncomingJoinPlayer;
class CIncomingAction;
class CIncomingChatPlayer;
class CIncomingMapSize;
class CCallableBanUpdate;
class CCallableRunQuery;
class CCallableScoreCheck;
class CIncomingGarenaPlayer;
class CCallableWarnCount;

class CBaseGame
{
public:
	CGHost *m_GHost;

protected:
	CTCPServer *m_Socket;							// listening socket
	CGameProtocol *m_Protocol;						// game protocol
//	vector<CGameSlot> m_Slots;						// vector of slots
	vector<CPotentialPlayer *> m_Potentials;		// vector of potential players (connections that haven't sent a W3GS_REQJOIN packet yet)
//	vector<CGamePlayer *> m_Players;				// vector of players
	vector<CCallableRunQuery *> m_RunQueries;
	vector<CCallableBanUpdate *> m_BanUpdates;
	vector<CCallableScoreCheck *> m_ScoreChecks;
	vector<CCallableWarnCount *> m_WarnCounts;
	queue<CIncomingAction *> m_Actions;				// queue of actions to be sent
	vector<string> m_Reserved;						// vector of player names with reserved slots (from the !hold command)
	vector<unsigned char> m_ReservedS;				// vector of slot nr for the reserved players
	vector<uint32_t> m_AutoWarnMarks;				// vector of minutemarks for detecting early leavers
	set<string> m_IPBlackList;						// set of IP addresses to blacklist from joining (todotodo: convert to uint32's for efficiency)
	vector<CGameSlot> m_EnforceSlots;				// vector of slots to force players to use (used with saved games)
	vector<PIDPlayer> m_EnforcePlayers;				// vector of pids to force players to use (used with saved games)
	vector<string> m_GameNames;
	CMap *m_Map;									// map data (this is a pointer to global data)
	CSaveGame *m_SaveGame;							// savegame data (this is a pointer to global data)
	CReplay *m_Replay;								// replay
	bool m_Exiting;									// set to true and this class will be deleted next update
	bool m_Saving;									// if we're currently saving game data to the database
	uint16_t m_HostPort;							// the port to host games on
	unsigned char m_GameState;						// game state, public or private
	unsigned char m_VirtualHostPID;					// virtual host's PID
	vector<unsigned char> m_FakePlayers;			// the fake player's PIDs (if present)	
	unsigned char m_WTVPlayerPID;					// the WaaaghTV player's PID (if present)
	unsigned char m_GProxyEmptyActions;
	string m_GameName;								// game name
	string m_LastGameName;							// last game name (the previous game name before it was rehosted)
	string m_OriginalGameName;						// game name
	string m_VirtualHostName;						// virtual host's name
	string m_OwnerName;								// name of the player who owns this game (should be considered an admin)
	string m_DefaultOwner;								// name of the player who owns this game (should be considered an admin)
	string m_CreatorName;							// name of the player who created this game
	string m_CreatorServer;							// battle.net server the player who created this game was on
	string m_AnnounceMessage;						// a message to be sent every m_AnnounceInterval seconds
	string m_StatString;							// the stat string when the game started (used when saving replays)
	string m_RmkVotePlayer;							// the player who started the rmk vote
	string m_KickVotePlayer;						// the player to be kicked with the currently running kick vote
	string m_HCLCommandString;						// the "HostBot Command Library" command string, used to pass a limited amount of data to specially designed maps
	uint32_t m_RandomSeed;							// the random seed sent to the Warcraft III clients
	uint32_t m_HostCounter;							// a unique game number
	uint32_t m_EntryKey;							// random entry key for LAN, used to prove that a player is actually joining from LAN
	uint32_t m_Latency;								// the number of ms to wait between sending action packets (we queue any received during this time)
	uint32_t m_DynamicLatency;
	uint32_t m_LastDynamicLatencyTicks;
	uint32_t m_LastAdminJoinAndFullTicks;
	uint32_t m_MaxSync;
	string m_MaxSyncUser;
	uint32_t m_SyncLimit;							// the maximum number of packets a player can fall out of sync before starting the lag screen
	uint32_t m_SyncCounter;							// the number of actions sent so far (for determining if anyone is lagging)
	uint32_t m_MaxSyncCounter;						// the largest number of keepalives received from any one player (for determining if anyone is lagging)
	uint32_t m_GameTicks;							// ingame ticks
	uint32_t m_CreationTime;						// GetTime when the game was created
	uint32_t m_LastPingTime;						// GetTime when the last ping was sent
	uint32_t m_LastRefreshTime;						// GetTime when the last game refresh was sent
	uint32_t m_LastDownloadTicks;					// GetTicks when the last map download cycle was performed
	uint32_t m_DownloadCounter;						// # of map bytes downloaded in the last second
	uint32_t m_LastDownloadCounterResetTicks;		// GetTicks when the download counter was last reset
	uint32_t m_LastAnnounceTime;					// GetTime when the last announce message was sent
	uint32_t m_AnnounceInterval;					// how many seconds to wait between sending the m_AnnounceMessage
	uint32_t m_LastAutoStartTime;					// the last time we tried to auto start the game
	uint32_t m_AutoStartPlayers;					// auto start the game when there are this many players or more
	uint32_t m_LastCountDownTicks;					// GetTicks when the last countdown message was sent
	uint32_t m_CountDownCounter;					// the countdown is finished when this reaches zero
//	uint32_t m_StartedLoadingTicks;					// GetTicks when the game started loading
	uint32_t m_StartPlayers;						// number of players when the game started
//	uint32_t m_LastLoadInGameResetTime;				// GetTime when the "lag" screen was last reset when using load-in-game
	uint32_t m_LastActionSentTicks;					// GetTicks when the last action packet was sent
	uint32_t m_LastActionLateBy;					// the number of ticks we were late sending the last action packet by
	uint32_t m_LastActionLateTicks;
	uint32_t m_StartedLaggingTime;					// GetTime when the last lag screen started
	uint32_t m_LastLagScreenTime;					// GetTime when the last lag screen was active (continuously updated)
	uint32_t m_LastReservedSeen;					// GetTime when the last reserved player was seen in the lobby
	uint32_t m_StartedRmkVoteTime;					// GetTime when the rmk vote was started
	uint32_t m_StartedKickVoteTime;					// GetTime when the kick vote was started
	uint32_t m_StartedVoteStartTime;// GetTime when the votestart was started
	uint32_t m_GameOverTime;						// GetTime when the game was over
	uint32_t m_LastPlayerLeaveTicks;				// GetTicks when the most recent player left the game
	uint32_t m_ActualyPrintPlayerWaitingStartDelay; // Counts the number of checks before the waiting for x players before start get printed again	
	double m_MinimumScore;							// the minimum allowed score for matchmaking mode
	double m_MaximumScore;							// the maximum allowed score for matchmaking mode
	bool m_SlotInfoChanged;							// if the slot info has changed and hasn't been sent to the players yet (optimization)
	bool m_DisableStats;
	uint32_t m_GameLoadedTime;					// GetTime when the game was loaded
	bool m_GameLoadedMessage;					// GameLoad message shown
	bool m_AllPlayersWarnChecked;				// iff true, all players have been warn checked and informed already.
	uint32_t m_LastWarnCheck;
	uint32_t m_DownloadInfoTime;                // GetTime when we last showed download info
	uint32_t m_EndGameTime;                     // GetTime when we issued !rehost
	uint32_t m_SwitchTime;						// GetTime when someone issued -switch
	bool m_Switched;							// if someone has switched
	uint32_t m_SwitchNr;						// How many have accepted
	unsigned char m_SwitchS;					// Who is switching
	unsigned char m_SwitchT;					// Target of the switch
	uint32_t m_LagScreenTime;				    // GetTime when the last lag screen was activated
	string m_AnnouncedPlayer;
//	uint32_t m_StartedLoadingTime;				// GetTime when the game started loading
	uint32_t m_LastDLCounterTicks;				// GetTicks when the DL counter was last reset
	uint32_t m_DLCounter;						// how many map download bytes have been sent since the last reset (for limiting download speed)
	bool m_RefreshCompleted;					// if the second half of the refresh sequence has been completed or not
	bool m_GameEnded;
	uint32_t m_GameEndedTime;
	bool m_Locked;									// if the game owner is the only one allowed to run game commands or not
	bool m_RefreshMessages;							// if we should display "game refreshed..." messages or not
	bool m_RefreshError;							// if there was an error refreshing the game
	bool m_RefreshRehosted;							// if we just rehosted and are waiting for confirmation that it was successful
	bool m_MuteAll;									// if we should stop forwarding ingame chat messages targeted for all players or not
	bool m_MuteLobby;								// if we should stop forwarding lobby chat messages
	bool m_CountDownStarted;						// if the game start countdown has started or not
	bool m_StartVoteStarted;
//	bool m_UnhostAllow;
//	bool m_GameLoading;								// if the game is currently loading or not
	bool m_GameLoaded;								// if the game has loaded or not
	bool m_LoadInGame;								// if the load-in-game feature is enabled or not
	bool m_Desynced;								// if the game has desynced or not
	bool m_Lagging;									// if the lag screen is active or not
	bool m_AutoSave;								// if we should auto save the game before someone disconnects
	bool m_MatchMaking;								// if matchmaking mode is enabled
	bool m_LocalAdminMessages;						// if local admin messages should be relayed or not
	uint32_t m_LastInfoShow;
	uint32_t m_LastGenInfoShow;
	uint32_t m_LastOwnerInfoShow;
	bool m_DoAutoWarns;								// enable automated warns for early leavers

public:
	vector<CGamePlayer *> m_Players;			// vector of players
	vector<CDBGamePlayer *> m_DBGamePlayers;	// vector of potential gameplayer data for the database
	vector<CDBBan *> m_DBBans;					// vector of potential ban data for the database (see the Update function for more info, it's not as straightforward as you might think)
	CDBBan * m_DBBanLast;						// last ban for the !banlast command - this is a pointer to one of the items in m_DBBans
	vector<CGameSlot> m_Slots;					// vector of slots
	vector<string> m_AutoBanTemp;
	bool m_CreatorAsFriend;
	bool m_GameLoading;								// if the game is currently loading or not
	bool m_ShowRealSlotCount;
	bool m_Bans;
	uint32_t m_UseDynamicLatency;
	uint32_t m_StartedLoadingTicks;					// GetTicks when the game started loading
	uint32_t m_StartedLoadingTime;				// GetTime when the game started loading
	uint32_t m_LastLoadInGameResetTime;				// GetTime when the "lag" screen was last reset when using load-in-game
	uint32_t m_LastLagScreenResetTime;
	int wtvprocessid;
	uint32_t m_PlayersatStart;					// how many players were at game start
	bool m_GameEndCountDownStarted;				// if the game end countdown has started or not
	uint32_t m_GameEndCountDownCounter;			// the countdown is finished when this reaches zero
	uint32_t m_GameEndLastCountDownTicks;		// GetTicks when the last countdown message was sent
	uint32_t m_Team1;							// Players in team 1
	uint32_t m_Team2;							// Players in team 2
	uint32_t m_Team3;							// Players in team 3
	uint32_t m_Team4;							// Players in team 4
	uint32_t m_TeamDiff;						// Difference between teams (in players number)
	unsigned char m_GameStateS;
	bool m_BanOn;								// If we ban the player or not
	uint32_t m_PlayersLeft;						// Number of players that have left.
	bool m_EvenPlayeredTeams;					// If the the teams are even playered or not
	bool m_ProviderCheck;						// if we allow only some providers
	bool m_ProviderCheck2;						// if we deny some providers
	bool m_CountryCheck;						// if we allow only some countries
	bool m_CountryCheck2;						// if we deny some countries
	bool m_ScoreCheck;							// if we allow some scores only
	bool m_ScoreCheckChecked;
	double m_ScoreCheckScore;
	uint32_t m_ScoreCheckRank;
	bool m_GarenaOnly;							// only allow GArena
	bool m_LocalOnly;							// only allow Local
	bool m_GameOverCanceled;					// cancel game over timer
	bool m_GameOverDiffCanceled;				// disable game over on team difference
	vector <string> m_Muted;					// muted player list
	vector <string> m_CensorMuted;				// muted player list
	vector <uint32_t> m_CensorMutedTime;		// muted player list
	vector <uint32_t> m_CensorMutedSeconds;
	uint32_t m_CensorMutedLastTime;
	bool m_DetourAllMessagesToAdmins;
	bool m_NormalCountdown;
	uint32_t m_LastPlayerWarningTicks;
	bool m_HCL;
	bool m_Listen;								// are we listening?
	bool m_RootListen;
	bool m_Rehost;
	bool m_OwnerJoined;
	string m_ListenUser;						// who is listening?
	double m_Scores;							// what score is allowed
	string m_Countries;							// what countries are allowed
	string m_Countries2;						// what countries are denied
	string m_Providers;							// what providers are allowed
	string m_Providers2;						// what providers are denied
	string m_ShowScoreOf;
	string m_ShowNoteOf;
	uint32_t m_LastSlotsUnoccupied;				// how many slots are not occupied
	bool m_AllSlotsOccupied;					// if we're full
	bool m_AllSlotsAnnounced;					// announced or not
	bool m_DownloadOnlyMode;					// ignore lobby time limit, kick people having the map
	unsigned char m_LastPlayerJoined;           // last player who joined
	bool m_PingsUpdated;
	uint32_t m_LastMars;						// time when last mars was issued
	uint32_t m_LastPlayerJoinedTime;			// time when last player joined
	uint32_t m_LastPlayerJoiningTime;			// time when last player tried to join
	uint32_t m_SlotsOccupiedTime;				// GetTime when we got full.
	uint32_t m_LastLeaverTicks;                 // GetTicks of the laster leaver
	string m_Server;							// Server we're on
	bool m_EndRequested;
	unsigned char m_EndRequestedTeam;
	uint32_t m_EndRequestedTicks;
	bool m_autohosted;
	string m_GetMapType;
	string m_GetMapPath;
	uint32_t m_GetMapNumPlayers;
	uint32_t m_GetMapNumTeams;
	unsigned char m_GetMapGameType;
	uint32_t m_GetMapOnlyAutoWarnIfMoreThanXPlayers;
	CBaseGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer );
	virtual ~CBaseGame( );

	virtual vector<CGameSlot> GetEnforceSlots( )	{ return m_EnforceSlots; }
	virtual vector<PIDPlayer> GetEnforcePlayers( )	{ return m_EnforcePlayers; }
	virtual CSaveGame *GetSaveGame( )				{ return m_SaveGame; }
	virtual uint16_t GetHostPort( )					{ return m_HostPort; }
	virtual unsigned char GetGameState( )			{ return m_GameState; }
	virtual unsigned char GetGProxyEmptyActions( )	{ return m_GProxyEmptyActions; }
	virtual string GetGameName( )					{ return m_GameName; }
	virtual string GetMapName( );
	virtual string GetLastGameName( )				{ return m_LastGameName; }
	virtual void SetGameName( string nGameName)		{ m_GameName = nGameName; }
	virtual void SetHostCounter( uint32_t nHostCounter)			{ m_HostCounter = nHostCounter; }
	virtual void SetLastRefreshTime( uint32_t nLastRefreshTime)	{ m_LastRefreshTime = nLastRefreshTime; }
	virtual void SetCreationTime( uint32_t nCreationTime)		{ m_CreationTime = nCreationTime; }
	virtual void SetHCL( string nHCL)				{ m_HCLCommandString = nHCL; }
	virtual void SetRehost( bool nRehost)			{ m_Rehost = nRehost; }
	virtual void SetAutoHosted( bool nautohosted ) 	{ m_autohosted = nautohosted; }
	virtual bool GetAutoHosted( )					{ return m_autohosted; }
	virtual uint32_t GetGameLoadedTime ()			{ return m_GameLoadedTime; }
	virtual uint32_t GetGameNr( );
	virtual string GetOriginalName( )				{ return m_OriginalGameName; }
	virtual string GetVirtualHostName( )			{ return m_VirtualHostName; }
	virtual string GetWTVPlayerName( )				{ return m_GHost->m_wtvPlayerName; }
	virtual string GetOwnerName( )					{ return m_OwnerName; }
	virtual string GetCreatorName( )				{ return m_CreatorName; }
	virtual string GetHCL( )						{ return m_HCLCommandString; }
	virtual uint32_t GetCreationTime( )				{ return m_CreationTime; }
	virtual string GetCreatorServer( )				{ return m_CreatorServer; }
	virtual uint32_t GetHostCounter( )				{ return m_HostCounter; }
	virtual uint32_t GetLastLagScreenTime( )		{ return m_LastLagScreenTime; }
	virtual bool GetLocked( )						{ return m_Locked; }
	virtual bool GetRefreshMessages( )				{ return m_RefreshMessages; }
	virtual bool GetCountDownStarted( )				{ return m_CountDownStarted; }
	virtual bool GetGameLoading( )					{ return m_GameLoading; }
	virtual bool GetGameLoaded( )					{ return m_GameLoaded; }
	virtual bool GetLagging( )						{ return m_Lagging; }
	virtual vector<CGamePlayer*> GetPlayers ( ) 	{ return m_Players; }

	virtual void SetEnforceSlots( vector<CGameSlot> nEnforceSlots )		{ m_EnforceSlots = nEnforceSlots; }
	virtual void SetEnforcePlayers( vector<PIDPlayer> nEnforcePlayers )	{ m_EnforcePlayers = nEnforcePlayers; }
	virtual void SetExiting( bool nExiting )							{ m_Exiting = nExiting; }
	virtual void SetAutoStartPlayers( uint32_t nAutoStartPlayers )		{ m_AutoStartPlayers = nAutoStartPlayers; }
	virtual void SetMinimumScore( double nMinimumScore )				{ m_MinimumScore = nMinimumScore; }
	virtual void SetMaximumScore( double nMaximumScore )				{ m_MaximumScore = nMaximumScore; }
	virtual void SetRefreshError( bool nRefreshError )					{ m_RefreshError = nRefreshError; }
	virtual void SetMatchMaking( bool nMatchMaking )					{ m_MatchMaking = nMatchMaking; }

	virtual uint32_t GetNextTimedActionTicks( );
	virtual uint32_t GetSlotsOccupied( );
	virtual uint32_t GetNumSlotsT1( );
	virtual uint32_t GetNumSlotsT2( );
	virtual uint32_t GetSlotsOccupiedT1( );
	virtual uint32_t GetSlotsOccupiedT2( );
	virtual uint32_t GetSlotsOpen( );
	virtual uint32_t GetSlotsOpenT1( );
	virtual uint32_t GetSlotsOpenT2( );
	virtual uint32_t GetNumPlayers( );
	virtual uint32_t GetNumFakePlayers( );
	virtual uint32_t GetNumHumanPlayers( );
	virtual string GetDescription( );

	virtual void SetAnnounce( uint32_t interval, string message );

	// processing functions

	virtual unsigned int SetFD( void *fd, void *send_fd, int *nfds );
	virtual bool Update( void *fd, void *send_fd );
	virtual void UpdatePost( void *send_fd );

	// generic functions to send packets to players

	virtual void Send( CGamePlayer *player, BYTEARRAY data );
	virtual void Send( unsigned char PID, BYTEARRAY data );
	virtual void Send( BYTEARRAY PIDs, BYTEARRAY data );
	virtual void SendAll( BYTEARRAY data );
	virtual void SendAlly( unsigned char PID, BYTEARRAY data );
	virtual void SendAdmin( unsigned char PID, BYTEARRAY data );
	virtual void SendEnemy( unsigned char PID, BYTEARRAY data );
	virtual void SendAdmin( BYTEARRAY data );

	// functions to send packets to players

	virtual void SendChat( unsigned char fromPID, CGamePlayer *player, string message );
	virtual void SendChat( unsigned char fromPID, unsigned char toPID, string message );
	virtual void SendChat( CGamePlayer *player, string message );
	virtual void SendChat( unsigned char toPID, string message );
	virtual void SendAllChat( unsigned char fromPID, string message );
	virtual void SendAllyChat( unsigned char fromPID, string message );
	virtual void SendAdminChat( unsigned char fromPID, string message );
//	virtual void SendEnemyChat( unsigned char fromPID, string message );
	virtual void SendAllChat( string message );
	virtual void SendLocalAdminChat( string message );
	virtual void SendAllyChat( string message );
	virtual void SendAdminChat( string message );
	virtual void SendAllSlotInfo( );
	virtual void SendVirtualHostPlayerInfo( CGamePlayer *player );
	virtual void SendFakePlayerInfo( CGamePlayer *player );
	virtual void SendWTVPlayerInfo( CGamePlayer *player );
	virtual void SendAllActions( );
	virtual void SendWelcomeMessage( CGamePlayer *player );
	virtual void SendEndMessage( );

	// events
	// note: these are only called while iterating through the m_Potentials or m_Players vectors
	// therefore you can't modify those vectors and must use the player's m_DeleteMe member to flag for deletion

	virtual void EventPlayerDeleted( CGamePlayer *player );
	virtual void EventPlayerDisconnectTimedOut( CGamePlayer *player );
	virtual void EventPlayerDisconnectPlayerError( CGamePlayer *player );
	virtual void EventPlayerDisconnectSocketError( CGamePlayer *player );
	virtual void EventPlayerDisconnectConnectionClosed( CGamePlayer *player );
	virtual void EventPlayerJoined( CPotentialPlayer *potential, CIncomingJoinPlayer *joinPlayer );
	virtual void EventPlayerJoinedWithScore( CPotentialPlayer *potential, CIncomingJoinPlayer *joinPlayer, double score );
	virtual void EventPlayerLeft( CGamePlayer *player, uint32_t reason );
	virtual void EventPlayerLoaded( CGamePlayer *player );
	virtual void EventPlayerAction( CGamePlayer *player, CIncomingAction *action );
	virtual void EventPlayerKeepAlive( CGamePlayer *player, uint32_t checkSum );
	virtual void EventPlayerChatToHost( CGamePlayer *player, CIncomingChatPlayer *chatPlayer );
	virtual bool EventPlayerBotCommand( CGamePlayer *player, string command, string payload );
	virtual void EventPlayerChangeTeam( CGamePlayer *player, unsigned char team );
	virtual void EventPlayerChangeColour( CGamePlayer *player, unsigned char colour );
	virtual void EventPlayerChangeRace( CGamePlayer *player, unsigned char race );
	virtual void EventPlayerChangeHandicap( CGamePlayer *player, unsigned char handicap );
	virtual void EventPlayerDropRequest( CGamePlayer *player );
	virtual void EventPlayerMapSize( CGamePlayer *player, CIncomingMapSize *mapSize );
	virtual void EventPlayerPongToHost( CGamePlayer *player, uint32_t pong );

	// these events are called outside of any iterations

	virtual void EventGameStarted( );
	virtual void EventGameLoaded( );

	// other functions

	virtual unsigned char GetSIDFromPID( unsigned char PID );
	virtual CGamePlayer *GetPlayerFromPID( unsigned char PID );
	virtual CGamePlayer *GetPlayerFromSID( unsigned char SID );
	virtual CGamePlayer *GetPlayerFromName( string name, bool sensitive );
	virtual uint32_t GetPlayerFromNamePartial( string name, CGamePlayer **player );
	virtual CGamePlayer *GetPlayerFromColour( unsigned char colour );
	virtual string GetPlayerList( );
	virtual unsigned char GetNewPID( );
	virtual unsigned char GetNewColour( );
	virtual BYTEARRAY GetPIDs( );
	virtual BYTEARRAY GetAllyPIDs( unsigned char PID);
	virtual BYTEARRAY GetPIDs( unsigned char excludePID );
	virtual BYTEARRAY GetAdminPIDs( );
	virtual BYTEARRAY GetRootAdminPIDs( );
	virtual BYTEARRAY GetRootAdminPIDs( unsigned char omitpid);
	virtual BYTEARRAY AddRootAdminPIDs( BYTEARRAY PIDs, unsigned char omitpid);
	virtual BYTEARRAY Silence(BYTEARRAY PIDs);
	virtual unsigned char GetHostPID( );
	virtual unsigned char GetEmptySlot( bool reserved );
	virtual unsigned char GetEmptySlotForFakePlayers( );
	virtual unsigned char GetEmptySlotAdmin( bool reserved );
	virtual unsigned char GetEmptySlot( unsigned char team, unsigned char PID );
	virtual void SwapSlots( unsigned char SID1, unsigned char SID2 );
	virtual void OpenSlot( unsigned char SID, bool kick );
	virtual void CloseSlot( unsigned char SID, bool kick );
	virtual void ComputerSlot( unsigned char SID, unsigned char skill, bool kick );
	virtual void ColourSlot( unsigned char SID, unsigned char colour );
	virtual void OpenAllSlots( );
	virtual void CloseAllSlots( );
	virtual void ShuffleSlots( );
	virtual vector<unsigned char> BalanceSlotsRecursive( vector<unsigned char> PlayerIDs, unsigned char *TeamSizes, double *PlayerScores, unsigned char StartTeam );
	virtual void BalanceSlots( );
	virtual void AddToSpoofed( string server, string name, bool sendMessage );
	virtual void AddToReserved( string name, unsigned char nr );
	virtual void DelFromReserved( string name );
	virtual void AddGameName( string name);
	virtual void AutoSetHCL ( );
	virtual bool IsGameName( string name );
//	virtual void AddToReserved( string name );
	virtual bool IsOwner( string name );
	virtual bool IsReserved( string name );
	virtual bool IsDownloading( );
	virtual bool IsGameDataSaved( );
	virtual void SaveGameData( );
	virtual void StartCountDown( bool force );
	virtual void StartCountDownAuto( bool requireSpoofChecks );
	virtual void StopPlayers( string reason );
	virtual void StopLaggers( string reason );
	virtual void CreateVirtualHost( );
	virtual void DeleteVirtualHost( );
	virtual void CreateFakePlayer( );
//	virtual void CreateInitialFakePlayers( uint32_t m );
	virtual void CreateInitialFakePlayers( );
	virtual void DeleteFakePlayer( );
	virtual void DeleteAFakePlayer( );
	virtual void CreateWTVPlayer( string name="Waaagh!TV", bool lobbyhost=false );
	virtual void DeleteWTVPlayer( );
	virtual bool IsMuted ( string name);
	virtual void AddToMuted( string name);
	virtual void AddToCensorMuted( string name);
	virtual uint32_t IsCensorMuted ( string name);
	virtual void ProcessCensorMuted( );
	virtual void DelFromMuted( string name);
	virtual unsigned char GetTeam ( unsigned char SID);
	virtual void AddToListen( unsigned char user);
	virtual void DelFromListen( unsigned char user);
	virtual bool IsListening ( unsigned char user);
	virtual void ReCalculateTeams( );
	virtual void InitSwitching ( unsigned char PID, string nr);
	virtual void SwitchAccept ( unsigned char PID);
	virtual void SwitchDeny ( unsigned char PID);
	virtual unsigned char ReservedSlot(string name);
	virtual void ResetReservedSlot(string name);
	virtual uint32_t CountDownloading( );
	virtual uint32_t ShowDownloading( );
	virtual uint32_t DownloaderNr( string name );
	virtual void SwapSlotsS( unsigned char SID1, unsigned char SID2 );
	virtual bool IsAutoBanned( string name );
	virtual bool IsAdmin( string name );
	virtual bool IsSafe( string name );
	virtual string Voucher( string name );
	virtual void AddNote( string name, string note );
	virtual bool IsNoted( string name );
	virtual string Note( string name );
	virtual bool IsRootAdmin( string name );
	virtual vector<CDBGamePlayer *> GamePlayers() { return m_DBGamePlayers; }
	virtual string CustomReason( string reason, string name );
	virtual string CustomReason( uint32_t ctime, string reason, string name);
	virtual string GetGameInfo( );
	virtual void SetDynamicLatency( );

};

#endif
