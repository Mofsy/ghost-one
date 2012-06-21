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
#include <boost/thread.hpp>

//
// CGHostDB
//

CGHostDB :: CGHostDB( CConfig *CFG )
{
	m_HasError = false;
	m_ScoreFormula = string();
}

CGHostDB :: ~CGHostDB( )
{

}

void CGHostDB :: RecoverCallable( CBaseCallable *callable )
{

}

bool CGHostDB :: Begin( )
{
	return true;
}

bool CGHostDB :: Commit( )
{
	return true;
}

uint32_t CGHostDB :: AdminCount( string server )
{
	return 0;
}

bool CGHostDB :: AdminCheck( string server, string user )
{
	return false;
}

uint32_t CGHostDB :: CountPlayerGames( string name )
{
	return 0;
}

bool CGHostDB :: AdminSetAccess( string server, string user, uint32_t access )
{
	return false;
}

uint32_t CGHostDB :: AdminAccess( string server, string user )
{
	return 0;
}

bool CGHostDB :: SafeCheck( string server, string user )
{
	return false;
}

bool CGHostDB :: SafeAdd( string server, string user, string voucher )
{
	return false;
}

bool CGHostDB :: SafeRemove( string server, string user )
{
	return false;
}

bool CGHostDB :: AdminAdd( string server, string user )
{
	return false;
}

bool CGHostDB :: AdminRemove( string server, string user )
{
	return false;
}

vector<string> CGHostDB :: AdminList( string server )
{
	return vector<string>( );
}

vector<string> CGHostDB :: RemoveTempBanList( string server )
{
	return vector<string>( );
}

vector<string> CGHostDB :: RemoveBanListDay( string server, uint32_t day )
{
	return vector<string>( );
}

CDBScoreSummary *CGHostDB :: ScoreCheck( string name)
{
	return NULL;
}

string CGHostDB :: Ranks ( string server)
{
	return "";
}

uint32_t CGHostDB :: ScoresCount( string server )
{
	return 0;
}

uint32_t CGHostDB :: TodayGamesCount( )
{
	return 0;
}

uint32_t CGHostDB :: BanCount( string server )
{
	return 0;
}

uint32_t CGHostDB :: BanCount( string name, uint32_t warn )
{
	return 0;
}

uint32_t CGHostDB :: BanCount( string server, string name, uint32_t warn )
{
	return 0;
}

CDBBan *CGHostDB :: BanCheck( string server, string user, string ip, uint32_t warn )
{
	return NULL;
}

CDBBan *CGHostDB :: BanCheckIP( string server, string user, uint32_t warn )
{
	return NULL;
}

bool CGHostDB :: BanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn )
{
	return false;
}

bool CGHostDB :: BanRemove( string server, string user, string admin, uint32_t warn )
{
	return false;
}

bool CGHostDB :: BanRemove( string user, uint32_t warn )
{
	return false;
}

uint32_t CGHostDB :: RemoveTempBans( string server )
{
	return 0;
}

bool CGHostDB :: BanUpdate( string user, string ip )
{
	return false;
}

string CGHostDB :: RunQuery( string query )
{
	return string();
}

bool CGHostDB :: WarnUpdate( string user, uint32_t before, uint32_t after )
{
	return false;
}

bool CGHostDB :: WarnForget( string name, uint32_t gamethreshold )
{
	return false;
}

vector<CDBBan *> CGHostDB :: BanList( string server )
{
	return vector<CDBBan *>( );
}

uint32_t CGHostDB :: GameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	return 0;
}

uint32_t CGHostDB :: GamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	return 0;
}

uint32_t CGHostDB :: GamePlayerCount( string name )
{
	return 0;
}

CDBGamePlayerSummary *CGHostDB :: GamePlayerSummaryCheck( string name )
{
	return NULL;
}

uint32_t CGHostDB :: DotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	return 0;
}

uint32_t CGHostDB :: DotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills )
{
	return 0;
}

uint32_t CGHostDB :: DotAPlayerCount( string name )
{
	return 0;
}

CDBDotAPlayerSummary *CGHostDB :: DotAPlayerSummaryCheck( string name )
{
	return NULL;
}

string CGHostDB :: FromCheck( uint32_t ip )
{
	return "??";
}

string CGHostDB :: WarnReasonsCheck( string user, uint32_t warn )
{
	return "??";
}
bool CGHostDB :: FromAdd( uint32_t ip1, uint32_t ip2, string country )
{
	return false;
}

bool CGHostDB :: DownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	return false;
}

