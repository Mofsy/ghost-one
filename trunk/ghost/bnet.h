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

#ifndef BNET_H
#define BNET_H

//
// CBNET
//

class CTCPClient;
class CCommandPacket;
class CBNCSUtilInterface;
class CBNETProtocol;
class CBNLSClient;
class CIncomingFriendList;
class CIncomingClanList;
class CIncomingChatEvent;
class CCallableAdminCount;
class CCallableAdminAdd;
class CCallableAdminRemove;
class CCallableAdminList;
class CCallableAccessesList;
class CCallableRanks;
class CCallableSafeAdd;
class CCallableSafeRemove;
class CCallableSafeList;
class CCallableCalculateScores;
class CCallableTodayGamesCount;
class CCallableBanCount;
class CCallableBanAdd;
class CCallableBanRemove;
class CCallableBanList;
class CCallableRunQuery;
class CCallableGamePlayerSummaryCheck;
class CCallableDotAPlayerSummaryCheck;
class CDBBan;

typedef pair<string,CCallableAdminCount *> PairedAdminCount;
typedef pair<string,CCallableAdminAdd *> PairedAdminAdd;
typedef pair<string,CCallableAdminRemove *> PairedAdminRemove;
typedef pair<string,CCallableBanCount *> PairedBanCount;
typedef pair<string,CCallableBanAdd *> PairedBanAdd;
typedef pair<string,CCallableBanRemove *> PairedBanRemove;
typedef pair<string,CCallableGamePlayerSummaryCheck *> PairedGPSCheck;
typedef pair<string,CCallableDotAPlayerSummaryCheck *> PairedDPSCheck;
typedef pair<string,CCallableCalculateScores *> PairedCalculateScores;
typedef pair<string,CCallableRanks *> PairedRanks;
typedef pair<string,CCallableSafeAdd *> PairedSafeAdd;
typedef pair<string,CCallableSafeRemove *> PairedSafeRemove;
typedef pair<string,CCallableRunQuery *> PairedRunQuery;

