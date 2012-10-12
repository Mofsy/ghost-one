<?php

# status will check the status of your servers and notify you of any problems
# it checks:
#  * whether your bots recently disconnected from battle.net
#  * whether your bots are running at all
#  * whether your bots are hosting
#  * whether your web servers are responsive
# the bot will be identified based on the name of the log file
#  ex: ghost_blah.log will be identified as "ghost_blah"

# it is recommended to set this up as a cronjob every ten minutes:
#    */10 * * * * cd /path/to/status/ && php status.php > /dev/null

# BEGIN CONFIGURATION

# smtp configuration
$smtp_host = "smtp.example.com";
$smtp_port = 25;
$smtp_username = "statusbot";
$smtp_password = "status";

# email configuration
$from_address = "statusbot@example.com";
$to_address = "status-list@example.com";

# web configuration (include trailing slash)
$web_path = '/var/www/status/';

# ghost configuration
# this is array of directories where log files may be found
# trailing slash is optional
# note: it is recommended to keep all files in one ghost directory
#  and use different configuration files to maintain different bots
$paths = array("/path/to/ghost-battlenet/", "/path/to/ghost-pvpgn/");

# web pages to check whether are up
# each page must have the status/test.txt file
#  the contents of this should be one character, '1'
# for example, for "target.com" below, we will ping <http://target.com/status/test.txt>
$targets = array('example.com', 's1.example.com', 's2.example.com');

# whether we should email status results using SMTP and mail configuration above
$doEmail = true;

# whether we should update the status website
$doWeb = true;

# END CONFIGURATION

$knownErrors = array(); //stores errors from file
$errors = array(); //stores errors we find
$botStatus = array(); //stores status array (status integer, error messages array) for each bot

# read error data from file

if(file_exists("status.txt")) {
	$handle = fopen("status.txt", "r");
	
	if($handle) {
		while(($buffer = fgets($handle, 4096)) !== false) {
			$knownErrors[] = trim($buffer);
		}

		fclose($handle);
	}
}

# get hostname
$hostname = exec("hostname");

# loop based on log files

foreach($paths as $path) {
	if(file_exists($path)) {
		$it = new DirectoryIterator($path);
		
		foreach($it as $file) {
			if($it->isFile() && !$it->isDot() && substr($file, -4) == ".log") {
				$botname = substr($file, 0, -4);
				$thisStatus = 0; //0 is neutral (maybe not in lobby)
				$thisError = array(); //stores error messages for just this bot
				$filename = $path . '/' . $file;
				
				$output_array = array();
				$lastline = exec("tail -n 1000 " . escapeshellarg($filename), $output_array);
				
				# scan output_array for interesting things
				foreach($output_array as $line) {
					# check if we disconnected from battle.net
					if(strpos($line, 'disconnected from battle.net') !== false) {
						$posBegin = strpos($line, 'BNET: ');
						
						if($posBegin !== false) {
							$realmBegin = $posBegin + 6;
							$posEnd = strpos($line, ']', $posBegin);
							
							if($posEnd !== false) {
								$realm = substr($line, $realmBegin, $posEnd - $realmBegin);
								
								if($realm != "GCloud") {
									$error = "$botname has disconnected from bnet/$realm";
								
									//need to ensure this error wasn't added
									// because this could occur multiple times
									if(!in_array($error, $errors)) {
										$errors[] = $error;
										$thisError[] = $error;
									}
								}
							}
						}
					}
					
					# check if user has joined game recently
					if(strpos($line, 'joined the game') !== false) {
						$thisStatus = 1;
					}
				}
				
				# check last line to see if the bot is still running
				$posBegin = strpos($lastline, '[');
				
				if($posBegin !== false) {
					$posEnd = strpos($lastline, ']', $posBegin);
					
					if($posEnd !== false) {
						$strTime = substr($lastline, $posBegin + 1, $posEnd - $posBegin - 1);
						$time = strtotime($strTime);
						
						if(time() - $time > 1200) {
							$error = "$botname does not appear to be running";
							$thisStatus = -1;
							
							$errors[] = $error;
							$thisError[] = $error;
						}
					}
				}
				
				$botStatus[$botname] = array($thisStatus, $thisError);
			}
		}
	}
}

# write bot status to web
$handle = fopen($web_path . 'index.html', 'w') or die('failed to write status');
fwrite($handle, '<html><body><table><tr><th>Bot</th><th>Status</th><th>Error</th></tr>');

foreach($botStatus as $botname => $statusArray) {
	$statusString = '<font color="orange">Unknown</font>';
	
	if($statusArray[0] == 0) {
		$statusString = '<font color="orange">Up, no game</font>';
	} else if($statusArray[0] == 1) {
		$statusString = '<font color="green">Good</font>';
	} else if($statusArray[0] == -1) {
		$statusString = '<font color="red">Down</font>';
	}
	
	$errorString = '<font color="green">None</font>';
	
	if(count($statusArray[1]) > 0) {
		$errorString = '<font color="red">' . implode('; ', $statusArray[1]) . '</font>';
	}
	
	fwrite($handle, '<tr><td>' . $botname . '</td><td>' . $statusString . '</td><td>' . $errorString . '</td></tr>');
}

fwrite($handle, '</table><p>Generated at ' . gmdate(DATE_RSS) . '.</p></body></html>');
fclose($handle);

# make sure pages are working

foreach($targets as $target) {
	$targetHandle = fopen('http://' . $target . '/status/test.txt', 'r');
	
	$line = '0';
	
	if($targetHandle) {
		$line = fgets($targetHandle);
		fclose($targetHandle);
	}
	
	if(trim($line) != '1') {
		$errors[] = 'failed to retrieve ' . $target . ' test page; down?';
	}
}

# write errors to file
$handle = fopen('status.txt', 'w') or die('failed to write to status text file');

foreach($errors as $error) {
	fwrite($handle, $error . "\n");
}

fclose($handle);

# see which errors are new, if any
$newErrors = array();

foreach($errors as $error) {
	if(!in_array($error, $knownErrors)) {
		$newErrors[] = $error;
	}
}

# report new errors via email
if(count($newErrors) > 0) {
	$message = "The status script on $hostname reports error(s)!\n\n";
	
	foreach($newErrors as $error) {
		$message .= "* $error\n";
	}
	
	require_once "Mail.php";

	$host = $smtp_host;
	$port = $smtp_port;
	$username = $smtp_username;
	$password = $smtp_password;
	$headers = array ('From' => $from_address,
					  'To' => $to_address,
					  'Subject' => "Alert from $hostname",
					  'Content-Type' => 'text/plain');
	$smtp = Mail::factory('smtp',
						  array ('host' => $host,
								 'port' => $port,
								 'auth' => true,
								 'username' => $username,
								 'password' => $password));
	
	$mail = $smtp->send($to_address, $headers, $message);

	if (PEAR::isError($mail)) {
		echo "Error while mailing " . $mail->getMessage() . " :(\n";
	}
}

?>
