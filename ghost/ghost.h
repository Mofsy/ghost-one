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

#ifndef GHOST_H
#define GHOST_H

#include "includes.h"
#include "config.h"
#include "ghostdb.h"

//
// CGHost
//

class CUDPSocket;
class CTCPServer;
class CTCPSocket;
class CGPSProtocol;
class CGCBIProtocol;
class CCRC32;
class CSHA1;
class CBNET;
class CBaseGame;
class CAdminGame;
class CGHostDB;
class CBaseCallable;
class CLanguage;
class CMap;
class CSaveGame;
//UDPCommandSocket patch
class CUDPServer;
class CConfig;
class CCallableGameUpdate;
struct DenyInfo;

class CMyCallableDownloadFile : public CBaseCallable
{
protected:
	string m_File;
	string m_Path;
	uint32_t m_Result;
public:
	CMyCallableDownloadFile( string nFile, string nPath ) : CBaseCallable( ), m_File( nFile ), m_Path( nPath ), m_Result( 0 ) { }
	virtual ~CMyCallableDownloadFile( ) { }
	virtual void operator( )( );
	virtual string GetFile( )					{ return m_File; }
	virtual string GetPath( )					{ return m_Path; }
	virtual uint32_t GetResult( )				{ return m_Result; }
	virtual void SetResult( uint32_t nResult )	{ m_Result = nResult; }
};
class CGHost
{
public:
	CUDPSocket *m_UDPSocket;				// a UDP socket for sending broadcasts and other junk (used with !sendlan)
	CTCPServer *m_ReconnectSocket;			// listening socket for GProxy++ reliable reconnects
	vector<CTCPSocket *> m_ReconnectSockets;// vector of sockets attempting to reconnect (connected but not identified yet)
	CGPSProtocol *m_GPSProtocol;
	CGCBIProtocol *m_GCBIProtocol;
	CCRC32 *m_CRC;							// for calculating CRC's
	CSHA1 *m_SHA;							// for calculating SHA1's
	vector<CBNET *> m_BNETs;				// all our battle.net connections (there can be more than one)
	CBaseGame *m_CurrentGame;				// this game is still in the lobby state
	CAdminGame *m_AdminGame;				// this "fake game" allows an admin who knows the password to control the bot from the local network
	vector<CBaseGame *> m_Games;			// these games are in progress
	CGHostDB *m_DB;							// database
	CGHostDB *m_DBLocal;					// local database (for temporary data)
	vector<CBaseCallable *> m_Callables;	// vector of orphaned callables waiting to die
	vector<BYTEARRAY> m_LocalAddresses;		// vector of local IP addresses
	map<string, DenyInfo> m_DenyIP;			// map (IP -> DenyInfo) of denied IP addresses
	CLanguage *m_Language;					// language
	CMap *m_Map;							// the currently loaded map
	CMap *m_AdminMap;						// the map to use in the admin game
	vector<CMap*> m_AutoHostMap;			// the maps to use when autohosting
	uint32_t m_AutoHostMapCounter;			// counter determining which autohost map should be hosted next
	CSaveGame *m_SaveGame;					// the save game to use
	vector<PIDPlayer> m_EnforcePlayers;		// vector of pids to force players to use in the next game (used with saved games)
	bool m_Exiting;							// set to true to force ghost to shutdown next update (used by SignalCatcher)
	bool m_ExitingNice;						// set to true to force ghost to disconnect from all battle.net connections and wait for all games to finish before shutting down
	bool m_Enabled;							// set to false to prevent new games from being created
	string m_Version;						// GHost++ version string
	string m_GHostVersion;					// GHost++ version string
	vector<string> providersn;
	vector<string> providers;
	uint32_t m_ScoresCount;
	bool m_ScoresCountSet;
	bool m_AutoHostLocal;
	string sPathEnd;
	uint32_t m_HostCounter;					// the current host counter (a unique number to identify a game, incremented each time a game is created)
	uint32_t m_MaxHostCounter;
	CMyCallableDownloadFile *m_CallableDownloadFile;
	bool m_spamwhispers;
	bool m_channeljoingreets;
	bool m_AdminsAndSafeCanDownload;
	bool m_channeljoinmessage;
	vector<string> m_channeljoinex;
	string m_channeljoinexceptions;
	bool m_broadcastinlan;
	bool m_sendrealslotcounttogproxy;
	bool m_onlyownerscanstart;
	vector<string> m_WarnForgetQueue;
	uint32_t m_MySQLQueueTicks;
	string m_AllowedCountries;
	string m_DeniedCountries;
	string m_OneVersion;
	string m_RootAdmin;
	bool m_AutoBan;							// if we have auto ban on by default or not				
	uint32_t m_AutoBanTeamDiffMax;			// if we have more then x number of players more then other team
	uint32_t m_AutoBanTimer;				// time in mins the auto ban will stay on in game.
	bool m_AutoBanAll;						// ban even if it does not make game uneven
	uint32_t m_AutoBanFirstXLeavers;			// bans the first x leavers reguardless of even or not.
	uint32_t m_AutoBanGameLoading;			// Ban if leave during loading
	uint32_t m_AutoBanCountDown;			// Ban if leave during game start countdown.
	uint32_t m_AutoBanGameEndMins;			// Ban if not left around x mins of game end time.
	uint32_t m_GUIPort;
	string m_CensorWords;
	bool m_CensorMute;
	bool m_CensorMuteAdmins;
	uint32_t m_CensorMuteFirstSeconds;
	uint32_t m_CensorMuteSecondSeconds;
	uint32_t m_CensorMuteExcessiveSeconds;
	vector<string> m_CensoredWords;
	bool m_ShuffleSlotsOnStart;
	bool m_ShowCountryNotAllowed;
	bool m_ShowScoresOnJoin;
	bool m_ShowNotesOnJoin;
	uint32_t m_AutoRehostDelay;
	bool m_RehostIfNameTaken;
	uint32_t m_OwnerAccess;
	uint32_t m_AdminAccess;
	int32_t m_ReplayTimeShift;
	string m_bnetpacketdelaymediumpvpgn;
	string m_bnetpacketdelaybigpvpgn;
	string m_bnetpacketdelaymedium;
	string m_bnetpacketdelaybig;
	string m_ScoreFormula;					// score formula, config value
	string m_ScoreMinGames;					// score min games, config value
	string m_AutoHostGameName;				// the base game name to auto host with
	string m_AutoHostMapCFG;				// the map config to auto host with
	string m_AutoHostOwner;
	string m_AutoHostServer;
	uint32_t m_AutoHostMaximumGames;		// maximum number of games to auto host	
	uint32_t m_AutoHostAutoStartPlayers;	// when using auto hosting auto start the game when this many players have joined
	uint32_t m_BotAutoStartPlayers;			// use this variable to reset the number of ppls needed for autostart in autohosting game
	uint32_t m_LastAutoHostTime;			// GetTime when the last auto host was attempted
	uint32_t m_LastGameUpdateTime;      	// GetTime when the gamelist was last updated
	CCallableGameUpdate *m_CallableGameUpdate;// threaded database game update in progress
	bool m_AutoHostMatchMaking;
	double m_AutoHostMinimumScore;
	double m_AutoHostMaximumScore;
	bool m_AllGamesFinished;				// if all games finished (used when exiting nicely)
	uint32_t m_AllGamesFinishedTime;		// GetTime when all games finished (used when exiting nicely)
	string m_LanguageFile;					// config value: language file
	string m_Warcraft3Path;					// config value: Warcraft 3 path
	bool m_TFT;								// config value: TFT enabled or not
	string m_BindAddress;					// config value: the address to host games on
	uint16_t m_HostPort;					// config value: the port to host games on
	uint16_t m_BroadCastPort;					// config value: the port to host games on
	bool m_Reconnect;						// config value: GProxy++ reliable reconnects enabled or not
	uint16_t m_ReconnectPort;				// config value: the port to listen for GProxy++ reliable reconnects on
	uint32_t m_ReconnectWaitTime;			// config value: the maximum number of minutes to wait for a GProxy++ reliable reconnect
	uint32_t m_MaxGames;					// config value: maximum number of games in progress
	char m_CommandTrigger;					// config value: the command trigger inside games
	string m_MapCFGPath;					// config value: map cfg path
	string m_SaveGamePath;					// config value: savegame path
	string m_MapPath;						// config value: map path
	bool m_SaveReplays;						// config value: save replays
	string m_ReplayPath;					// config value: replay path
	string m_VirtualHostName;				// config value: virtual host name
	string m_BlacklistedNames;				// config value: blacklisted names
	bool m_HideIPAddresses;					// config value: hide IP addresses from players
	bool m_CheckMultipleIPUsage;			// config value: check for multiple IP address usage
	uint32_t m_SpoofChecks;					// config value: do automatic spoof checks or not
	uint32_t m_NewOwner;					// config value: new owner in game
	bool m_RequireSpoofChecks;				// config value: require spoof checks or not
	bool m_RefreshMessages;					// config value: display refresh messages or not (by default)
	bool m_AutoLock;						// config value: auto lock games when the owner is present
	bool m_AutoSave;						// config value: auto save before someone disconnects
	uint32_t m_AllowDownloads;				// config value: allow map downloads or not
	bool m_PingDuringDownloads;				// config value: ping during map downloads or not
	bool m_LCPings;							// config value: use LC style pings (divide actual pings by two)
	uint32_t m_DropVoteTime;       			// config value: accept drop votes after this amount of seconds
	uint32_t m_AutoKickPing;				// config value: auto kick players with ping higher than this
	uint32_t m_BanMethod;					// config value: ban method (ban by name/ip/both)
	string m_IPBlackListFile;				// config value: IP blacklist file (ipblacklist.txt)
	uint32_t m_Latency;						// config value: the latency (by default)
	uint32_t m_SyncLimit;					// config value: the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
	bool m_VoteStartAllowed;    			// config value: if votestarts are allowed or not
	bool m_VoteStartAutohostOnly;           // config value: if votestarts are only allowed in autohosted games
	uint32_t m_VoteStartMinPlayers;         // config value: minimum number of players before users can !votestart
	bool m_VoteKickAllowed;					// config value: if votekicks are allowed or not
	uint32_t m_VoteKickPercentage;			// config value: percentage of players required to vote yes for a votekick to pass
	string m_DefaultMap;					// config value: default map (map.cfg)
	string m_MOTDFile;						// config value: motd.txt
	string m_GameLoadedFile;				// config value: gameloaded.txt
	string m_GameOverFile;					// config value: gameover.txt
	bool m_LocalAdminMessages;				// config value: send local admin messages or not
	bool m_AdminMessages;					// config value: send admin messages or not
	bool m_AdminGameCreate;					// config value: create the admin game or not
	uint16_t m_AdminGamePort;				// config value: the port to host the admin game on
	string m_AdminGamePassword;				// config value: the admin game password
	string m_AdminGameMap;					// config value: the admin game map config to use
	unsigned char m_LANWar3Version;			// config value: LAN warcraft 3 version
	uint32_t m_ReplayWar3Version;			// config value: replay warcraft 3 version (for saving replays)
	uint32_t m_ReplayBuildNumber;			// config value: replay build number (for saving replays)
	bool m_TCPNoDelay;						// config value: use Nagle's algorithm or not
	uint32_t m_MatchMakingMethod;			// config value: the matchmaking method
	uint32_t m_MapGameType;                 // config value: the MapGameType overwrite (aka: refresh hack)
	