uint32_t CGHostDB :: LastAccess( )
{
	return 0;
}

uint32_t CGHostDB :: W3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	return 0;
}

bool CGHostDB :: W3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints )
{
	return false;
}

bool CGHostDB :: W3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals )
{
	return false;
}

bool CGHostDB :: W3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings )
{
	return false;
}

void CGHostDB :: CreateThread( CBaseCallable *callable )
{
	callable->SetReady( true );
}

void CGHostDB :: CreateThreadReal( CBaseCallable *callable )
{
	try
	{
		boost :: thread Thread( boost :: ref( *callable ) );
	}
	catch( boost :: thread_resource_error tre )
	{
		CONSOLE_Print( "[GHOST] error spawning thread on attempt #1 [" + string( tre.what( ) ) + "], pausing execution and trying again in 50ms" );
//		MILLISLEEP( 50 );

		try
		{
			boost :: thread Thread( boost :: ref( *callable ) );
		}
		catch( boost :: thread_resource_error tre2 )
		{
			CONSOLE_Print( "[GHOST] error spawning thread on attempt #2 [" + string( tre2.what( ) ) + "], giving up" );
			callable->SetReady( true );
		}
	}
}

CCallableAdminCount *CGHostDB :: ThreadedAdminCount( string server )
{
	return NULL;
}

CCallableAdminCheck *CGHostDB :: ThreadedAdminCheck( string server, string user )
{
	return NULL;
}

CCallableAdminAdd *CGHostDB :: ThreadedAdminAdd( string server, string user )
{
	return NULL;
}

CCallableAdminRemove *CGHostDB :: ThreadedAdminRemove( string server, string user )
{
	return NULL;
}

CCallableAdminList *CGHostDB :: ThreadedAdminList( string server )
{
	return NULL;
}

CCallableAccessesList *CGHostDB :: ThreadedAccessesList( string server )
{
	return NULL;
}

CCallableSafeList *CGHostDB :: ThreadedSafeList( string server )
{
	return NULL;
}

CCallableSafeList *CGHostDB :: ThreadedSafeListV( string server )
{
	return NULL;
}

CCallableSafeList *CGHostDB :: ThreadedNotes( string server )
{
	return NULL;
}

CCallableSafeList *CGHostDB :: ThreadedNotesN( string server )
{
	return NULL;
}

CCallableRanks *CGHostDB :: ThreadedRanks( string server )
{
	return NULL;
}

CCallableSafeCheck *CGHostDB :: ThreadedSafeCheck( string server, string user )
{
	return NULL;
}

CCallableSafeAdd *CGHostDB :: ThreadedSafeAdd( string server, string user, string voucher )
{
	return NULL;
}

CCallableSafeRemove *CGHostDB :: ThreadedSafeRemove( string server, string user )
{
	return NULL;
}

CCallableNoteAdd *CGHostDB :: ThreadedNoteAdd( string server, string user, string note )
{
	return NULL;
}

CCallableNoteAdd *CGHostDB :: ThreadedNoteUpdate( string server, string user, string note )
{
	return NULL;
}

CCallableNoteRemove *CGHostDB :: ThreadedNoteRemove( string server, string user )
{
	return NULL;
}

CCallableCalculateScores *CGHostDB :: ThreadedCalculateScores( string formula, string mingames )
{
	return NULL;
}

CCallableWarnUpdate *CGHostDB :: ThreadedWarnUpdate( string name, uint32_t before, uint32_t after )
{
	return NULL;
}

CCallableWarnForget *CGHostDB :: ThreadedWarnForget( string name, uint32_t gamethreshold )
{
	return NULL;
}

CCallableTodayGamesCount *CGHostDB :: ThreadedTodayGamesCount( )
{
	return NULL;
}

CCallableBanCount *CGHostDB :: ThreadedBanCount( string server )
{
	return NULL;
}

CCallableBanCheck *CGHostDB :: ThreadedBanCheck( string server, string user, string ip, uint32_t warn )
{
	return NULL;
}

CCallableBanAdd *CGHostDB :: ThreadedBanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t expiredaytime, uint32_t warn )
{
	return NULL;
}

CCallableBanRemove *CGHostDB :: ThreadedBanRemove( string server, string user, string admin, uint32_t warn )
{
	return NULL;
}

CCallableBanRemove *CGHostDB :: ThreadedBanRemove( string user, uint32_t warn )
{
	return NULL;
}

CCallableBanList *CGHostDB :: ThreadedBanList( string server )
{
	return NULL;
}

CCallableGameAdd *CGHostDB :: ThreadedGameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	return NULL;
}

