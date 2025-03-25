<?php
error_log("CGI script started");

echo "Content-type: text/html\r\n\r\n";

echo "<html><body>";
echo "<h1>Hello from PHP CGI</h1>";
echo "<p>Current time: " . date("Y-m-d H:i:s") . "</p>";
echo "</body></html>";

error_log("CGI script completed");
?>