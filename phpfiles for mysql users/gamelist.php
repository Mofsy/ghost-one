<html>
<body>
<h1>Games</h1>

<?php
mysql_connect("localhost", "root", "password");
mysql_select_db("ghost");

$result = mysql_query("SELECT * FROM gamelist");

while($row = mysql_fetch_array($result)) {
    if($row['gamename'] != "") {
        echo $row['gamename'] . " (" . $row['slotstaken'] . "/" . $row['slotstotal'] . ")<br>";
    }
}
?>

</body>
</html>