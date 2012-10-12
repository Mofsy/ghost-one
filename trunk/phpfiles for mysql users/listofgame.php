<html>
<meta http-equiv="refresh" content="9">
<body>
<?php
mysql_connect("127.0.0.1:3306", "root", "password");
mysql_select_db("ghost");
$result = mysql_query("SELECT * FROM gamelist");
while($row = mysql_fetch_array($result))
{
	if(!empty($row['gamename']))
	{
		echo "<br>There are " . $row['totalplayers'] . " players in " . $row['totalgames'] . " games.";
        echo "<br>" . $row['id'] . ". " . $row['gamename'] . " (" . $row['slotstaken'] . "/" . $row['slotstotal'] . ") [TempOwner]: " . $row['ownername'] . " [Creator]: " . $row['creatorname'] . "<br>";
		echo "Players in Lobby:";

		$array = explode("\t", $row['usernames']);

		echo '<table cellspacing="0" cellpadding="2" border="1">';

		for($i = 0; $i < count($array) - 2; $i += 3)
		{
			$username = $array[$i];
			$ping = $array[$i + 2];

			if(!empty($username))
				echo '<tr><td>' , $username , '</td><td>' , $ping , '</td></tr>';
		}
		echo '</table>';
	}
}
?>
</body>
</html>