class CBNET
{
public:
	CGHost *m_GHost;

private:
	CTCPClient *m_Socket;							// the connection to battle.net
	CBNETProtocol *m_Protocol;						// battle.net protocol
	CBNLSClient *m_BNLSClient;						// the BNLS client (for external warden handling)
	queue<CCommandPacket *> m_Packets;				// queue of incoming packets
	CBNCSUtilInterface *m_BNCSUtil;					// the interface to the bncsutil library (used for logging into battle.net)
	queue<BYTEARRAY> m_OutPackets;					// queue of outgoing packets to be sent (to prevent getting kicked for flooding)
	queue<BYTEARRAY> m_OutPackets2;					// queue of outgoing packets to be sent (to prevent getting kicked for flooding)
	vector<CIncomingFriendList *> m_Friends;		// vector of friends
	vector<CIncomingClanList *> m_Clans;			// vector of clan members
	vector<string> m_ClanRecruits;
	vector<string> m_ClanPeons;
	vector<string> m_ClanGrunts;
	vector<string> m_ClanShamans;
	vector<string> m_ClanChieftains;
	vector<PairedAdminCount> m_PairedAdminCounts;	// vector of paired threaded database admin counts in progress
	vector<PairedAdminAdd> m_PairedAdminAdds;		// vector of paired threaded database admin adds in progress
	vector<PairedBanCount> m_PairedBanCounts;		// vector of paired threaded database ban counts in progress
	vector<PairedBanAdd> m_PairedBanAdds;			// vector of paired threaded database ban adds in progress
	vector<PairedBanRemove> m_PairedBanRemoves;		// vector of paired threaded database ban removes in progress
	vector<PairedGPSCheck> m_PairedGPSChecks;		// vector of paired threaded database game player summary checks in progress
	vector<PairedCalculateScores> m_PairedCalculateScores;	// vector of paired threaded database score calculations in progress
	vector<PairedRanks> m_PairedRanks;				// vector of paired threaded database ranks in progress
	vector<PairedSafeAdd> m_PairedSafeAdds;			// vector of paired threaded database safe adds in progress
	vector<PairedSafeRemove> m_PairedSafeRemoves;	// vector of paired threaded database safe removes in progress
	vector<PairedRunQuery> m_PairedRunQueries;
	string m_DownloadFileUser;
	CCallableTodayGamesCount *m_CallableTodayGamesCount;
	CCallableAdminList *m_CallableAdminList;		// threaded database admin list in progress
	CCallableAccessesList *m_CallableAccessesList;	// threaded database accesses list in progress
	CCallableSafeList *m_CallableSafeList;			// threaded database safe list in progress
	CCallableSafeList *m_CallableSafeListV;			// threaded database safe list in progress
	CCallableSafeList *m_CallableNotes;				// threaded database notes in progress
	CCallableSafeList *m_CallableNotesN;			// threaded database notes in progress
	CCallableBanList *m_CallableBanList;			// threaded database ban list in progress
//	vector<string> m_Admins;						// vector of cached admins
//	vector<string> m_Safe;							// vector of cached safelist
	vector<CDBBan *> m_Bans;						// vector of cached bans
	vector<uint32_t> m_Accesses;					// vector of cached accesses
	uint32_t m_LastAccess;
	vector<uint32_t> m_BanlistIndexes;
	bool m_Exiting;									// set to true and this class will be deleted next update
	string m_Server;								// battle.net server to connect to
	string m_ServerIP;								// battle.net server to connect to (the IP address so we don't have to resolve it every time we connect)
	string m_ServerAlias;							// battle.net server alias (short name, e.g. "USEast")
	string m_BNLSServer;							// BNLS server to connect to (for warden handling)
	uint16_t m_BNLSPort;							// BNLS port
	uint32_t m_BNLSWardenCookie;					// BNLS warden cookie
	string m_CDKeyROC;								// ROC CD key
	string m_CDKeyTFT;								// TFT CD key
	string m_CountryAbbrev;							// country abbreviation
	string m_Country;								// country
	uint32_t m_LocaleID;							// see: http://msdn.microsoft.com/en-us/library/0h88fahh%28VS.85%29.aspx
	string m_UserName;								// battle.net username
	string m_UserPassword;							// battle.net password
	string m_FirstChannel;							// the first chat channel to join upon entering chat (note: we hijack this to store the last channel when entering a game)
	string m_CurrentChannel;						// the current chat channel
	string m_RootAdmin;								// the root admin
	uint32_t m_ScoresCount;
	bool m_ScoresCountSet;
	char m_CommandTrigger;							// the character prefix to identify commands
	unsigned char m_War3Version;					// custom warcraft 3 version for PvPGN users
	BYTEARRAY m_EXEVersion;							// custom exe version for PvPGN users
	BYTEARRAY m_EXEVersionHash;						// custom exe version hash for PvPGN users
	string m_PasswordHashType;						// password hash type for PvPGN users
	string m_PVPGNRealmName;						// realm name for PvPGN users (for mutual friend spoofchecks)
	uint32_t m_MaxMessageLength;					// maximum message length for PvPGN users
	uint32_t m_HostCounterID;						// the host counter ID to identify players from this realm
	uint32_t m_LastDisconnectedTime;				// GetTime when we were last disconnected from battle.net
	uint32_t m_LastConnectionAttemptTime;			// GetTime when we last attempted to connect to battle.net
	uint32_t m_NextConnectTime;						// GetTime when we should try connecting to battle.net next (after we get disconnected)
	uint32_t m_LastNullTime;						// GetTime when the last null packet was sent for detecting disconnects
	uint32_t m_LastOutPacketTicks2;					// GetTicks when the last packet was sent for the m_OutPackets queue
	uint32_t m_LastOutPacketTicks;					// GetTicks when the last packet was sent for the m_OutPackets queue
	uint32_t m_LastOutPacketSize;
	uint32_t m_LastAdminRefreshTime;				// GetTime when the admin list was last refreshed from the database
	uint32_t m_LastBanRefreshTime;					// GetTime when the ban list was last refreshed from the database
	bool m_FirstConnect;
	uint32_t m_LastMars;
	uint32_t m_LastTempBansRemoveTime;
	bool m_RemoveTempBans;
	uint32_t m_LastTempBanRemoveTime;
	uint32_t m_LastBanRemoveTime;
	bool m_WaitingToConnect;						// if we're waiting to reconnect to battle.net after being disconnected
	bool m_LoggedIn;								// if we've logged into battle.net or not
	bool m_InChat;									// if we've entered chat or not (but we're not necessarily in a chat channel yet)
	bool m_HoldFriends;								// whether to auto hold friends when creating a game or not
	bool m_HoldClan;								// whether to auto hold clan members when creating a game or not
	bool m_PublicCommands;							// whether to allow public commands or not
	uint32_t m_LastStats;							// GetTime when the last stats was announced
	uint32_t m_LastChatCommandTime;					// GetTime when the last chat command was sent for the m_ChatCommands queue
	bool m_Whereis;


public:
	CBNET( CGHost *nGHost, string nServer, string nServerAlias, string nBNLSServer, uint16_t nBNLSPort, uint32_t nBNLSWardenCookie, string nCDKeyROC, string nCDKeyTFT, string nCountryAbbrev, string nCountry, uint32_t nLocaleID, string nUserName, string nUserPassword, string nFirstChannel, string nRootAdmin, char nCommandTrigger, bool nHoldFriends, bool nHoldClan, bool nPublicCommands, unsigned char nWar3Version, BYTEARRAY nEXEVersion, BYTEARRAY nEXEVersionHash, string nPasswordHashType, string nPVPGNRealmName, uint32_t nMaxMessageLength, uint32_t nHostCounterID );
	~CBNET( );