CCallableGamePlayerAdd *CGHostDB :: ThreadedGamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	return NULL;
}

CCallableGamePlayerSummaryCheck *CGHostDB :: ThreadedGamePlayerSummaryCheck( string name )
{
	return NULL;
}

CCallableDotAGameAdd *CGHostDB :: ThreadedDotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	return NULL;
}

CCallableDotAPlayerAdd *CGHostDB :: ThreadedDotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills )
{
	return NULL;
}

CCallableDotAPlayerSummaryCheck *CGHostDB :: ThreadedDotAPlayerSummaryCheck( string name, string formula, string mingames, string gamestate )
{
	return NULL;
}

CCallableDownloadAdd *CGHostDB :: ThreadedDownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	return NULL;
}

CCallableWarnCount *CGHostDB :: ThreadedWarnCount( string name, uint32_t warn )
{
	return NULL;
}

CCallableBanUpdate *CGHostDB :: ThreadedBanUpdate( string name, string ip )
{
	return NULL;
}

CCallableRunQuery *CGHostDB :: ThreadedRunQuery( string query )
{
	return NULL;
}

CCallableScoreCheck *CGHostDB :: ThreadedScoreCheck( string category, string name, string server )
{
	return NULL;
}

CCallableW3MMDPlayerAdd *CGHostDB :: ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	return NULL;
}

CCallableW3MMDVarAdd *CGHostDB :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints )
{
	return NULL;
}

CCallableW3MMDVarAdd *CGHostDB :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals )
{
	return NULL;
}

CCallableW3MMDVarAdd *CGHostDB :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings )
{
	return NULL;
}

//
// Callables
//

void CBaseCallable :: Init( )
{
	m_StartTicks = GetTicks( );
}

void CBaseCallable :: Close( )
{
	m_EndTicks = GetTicks( );
	m_Ready = true;
}

CCallableAdminCount :: ~CCallableAdminCount( )
{

}

CCallableAdminCheck :: ~CCallableAdminCheck( )
{

}

CCallableAdminAdd :: ~CCallableAdminAdd( )
{

}

CCallableAdminRemove :: ~CCallableAdminRemove( )
{

}

CCallableAdminList :: ~CCallableAdminList( )
{

}

CCallableAccessesList :: ~CCallableAccessesList( )
{

}

CCallableSafeList :: ~CCallableSafeList( )
{

}

CCallableRanks :: ~CCallableRanks( )
{

}

CCallableSafeCheck :: ~CCallableSafeCheck( )
{

}

CCallableSafeAdd :: ~CCallableSafeAdd( )
{

}

CCallableSafeRemove :: ~CCallableSafeRemove( )
{

}

CCallableNoteAdd :: ~CCallableNoteAdd( )
{

}

CCallableNoteRemove :: ~CCallableNoteRemove( )
{

}

CCallableCalculateScores :: ~CCallableCalculateScores( )
{

}

CCallableWarnUpdate :: ~CCallableWarnUpdate( )
{

}

CCallableWarnForget :: ~CCallableWarnForget( )
{

}

CCallableTodayGamesCount :: ~CCallableTodayGamesCount( )
{

}

CCallableBanCount :: ~CCallableBanCount( )
{

}

CCallableBanCheck :: ~CCallableBanCheck( )
{
	delete m_Result;
}

CCallableBanAdd :: ~CCallableBanAdd( )
{

}

CCallableBanRemove :: ~CCallableBanRemove( )
{

}

CCallableBanList :: ~CCallableBanList( )
{
	// don't delete anything in m_Result here, it's the caller's responsibility
}

CCallableGameAdd :: ~CCallableGameAdd( )
{

}

CCallableGamePlayerAdd :: ~CCallableGamePlayerAdd( )
{

}

CCallableGamePlayerSummaryCheck :: ~CCallableGamePlayerSummaryCheck( )
{
	delete m_Result;
}

CCallableDotAGameAdd :: ~CCallableDotAGameAdd( )
{

}

CCallableDotAPlayerAdd :: ~CCallableDotAPlayerAdd( )
{

}

CCallableDotAPlayerSummaryCheck :: ~CCallableDotAPlayerSummaryCheck( )
{
	delete m_Result;
}

CCallableDownloadAdd :: ~CCallableDownloadAdd( )
{

}

CCallableWarnCount :: ~CCallableWarnCount( )
{

}

CCallableBanUpdate :: ~CCallableBanUpdate( )
{

}

CCallableRunQuery :: ~CCallableRunQuery( )
{

}

