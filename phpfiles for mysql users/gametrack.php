<?php

# gametrack collects information based on the latest games
# specifically, for every player (name-realm combination), it maintains:
#  * the last ten bots that the player was seen in
#  * the last ten games that the player was seen in
#  * averagy stay percentage
#  * total number of games
#  * time user was first seen
#  * time user was last seen
# the bots and games are based on games.botid and games.id respectively
#  additionally, they are both stored as comma-delimited strings
#  to retrieve an array of ID integers, use explode(',', $str)

# it is recommended to set this up as a cronjob every five minutes:
#    */5 * * * * cd /path/to/gametrack/ && php gametrack.php > /dev/null

# BEGIN CONFIGURATION

$db_name = "ghost";
$db_host = "localhost";
$db_username = "ghost";
$db_password = "";

# END CONFIGURATION

function escape($str) {
	return mysql_real_escape_string($str);
}

echo "Connecting to database\n";

mysql_connect($db_host, $db_username, $db_password);
mysql_select_db($db_name);

# create table if not already there
echo "Creating table if not exists\n";
mysql_query("CREATE TABLE IF NOT EXISTS gametrack (name VARCHAR(15), realm VARCHAR(100), bots VARCHAR(40), lastgames VARCHAR(100), total_leftpercent DOUBLE, num_leftpercent INT, num_games INT, time_created DATETIME, time_active DATETIME, KEY name (name), KEY realm (realm))") or die(mysql_error());

# read the next player from file
echo "Detecting next player\n";
$next_player = 1;
$filename = "next_player.txt";

if(file_exists($filename) && is_readable($filename)) {
	$fh = fopen($filename, 'r');
	$next_player = intval(trim(fgets($fh)));
	fclose($fh);
}

echo "Detected next player: $next_player\n";

# get the next 5000 players
echo "Reading next 5000 players\n";
$result = mysql_query("SELECT gameplayers.botid, name, spoofedrealm, gameid, gameplayers.id, (`left`/duration) FROM gameplayers LEFT JOIN games ON games.id = gameid WHERE gameplayers.id >= '$next_player' ORDER BY gameplayers.id LIMIT 5000") or die(mysql_error());
echo "Got " . mysql_num_rows($result) . " players\n";

while($row = mysql_fetch_array($result)) {
	$botid = intval($row[0]);
	$name = escape($row[1]);
	$realm = escape($row[2]);
	$gameid = escape($row[3]);
	$leftpercent = escape($row[5]);
	
	# see if this player already has an entry, and retrieve if there is
	$checkResult = mysql_query("SELECT bots, lastgames FROM gametrack WHERE name = '$name' AND realm = '$realm'") or die(mysql_error());
	
	if($checkRow = mysql_fetch_array($checkResult)) {
		# update bots and lastgames shifting-window arrays
		$bots = explode(',', $checkRow[0]);
		$lastgames = explode(',', $checkRow[1]);
		
		if(in_array($botid, $bots)) {
			$bots = array_diff($bots, array($botid));
		}
		
		$bots[] = $botid;
		$lastgames[] = $gameid;
		
		if(count($bots) > 10) {
			array_shift($bots);
		}
		
		if(count($lastgames) > 10) {
			array_shift($lastgames);
		}
		
		$botString = escape(implode(',', $bots));
		$lastString = escape(implode(',', $lastgames));
		mysql_query("UPDATE gametrack SET bots = '$botString', lastgames = '$lastString', total_leftpercent = total_leftpercent + '$leftpercent', num_leftpercent = num_leftpercent + 1, num_games = num_games + 1, time_active = NOW() WHERE name = '$name' AND realm = '$realm'") or die(mysql_error());
	} else {
		$botString = escape($botid);
		$lastString = escape($gameid);
		mysql_query("INSERT INTO gametrack (name, realm, bots, lastgames, total_leftpercent, num_leftpercent, num_games, time_created, time_active) VALUES ('$name', '$realm', '$botString', '$lastString', '$leftpercent', '1', '1', NOW(), NOW())") or die(mysql_error());
	}
	
	$next_player = $row[4] + 1;
}

$fh = fopen($filename, 'w');
fwrite($fh, $next_player . "\n");

?>