	uint32_t m_DenyMaxDownloadTime;			// config value: the maximum download time in milliseconds
	uint32_t m_DenyMaxMapsizeTime;			// config value: the maximum time in milliseconds to wait for a MAPSIZE packet
	uint32_t m_DenyMaxReqjoinTime;			// config value: the maximum time in milliseconds to wait for a REQJOIN packet
	uint32_t m_DenyMaxIPUsage;				// config value: the maximum number of users from the same IP address to accept
	uint32_t m_DenyMaxLoadTime;				// config value: the maximum time in milliseconds to wait for players to load
	
	uint32_t m_DenyMapsizeDuration;			// config value: time to deny due to no mapsize received
	uint32_t m_DenyDownloadDuration;		// config value: time to deny due to low download speed
	uint32_t m_DenyReqjoinDuration;			// config value: time to deny due to no reqjoin received
	uint32_t m_DenyIPUsageDuration;			// config value: time to deny due to high multiple IP usage
	uint32_t m_DenyLoadDuration;			// config value: time to deny due to no load received
	uint32_t m_ExtendDeny;					// config value: time for the denial to be extended
	uint32_t m_PlayerBeforeStartPrintDelay; // config value: delay * 10s is the time between two WaitingForPlayersBeforeStart prints
	uint32_t m_RehostPrintingDelay;			// config value: delay X times of rehost print should be not sent to prevent spamming in lobby		
	
