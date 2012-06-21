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

#ifndef GAME_H
#define GAME_H

//
// CGame
//

class CDBBan;
class CDBGame;
class CDBGamePlayer;
class CStats;
class CCallableBanCheck;
class CCallableRanks;
class CCallableCalculateScores;
class CCallableWarnUpdate;
class CCallableWarnForget;
class CCallableBanAdd;
class CCallableGameAdd;
class CCallableGamePlayerSummaryCheck;
class CCallableDotAPlayerSummaryCheck;

typedef pair<string,CCallableBanCheck *> PairedBanCheck;
typedef pair<string,CCallableBanAdd *> PairedBanAdd;
typedef pair<string,CCallableGamePlayerSummaryCheck *> PairedGPSCheck;
typedef pair<string,CCallableDotAPlayerSummaryCheck *> PairedDPSCheck;

class CGame : public CBaseGame
{
protected:
	CDBGame *m_DBGame;							// potential game data for the database
	CStats *m_Stats;							// class to keep track of game stats such as kills/deaths/assists in dota
	CCallableGameAdd *m_CallableGameAdd;		// threaded database game addition in progress
	vector<PairedBanCheck> m_PairedBanChecks;	// vector of paired threaded database ban checks in progress
	vector<PairedRanks> m_PairedRanks;			// vector of paired threaded database ranks in progress
	vector<PairedCalculateScores> m_PairedCalculateScores;			// vector of paired threaded database calculate scores in progress
	vector<PairedSafeAdd> m_PairedSafeAdds;		// vector of paired threaded database safe adds in progress
	vector<PairedSafeRemove> m_PairedSafeRemoves;// vector of paired threaded database safe removes in progress
	vector<PairedGPSCheck> m_PairedGPSChecks;	// vector of paired threaded database game player summary checks in progress
	vector<PairedDPSCheck> m_PairedDPSChecks;	// vector of paired threaded database DotA player summary checks in progress
	uint32_t m_GameOverTime;					// GetTime when the game was over as reported by the stats class
	bool m_DoAutoWarns;							// enable automated warns for early leavers

public:
	CGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer );
	vector<PairedBanAdd> m_PairedBanAdds;		// vector of paired threaded database ban adds in progress
	vector<PairedBanRemove> m_PairedBanRemoves;	// vector of paired threaded database ban removes in progress
	virtual ~CGame( );

	virtual bool Update( void *fd, void *send_fd );
	virtual void EventPlayerDeleted( CGamePlayer *player );
	virtual void EventPlayerLeft( CGamePlayer *player, uint32_t reason  );
	virtual void EventPlayerAction( CGamePlayer *player, CIncomingAction *action );
	virtual bool EventPlayerBotCommand( CGamePlayer *player, string command, string payload );
	virtual void EventGameStarted( );
	virtual bool IsGameDataSaved( );
	virtual void SaveGameData( );

	virtual void WarnPlayer( CDBBan *LastMatch, string Reason, string User);
	virtual void WarnPlayer( string Victim, string Reason, string User);
};

#endif