CCallableScoreCheck :: ~CCallableScoreCheck( )
{

}

CCallableW3MMDPlayerAdd :: ~CCallableW3MMDPlayerAdd( )
{

}

CCallableW3MMDVarAdd :: ~CCallableW3MMDVarAdd( )
{

}

//
// CDBBan
//

CDBBan :: CDBBan( string nServer, string nName, string nIP, string nDate, string nGameName, string nAdmin, string nReason, string nExpireDate)
{
	m_Server = nServer;
	m_Name = nName;
	m_IP = nIP;
	m_Date = nDate;
	m_GameName = nGameName;
	m_Admin = nAdmin;
	m_Reason = nReason;
	m_ExpireDate = nExpireDate;
}

CDBBan :: ~CDBBan( )
{

}

//
// CDBGame
//

CDBGame :: CDBGame( uint32_t nID, string nServer, string nMap, string nDateTime, string nGameName, string nOwnerName, uint32_t nDuration )
{
	m_ID = nID;
	m_Server = nServer;
	m_Map = nMap;
	m_DateTime = nDateTime;
	m_GameName = nGameName;
	m_OwnerName = nOwnerName;
	m_Duration = nDuration;
}

CDBGame :: ~CDBGame( )
{

}

//
// CDBGamePlayer
//

CDBGamePlayer :: CDBGamePlayer( uint32_t nID, uint32_t nGameID, string nName, string nIP, uint32_t nSpoofed, string nSpoofedRealm, uint32_t nReserved, uint32_t nLoadingTime, uint32_t nLeft, string nLeftReason, uint32_t nTeam, uint32_t nColour, uint32_t nSlot, string nCountry, uint32_t nLeavingTime, uint32_t nTeam1, uint32_t nTeam2, uint32_t nLeftEarly )
{
	m_ID = nID;
	m_GameID = nGameID;
	m_Name = nName;
	m_IP = nIP;
	m_Spoofed = nSpoofed;
	m_SpoofedRealm = nSpoofedRealm;
	m_Reserved = nReserved;
	m_LoadingTime = nLoadingTime;
	m_Left = nLeft;
	m_LeftReason = nLeftReason;
	m_Team = nTeam;
	m_Colour = nColour;
	m_LeftEarly = nLeftEarly;
	m_Slot = nSlot;
	m_Country = nCountry;
	m_LeavingTime = nLeavingTime;
	m_Team1 = nTeam1;
	m_Team2 = nTeam2;
}

CDBGamePlayer :: ~CDBGamePlayer( )
{

}

//
// CDBGamePlayerSummary
//

CDBGamePlayerSummary :: CDBGamePlayerSummary( string nServer, string nName, string nFirstGameDateTime, string nLastGameDateTime, uint32_t nTotalGames, uint32_t nMinLoadingTime, uint32_t nAvgLoadingTime, uint32_t nMaxLoadingTime, uint32_t nMinLeftPercent, uint32_t nAvgLeftPercent, uint32_t nMaxLeftPercent, uint32_t nMinDuration, uint32_t nAvgDuration, uint32_t nMaxDuration )
{
	m_Server = nServer;
	m_Name = nName;
	m_FirstGameDateTime = nFirstGameDateTime;
	m_LastGameDateTime = nLastGameDateTime;
	m_TotalGames = nTotalGames;
	m_MinLoadingTime = nMinLoadingTime;
	m_AvgLoadingTime = nAvgLoadingTime;
	m_MaxLoadingTime = nMaxLoadingTime;
	m_MinLeftPercent = nMinLeftPercent;
	m_AvgLeftPercent = nAvgLeftPercent;
	m_MaxLeftPercent = nMaxLeftPercent;
	m_MinDuration = nMinDuration;
	m_AvgDuration = nAvgDuration;
	m_MaxDuration = nMaxDuration;
}

CDBGamePlayerSummary :: ~CDBGamePlayerSummary( )
{

}

//
// CDBDotAGame
//

CDBDotAGame :: CDBDotAGame( uint32_t nID, uint32_t nGameID, uint32_t nWinner, uint32_t nMin, uint32_t nSec )
{
	m_ID = nID;
	m_GameID = nGameID;
	m_Winner = nWinner;
	m_Min = nMin;
	m_Sec = nSec;
}

CDBDotAGame :: ~CDBDotAGame( )
{

}

//
// CDBDotAPlayer
//

