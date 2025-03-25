<?php
error_log("POST test script started");

// Force output buffering off
ini_set('output_buffering', 0);

// Explicitly send headers
echo "Content-type: text/html\r\n\r\n";
// Force flush
flush();

echo "<html><body>";
echo "<h1>POST Request Test</h1>";
flush();

$raw_input = file_get_contents('php://input');
error_log("Raw input: " . $raw_input);

echo "<h2>Request Details:</h2>";
echo "<p><strong>Request Method:</strong> " . $_SERVER['REQUEST_METHOD'] . "</p>";
flush();

if (!empty($_POST)) {
    echo "<h2>Form Data:</h2>";
    echo "<ul>";
    foreach ($_POST as $key => $value) {
        echo "<li><strong>" . htmlspecialchars($key) . ":</strong> " . htmlspecialchars($value) . "</li>";
    }
    echo "</ul>";
} else {
    echo "<p>No form data received.</p>";
}

if (!empty($raw_input)) {
    echo "<h2>Raw Input:</h2>";
    echo "<pre>" . htmlspecialchars($raw_input) . "</pre>";
    
    $json_data = json_decode($raw_input, true);
    if ($json_data !== null) {
        echo "<h2>Parsed JSON:</h2>";
        echo "<ul>";
        foreach ($json_data as $key => $value) {
            echo "<li><strong>" . htmlspecialchars($key) . ":</strong> " . 
                 (is_string($value) ? htmlspecialchars($value) : print_r($value, true)) . 
                 "</li>";
        }
        echo "</ul>";
    }
}

echo "<p>Current time: " . date("Y-m-d H:i:s") . "</p>";
echo "</body></html>";

// Final flush
flush();
error_log("POST test script completed");
?>