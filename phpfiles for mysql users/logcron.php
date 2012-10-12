<?php

# logcron is a basic alternative to log_rotate that is much easier to setup
# it will simply move your .log files to .log_
# this way, old log files that are no longer important are overwritten

# for advanced log rotation, we recommend logrotate (man logrotate)

# it is recommended to set this up as a cronjob every day
# this means that logs over a day old may be deleted, and logs two days old are sure to be deleted
#   0 0 * * * cd /path/to/logcron/ && php logcron.php > /dev/null

# BEGIN CONFIGURATION

# array of directories to scan for log files
$paths = array("/path/to/ghost-battlenet", "/path/to/ghost-pvpgn");

# END CONFIGURATION

foreach($paths as $path) {
	if(file_exists($path)) {
		$it = new DirectoryIterator($path);
		
		foreach($it as $file) {
			if($it->isFile() && !$it->isDot() && substr($file, -4) == ".log") {
				rename($path . '/' . $file, $path . '/' . $file . "_");
			}
		}
	}
}

?>
