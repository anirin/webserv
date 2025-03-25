<?php
error_log("GET test script started");

echo "Content-type: text/html\r\n\r\n";

echo "<html><body>";
echo "<h1>GET Request Test</h1>";

if (empty($_GET)) {
    echo "<p>No GET parameters received.</p>";
    echo "<p>Try accessing this script with parameters: <code>?name=John&age=25</code></p>";
} else {
    echo "<h2>Received Parameters:</h2>";
    echo "<ul>";
    foreach ($_GET as $key => $value) {
        echo "<li><strong>" . htmlspecialchars($key) . ":</strong> " . htmlspecialchars($value) . "</li>";
    }
    echo "</ul>";
}

echo "<p>Current time: " . date("Y-m-d H:i:s") . "</p>";
echo "</body></html>";

error_log("GET test script completed");
?>