	uint32_t m_ActualRehostPrintingDelay;   // Counts the number of checks before printing again
	uint32_t m_NumPlayersforAutoStart;  	// store value of the number of players required for an autostart
	uint32_t m_StartGameWhenAtLeastXPlayers;
	uint32_t m_RefreshDuration;
	uint32_t m_MoreFPsLobby;
	uint32_t m_BnetNonAdminCommands;
	bool m_UDPConsole;						// config value: console output redirected to UDP
	bool m_Verbose;							// config value: show all info or just some
	bool m_RelayChatCommands;				// config value: show/hide issued commands
	bool m_BlueCanHCL;	
	bool m_BlueIsOwner;
	bool m_CustomName;
	bool m_AppleIcon;
	bool m_FakePlayersLobby;
	bool m_DenyPatchEnable;
	bool m_PrefixName;
	bool m_SquirrelTxt;
	bool m_ReplaysByName;
	bool m_NoDelMap;
	bool m_NoDLMap;
	bool m_NoMapDLfromEpicwar;
	bool m_OnlyMapDLfromHive;
	bool m_OtherPort;
	string m_InvalidTriggers;
	string m_InvalidReplayChars;
	string m_RehostChar;
	string m_OwnerNameAlias;
	bool m_QueueGameRefresh;
	bool m_VietTxt;
	bool m_EnableUnhost;
	bool m_BlacklistSlowDLder;
	bool m_RejectColoredName;
	bool m_GarenaOnly;
	bool m_BanListLoaded;
	bool m_ReplaceBanWithWarn;
	bool m_forceautohclindota;
	bool m_PlaceAdminsHigherOnlyInDota;
	bool m_AutoStartDotaGames;
	double m_AllowedScores;
	double m_AutoHostAllowedScores;
	bool m_AllowNullScoredPlayers;
	bool m_QuietRehost;
	bool m_CalculatingScores;
	string DBType;
	bool m_UpdateDotaEloAfterGame;
	bool m_UpdateDotaScoreAfterGame;	
	string m_LastGameName;
	string m_ExternalIP;					// our external IP
	uint32_t m_ExternalIPL;					// our external IP long format
	string m_Country;						// our country
	string m_wtvPath;
	string m_wtvPlayerName;
	bool m_wtv;
	bool m_ForceLoadInGame;
	bool m_ShowRealSlotCount;
	vector<string> m_CachedSpoofedIPs;
	vector<string> m_CachedSpoofedNames;
	vector<string> m_Providers;				//
	vector<string> m_Welcome;				// our welcome message
	vector<string> m_ChannelWelcome;		// our welcome message
	vector<string> m_FPNames;				// our fake player names
	vector<string> m_FPNamesLast;			// our last fake player names
	vector<string> m_LanRoomName;			// our lan room name
	vector<string> m_Mars;					// our mars messages
	vector<string> m_MarsLast;				// our last mars messages
	uint32_t m_ChannelJoinTime;				// when we enter a channel
	bool m_NewChannel;            		  	// did we enter a channel
	vector<string> m_UDPUsers;				// our udp users
	vector<string> m_IPUsers;
	string m_LastIp;						// our last udp command is comming from this ip
	uint32_t m_IPBanning;					// config value: handle ip bans
	uint32_t m_Banning;						// config value: handle name bans
	string m_DisableReason;					// reason for disabling the bot
	uint32_t m_BanTheWarnedPlayerQuota;	// number of warns needed to ban the player
	uint32_t m_BanTimeOfWarnedPlayer;	// number of days the tempban from the warns will last
	uint32_t m_TBanLastTime;			// number of days to tempban when tbanlast
	uint32_t m_BanLastTime;				// number of days to tempban when banlast
	uint32_t m_BanTime;					// number of days to tempban when banning
	uint32_t m_WarnTimeOfWarnedPlayer;	// number of days the warn will last
	uint32_t m_GameNumToForgetAWarn;  // number of games till the first of the warns gets forgotten
	bool m_AutoWarnEarlyLeavers;	// config value: warn people who leave the hosted game early
	bool m_SafelistedBanImmunity;
	uint32_t m_GameLoadedPrintout;	// config value: how many secs should Ghost wait to printout the GameLoaded msg
	uint32_t m_InformAboutWarnsPrintout; // config value: how many secs should ghost wait to printout the warn count to each player.