	uint32_t m_LastGameCountRefreshTime;
	uint32_t m_LastHelpTicks;
	uint32_t m_LastGetGamesTicks;
	uint32_t m_LastMassWhisperTime;					// GetTime when we sent our last mass whisper
	uint32_t m_LastFriendListTime;					// GetTime when we got the friend list
	uint32_t m_TodayGamesCount;
	vector<string> m_Notes;							// vector of cached notes names
	vector<string> m_NotesN;						// vector of cached notes
	vector<string> m_Safe;							// vector of cached safelist
	vector<string> m_SafeVouchers;					// vector of cached safelist vouchers
	vector<string> m_Admins;						// vector of cached admins
	vector<string> m_RemoveTempBansList;			// vector of temp bans to remove today
	vector<string> m_RemoveBansList;				// vector of bans to remove
	string m_RemoveTempBansUser;
	vector<PairedAdminRemove> m_PairedAdminRemoves;	// vector of paired threaded database admin removes in progress
	vector<string> m_FriendsEnteringBnet;
	vector<PairedDPSCheck> m_PairedDPSChecks;		// vector of paired threaded database DotA player summary checks in progress
	uint32_t m_LastFriendEnteredWhisper;
	bool GetExiting( )					{ return m_Exiting; }
	string GetServer( )					{ return m_Server; }
	string GetServerAlias( )			{ return m_ServerAlias; }
	string GetCDKeyROC( )				{ return m_CDKeyROC; }
	string GetCDKeyTFT( )				{ return m_CDKeyTFT; }
	string GetUserName( )				{ return m_UserName; }
	string GetUserPassword( )			{ return m_UserPassword; }
	string GetFirstChannel( )			{ return m_FirstChannel; }
	string GetCurrentChannel( )			{ return m_CurrentChannel; }
	string GetRootAdmin( )				{ return m_RootAdmin; }
	char GetCommandTrigger( )			{ return m_CommandTrigger; }
	BYTEARRAY GetEXEVersion( )			{ return m_EXEVersion; }
	BYTEARRAY GetEXEVersionHash( )		{ return m_EXEVersionHash; }
	string GetPasswordHashType( )		{ return m_PasswordHashType; }
	string GetPVPGNRealmName( )			{ return m_PVPGNRealmName; }
	uint32_t GetHostCounterID( )		{ return m_HostCounterID; }
	bool GetLoggedIn( )					{ return m_LoggedIn; }
	bool GetInChat( )					{ return m_InChat; }
	bool GetHoldFriends( )				{ return m_HoldFriends; }
	bool GetHoldClan( )					{ return m_HoldClan; }
	bool GetPublicCommands( )			{ return m_PublicCommands; }
	vector<string> *GetAdmins( )		{ return &m_Admins; }
	vector<CIncomingFriendList *> *GetFriends( ) { return &m_Friends; }
	uint32_t GetOutPacketsQueued( )		{ return m_OutPackets.size( ); }
	BYTEARRAY GetUniqueName( );

