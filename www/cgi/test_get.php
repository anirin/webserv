<?php
// Enable error reporting (for debugging)
error_reporting(E_ALL);
ini_set('display_errors', 1);

// Retrieve CGI environment variables
$requestMethod = getenv('REQUEST_METHOD');
$queryString = getenv('QUERY_STRING');
$scriptName = getenv('SCRIPT_NAME');

header("Content-Type: text/html; charset=UTF-8");

// Function to parse the query string
function parseQueryString($queryString) {
    $params = [];
    if (!empty($queryString)) {
        // Split the query string by '&'
        $pairs = explode('&', $queryString);
        foreach ($pairs as $pair) {
            // Split each pair by '=' (limit to 2 parts)
            $keyValue = explode('=', $pair, 2);
            if (count($keyValue) === 2) {
                $key = urldecode($keyValue[0]);
                $value = urldecode($keyValue[1]);
                $params[$key] = $value;
            }
        }
    }
    return $params;
}

// Get the query parameters
$queryParams = parseQueryString($queryString);

?>
<html>
<head>
    <title>GET Request Test</title>
</head>
<body>
    <h1>GET Request Processing</h1>
    
    <h2>Environment Variables:</h2>
    <pre>
REQUEST_METHOD: <?php echo $requestMethod; ?>

QUERY_STRING: <?php echo $queryString; ?>

SCRIPT_NAME: <?php echo $scriptName; ?>
    </pre>
    
    <h2>Raw Query String:</h2>
    <pre><?php echo htmlspecialchars($queryString); ?></pre>
    
    <h2>Query Parameters:</h2>
    <pre>
<?php
if (!empty($queryParams)) {
    foreach ($queryParams as $key => $value) {
        echo htmlspecialchars("$key: $value") . "\n";
    }
} else {
    echo "No query parameters available.";
}
?>
    </pre>
    
    <p>Processing Complete</p>
</body>
</html>