	bool m_LanAdmins;						// config value: LAN people who join the game are considered admins
	bool m_AdminsOnLan;						// config value: admin players who join through LAN can command
	bool m_LanRootAdmins;					// config value: LAN people who join the game are considered rootadmins
	bool m_LocalAdmins;						// config value: Local(localhost or GArena) people who join the game are considered admins
	bool m_KickUnknownFromChannel;			// config value: kick unknown people from channel
	bool m_KickBannedFromChannel;			// config value: kick banned people from channel
	bool m_BanBannedFromChannel;			// config value: ban banned people from channel
	bool m_NonAdminCommands;      			// config value: non admin commands available or not
	bool m_NotifyBannedPlayers;				// config value: send message to banned players that they have been banned
	bool m_FindExternalIP;					// config value: find external IP on startup
	bool m_AltFindIP;						// config value: find external IP with different site
	bool m_RootAdminsSpoofCheck;			// config value: root admins need to spoof check or not.
	bool m_AdminsSpoofCheck;				// config value: admins need to spoof check or not.
	bool m_TwoLinesBanAnnouncement;			// config value: announce bans+reason on two lines, otherwise on one
	bool m_addcreatorasfriendonhost;		// config value: add the creator as friend on hosting a game
	bool m_autohclfromgamename;				// config value: auto set HCL based on gamename, ignore map_defaulthcl
	bool m_norank;
	bool m_nostatsdota; 
	bool m_UsersCanHost;
	bool m_SafeCanHost;
	bool m_Console;
	bool m_Log;
	uint32_t m_gameoverbasefallen;			// config value: initiate game over timer when x seconds have passed since world tree/frozen throne has fallen
	uint32_t m_gameoverminpercent;			// config value: initiate game over timer when percent of people remaining is less than.
	uint32_t m_gameoverminplayers;			// config value: initiate game over timer when there are less than this number of players.
	bool m_gameoveroneplayer;				// config value: initiate game over timer when only 1 player in game.
	uint32_t m_gameovermaxteamdifference;	// config value: initiate game over timer if team unbalance is greater than this.
	string m_CustomVersionText;				// config value: custom text to add to the version
	uint32_t m_totaldownloadspeed;			// config value: total download speed allowed per all clients
	uint32_t m_clientdownloadspeed;			// config value: max download speed per client
	uint32_t m_maxdownloaders;				// config value: max clients allowed to download at once
	uint32_t m_NewRefreshTime;				// config value: send refresh every n seconds
	string m_AutoHostCountries;				// config value: which countries to allow
	string m_AutoHostCountries2;			// config value: which countries to deny
	bool m_AutoHosted;
	bool m_AutoHostAllowStart;				// config value: allow players to start the game
	bool m_AutoHostCountryCheck;			// config value: country check enabled?
	bool m_AutoHostCountryCheck2;			// config value: country check2 enabled?
	bool m_AutoHostGArena;					// config value: only allow GArena
	bool m_patch23;							// config value: use for patch 1.23
	bool m_patch21;							// config value: use for patch 1.21
	bool m_inconsole;
	uint32_t m_gamestateinhouse;
	bool m_SafeLobbyImmunity;
	bool m_DetourAllMessagesToAdmins;
	bool m_NormalCountdown;
	bool m_UseDynamicLatency;
	bool m_DynamicLatency2xPingMax;
	bool m_UnbanRemovesChannelBans;
	bool m_WhisperAllMessages;
	uint32_t m_DynamicLatencyMaxToAdd;
	uint32_t m_DynamicLatencyAddedToPing;
	bool m_DynamicLatencyIncreasewhenLobby;
	uint32_t m_LastDynamicLatencyConsole;
	bool m_AdminsLimitedUnban;
	bool m_AdminsCantUnbanRootadminBans;
	bool m_LobbyAnnounceUnoccupied;
	bool m_EndReq2ndTeamAccept;
	bool m_HideBotFromNormalUsersInGArena;
	bool m_detectwtf;
	bool m_autoinsultlobby;
	bool m_ShowDownloadsInfo;				// config value: show info on downloads in progress
	bool m_Rehosted;
	bool m_Hosted;
	bool m_newLatency;
	bool m_newTimer;
	uint32_t m_newTimerResolution;
	bool m_newTimerStarted;
	string m_RehostedName;
	string m_HostedName;
	string m_RehostedServer;
	uint32_t m_ShowDownloadsInfoTime;
	vector<string> m_Commands;
	string m_RootAdmins;
	string m_AdminsWithUnhost;
	string m_FakePings;
	string m_UDPPassword;
	bool m_onlyownerscanswapadmins;
	bool m_HoldPlayersForRMK;
	string m_PlayersfromRMK;
	bool newGame;
	string newGameUser;
	string newGameServer;
	bool newGameProvidersCheck;
	bool newGameProviders2Check;
	string newGameProviders;
	string newGameProviders2;
	bool newGameCountryCheck;
	bool newGameCountry2Check;
	string newGameCountries;
	string newGameCountries2;
	unsigned char newGameGameState;
	string newGameName;
	bool newGameGArena;
	bool m_LogReduction;
	uint32_t m_LobbyDLLeaverBanTime;
	uint32_t m_DLRateLimit;
	uint32_t m_LobbyTimeLimit;
	uint32_t m_LobbyTimeLimitMax;
//	bool m_dbopen;