	// processing functions

	unsigned int SetFD( void *fd, void *send_fd, int *nfds );
	bool Update( void *fd, void *send_fd );
	void ExtractPackets( );
	void ProcessPackets( );
	void ProcessChatEvent( CIncomingChatEvent *chatEvent );

	// functions to send packets to battle.net

	void SendJoinChannel( string channel );
	void SendGetFriendsList( );
	void SendGetClanList( );
	void QueueEnterChat( );
	void ImmediateChatCommand( string chatCommand );
	void SendChatCommand( string chatCommand );
	void QueueChatCommand( string chatCommand );
	void QueueChatCommand( string chatCommand, string user, bool whisper );
	void QueueGameCreate( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *saveGame, uint32_t hostCounter );
	void QueueGameRefresh( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *saveGame, uint32_t upTime, uint32_t hostCounter, uint32_t SlotsTotal, uint32_t SlotsOpen );
	void QueueGameUncreate( );

	void UnqueuePackets( unsigned char type );
	void UnqueueChatCommand( string chatCommand );
	void UnqueueGameRefreshes( );

	// other functions

	void WarnPlayer (string Victim, string Reason, string User, bool Whisper);
	void SetWhereis(bool nWhereis) { m_Whereis = nWhereis; }
	bool GetWhereis( ) { return m_Whereis; }
	bool IsAdmin( string name );
	bool IsSafe( string name );
	string Voucher( string name );
	bool IsNoted( string name );
	string Note( string name );
	bool IsRootAdmin( string name );
	CDBBan *IsBannedName( string name );
	CDBBan *IsBannedIP( string ip );
	void AddAdmin( string name );
	void AddSafe( string name, string voucher );
	void AddNote( string name, string note );
	void AddBan( string name, string ip, string gamename, string admin, string reason, string expiredate );
	void AddBan( string name, string ip, string date, string gamename, string admin, string reason, string expiredate );
	void RemoveAdmin( string name );
	void RemoveSafe( string name );
	void RemoveNote( string name );
	void RemoveBan( string name );
	void HoldFriends( CBaseGame *game );
	void HoldClan( CBaseGame *game );
	bool IsFriend( string name );
	bool IsClanMember( string name );
	bool IsClanChieftain( string name );
	bool IsClanShaman( string name );
	bool IsClanGrunt( string name );
	bool IsClanPeon( string name );
	bool IsClanRecruit( string name );
	bool IsClanFullMember( string name );

	virtual uint32_t LastAccess( );
	virtual bool UpdateAccess( string name, uint32_t access );
	virtual void UpdateMapRemoveBan( uint32_t n );
	virtual void UpdateMapAddBan( uint32_t n );
	virtual void UpdateMap( );
	virtual void ChannelJoin( string name );
	string GetPlayerFromNamePartial( string name);
};

#endif