CDBDotAPlayer :: CDBDotAPlayer( )
{
	m_ID = 0;
	m_GameID = 0;
	m_Colour = 0;
	m_Kills = 0;
	m_Deaths = 0;
	m_CreepKills = 0;
	m_CreepDenies = 0;
	m_Assists = 0;
	m_Gold = 0;
	m_NeutralKills = 0;
	m_NewColour = 0;
	m_TowerKills = 0;
	m_RaxKills = 0;
	m_CourierKills = 0;
}

CDBDotAPlayer :: CDBDotAPlayer( uint32_t nID, uint32_t nGameID, uint32_t nColour, uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nGold, uint32_t nNeutralKills, string nItem1, string nItem2, string nItem3, string nItem4, string nItem5, string nItem6, string nHero, uint32_t nNewColour, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills )
{
	m_ID = nID;
	m_GameID = nGameID;
	m_Colour = nColour;
	m_Kills = nKills;
	m_Deaths = nDeaths;
	m_CreepKills = nCreepKills;
	m_CreepDenies = nCreepDenies;
	m_Assists = nAssists;
	m_Gold = nGold;
	m_NeutralKills = nNeutralKills;
	m_Items[0] = nItem1;
	m_Items[1] = nItem2;
	m_Items[2] = nItem3;
	m_Items[3] = nItem4;
	m_Items[4] = nItem5;
	m_Items[5] = nItem6;
	m_Hero = nHero;
	m_NewColour = nNewColour;
	m_TowerKills = nTowerKills;
	m_RaxKills = nRaxKills;
	m_CourierKills = nCourierKills;
}

CDBDotAPlayer :: ~CDBDotAPlayer( )
{

}

string CDBDotAPlayer :: GetItem( unsigned int i )
{
	if( i < 6 )
		return m_Items[i];

	return string( );
}

void CDBDotAPlayer :: SetItem( unsigned int i, string item )
{
	if( i < 6 )
		m_Items[i] = item;
}

//
// CDBDotAPlayerSummary
//

CDBDotAPlayerSummary :: CDBDotAPlayerSummary( string nServer, string nName, uint32_t nTotalGames, uint32_t nTotalWins, uint32_t nTotalLosses, uint32_t nTotalKills, uint32_t nTotalDeaths, uint32_t nTotalCreepKills, uint32_t nTotalCreepDenies, uint32_t nTotalAssists, uint32_t nTotalNeutralKills, uint32_t nTotalTowerKills, uint32_t nTotalRaxKills, uint32_t nTotalCourierKills,
											 double nWinsPerGame, double nLossesPerGame,
											 double nKillsPerGame, double nDeathsPerGame, double nCreepKillsPerGame, double nCreepDeniesPerGame, 
											 double nAssistsPerGame, double nNeutralKillsPerGame, double nScore, double nTowerKillsPerGame, double nRaxKillsPerGame, double nCourierKillsPerGame, uint32_t nRank )
{
	m_Server = nServer;
	m_Name = nName;
	m_TotalGames = nTotalGames;
	m_TotalWins = nTotalWins;
	m_TotalLosses = nTotalLosses;
	m_TotalKills = nTotalKills;
	m_TotalDeaths = nTotalDeaths;
	m_TotalCreepKills = nTotalCreepKills;
	m_TotalCreepDenies = nTotalCreepDenies;
	m_TotalAssists = nTotalAssists;
	m_TotalNeutralKills = nTotalNeutralKills;
	m_WinsPerGame = nWinsPerGame;
	m_LossesPerGame = nLossesPerGame;
	m_KillsPerGame = nKillsPerGame;
	m_DeathsPerGame = nDeathsPerGame;
	m_CreepKillsPerGame = nCreepKillsPerGame;
	m_CreepDeniesPerGame = nCreepDeniesPerGame;
	m_AssistsPerGame = nAssistsPerGame;
	m_NeutralKillsPerGame = nNeutralKillsPerGame;
	m_Score = nScore;
	m_TotalTowerKills = nTotalTowerKills;
	m_TotalRaxKills = nTotalRaxKills;
	m_TotalCourierKills = nTotalCourierKills;
	m_TowerKillsPerGame = nTowerKillsPerGame;
	m_RaxKillsPerGame = nRaxKillsPerGame;
	m_CourierKillsPerGame = nCourierKillsPerGame;
	m_Rank = nRank;
}

CDBDotAPlayerSummary :: ~CDBDotAPlayerSummary( )
{

}

CDBScoreSummary :: CDBScoreSummary(string nName, double nScore, uint32_t nRank)
{
	m_Score = nScore;
	m_Rank = nRank;
	m_Name = nName;
}

CDBScoreSummary :: ~CDBScoreSummary()
{

}
