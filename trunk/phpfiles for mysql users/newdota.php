<?php

# newdota will notify you of updates to the popular DotA Allstars map by email
# you must have PHP email setup properly

# it is recommended to set this up as a cronjob every ten minutes:
#   */10 * * * * cd /path/to/newdota/ && php newdota.php > newdota.log
# or, if you don't like logs:
#   */10 * * * * cd /path/to/newdota/ && php newdota.php > /dev/null

# BEGIN CONFIGURATION

$targets = array('youremailaddress@example.com');
$from_address = "dota-updates-notify@example.com";

# END CONFIGURATION

function escape($str) {
	return mysql_real_escape_string($str);
}

# read current version from file
echo "Detecting current version\n";
$currentVersion = 1;
$filename = "newdota_current.txt";

if(file_exists($filename) && is_readable($filename)) {
	$fh = fopen($filename, 'r');
	$currentVersion = trim(fgets($fh));
	fclose($fh);
}

echo "Detected current version: $currentVersion\n";

$fh = fopen("http://www.getdota.com", 'r') or die('could not open the getdota page');
$latestVersion = false;

while(($buffer = fgets($fh, 4096)) !== false) {
	if(strpos($buffer, "Latest Map") !== FALSE) {
		echo "Detected latest version: $buffer\n";
		$latestVersion = $buffer;
	}
}

if($latestVersion === false) {
	echo "Error: could not retrieve latest version!\n";
} else if(trim($latestVersion) != trim($currentVersion)) {
	echo "New version! Emailing...\n";
	
	foreach($targets as $target) {
		$headers = "From: $from_address\r\n";
		$headers .= "To: $target\r\n";
		$headers .= "Content-type: text/html\r\n";
		mail($target, "New DotA version!", "DotA updates has detected a new DotA version.<br /><br />$latestVersion", $headers);
	}
	
	echo "Sent emails. Now writing to file.\n";
	
	$fh = fopen($filename, 'w');
	fwrite($fh, $latestVersion . "\n");
} else {
	echo "Still up-to-date..\n";
}

?>
