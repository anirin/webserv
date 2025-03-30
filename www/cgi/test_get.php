<?php
// Simple GET request test

// Get request method
echo "<h1>GET Request Test</h1>";
echo "<p>Request Method: " . ($_SERVER['REQUEST_METHOD'] ?? 'Unknown') . "</p>";

// Get and display query parameters
$query_string = $_SERVER['QUERY_STRING'] ?? '';
echo "<p>Query String: " . htmlspecialchars($query_string) . "</p>";

// Parse and display parameters
echo "<h2>Query Parameters:</h2>";
if (!empty($query_string)) {
    parse_str($query_string, $params);
    echo "<ul>";
    foreach ($params as $key => $value) {
        echo "<li>" . htmlspecialchars($key) . ": " . htmlspecialchars($value) . "</li>";
    }
    echo "</ul>";
} else {
    echo "<p>No parameters found. Try adding ?name=value to the URL.</p>";
}
?>