	CGHost( CConfig *CFG );
	~CGHost( );

	void UDPChatDel(string ip);
	void UDPChatAdd(string ip);
	bool UDPChatAuth(string ip);
	void UDPChatSendBack(string s);
	void UDPChatSend(string s);
	void UDPChatSend(BYTEARRAY b);
	string UDPChatWhoIs(string c, string s);
	string Commands(unsigned int idx);
	bool CommandAllowedToShow( string c);
	void ReadProviders();
	void ReadWelcome();	
	void ReadChannelWelcome();
	void ReadMars();
	string GetMars();
	void ReadFP();	
	string GetFPName();
	string GetRehostChar();
	void ReadRoomData();
	string GetRoomName(string RoomID);
	void SetTimerResolution();
	void EndTimer();
	void AdminGameMessage(string name, string message);
	void UDPCommands(string Message);
	bool ShouldFakePing(string name);
	bool IsRootAdmin(string name);
	bool IsAdminWithUnhost(string name);
	void AddRootAdmin(string name);
	void DelRootAdmin(string name);
	void ReloadConfig();
	uint32_t CMDAccessAddOwner (uint32_t acc);
	uint32_t CMDAccessAllOwner ();
	uint32_t CMDAccessAdd( uint32_t access, uint32_t acc);
	uint32_t CMDAccessDel( uint32_t access, uint32_t acc);
	void SaveHostCounter();
	void LoadHostCounter();
	void AddSpoofedIP (string name, string ip);
	bool IsSpoofedIP(string name, string ip);
	void ParseCensoredWords( );
	bool IsChannelException(string name);
	string Censor( string msg);
	string CensorMessage( string msg);
	string CensorRemoveDots( string msg);
	string IncGameNr( string name);
	uint32_t ScoresCount( );
	void CalculateScoresCount();

	// processing functions

	bool Update( unsigned long usecBlock );

	// events

	void EventBNETConnecting( CBNET *bnet );
	void EventBNETConnected( CBNET *bnet );
	void EventBNETDisconnected( CBNET *bnet );
	void EventBNETLoggedIn( CBNET *bnet );
	void EventBNETGameRefreshed( CBNET *bnet );
	void EventBNETGameRefreshFailed( CBNET *bnet );
	void EventBNETConnectTimedOut( CBNET *bnet );
	void EventBNETWhisper( CBNET *bnet, string user, string message );
	void EventBNETChat( CBNET *bnet, string user, string message );
	void EventBNETEmote( CBNET *bnet, string user, string message );
	void EventGameDeleted( CBaseGame *game );

	// other functions
	virtual CMyCallableDownloadFile *ThreadedDownloadFile( string url, string path );
	void ClearAutoHostMap( );
	void ReloadConfigs( );
	void SetConfigs( CConfig *CFG );
	void ExtractScripts( );
	void LoadIPToCountryData( );
	void LoadIPToCountryDataOpt( );
	void CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, string creatorName, string creatorServer, bool whisper );
	CTCPServer *m_GameBroadcastersListener; // listening socket for game broadcasters
	vector<CTCPSocket *> m_GameBroadcasters;// vector of sockets that broadcast the games
	void DenyIP( string ip, uint32_t duration, string reason );
	bool CheckDeny( string ip );
	// UDPCommandSocket patch
	CUDPServer *m_UDPCommandSocket;		// a UDP socket for receiving commands
	string m_UDPCommandSpoofTarget;     // the realm to send udp received commands to
		
	// Metal_Koola's attempts
	bool m_dropifdesync;				// config value: Drop desynced players	
	int m_CookieOffset;					// System used to remove need for bnet_bnlswardencookie. May need further optimization.
};

struct DenyInfo {
	uint32_t Time;
	uint32_t Duration;
	uint32_t Count;
};

#endif
